/*
#
# Copyright (C) 2020 University of Southern California.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License,
# version 3, as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The details of the GNU General Public License v3 can be found at
# https://choosealicense.com/licenses/gpl-3.0/
#
*/

/* 
   Takes input trace as pcap and outputs 
   comma separated data about connections and events 
   on stdout.  
*/


#include <stdio.h>
#include <iostream>
#include <string.h>
#include <fstream>
#include <assert.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <limits.h>
#include <libtrace.h>
#include <signal.h>

#include <map>
#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

#define JUMBO_MAX 9000

long int flows = 0, sflows = 0, cflows = 0;

// Connection states
enum states{OPEN, HALFCLOSED, CLOSED, TBD};

// Shift CLOSE events by this much
const double THRESH = 0.00001;

// Shift start of conns by this much so
// we can start servers before clients
const double SHIFT = 10;

// Start a server this many second
// before a client tries to connect to it
const double SRV_SHIFT = 6;

// How long to wait to start a new conn w same fid
// This is to avoid starting new conns when there is
// a RST and a peer keeps sending data after it
const int TIME_WAIT = 2;

// Counter for flows
int conn_id_counter = 0;

// If we are rewriting IPs then we have to
// rewrite ports too. Start from this port number
// and cycle through.
std::unordered_map <std::string, int> nextport;
std::unordered_map <std::string, std::unordered_set <int>> portsInUse;
std::unordered_map <int, int> portsToChange;
std::unordered_set <int> reservedPorts;


int checkUsed(std::string address, int port)
{
  if (portsInUse.find(address) == portsInUse.end())
    {
      std::unordered_set <int> myset;
      portsInUse[address] = myset;
      return port;
    }
  if (portsInUse[address].find(port) == portsInUse[address].end() && port != 0 && portsToChange.find(port) == portsToChange.end() && reservedPorts.find(port) != reservedPorts.end())
    {
      portsInUse[address].insert(port);
      return port;
    }

  if (nextport.find(address) == nextport.end())
      nextport[address] = 1024;
    
  if (nextport[address] > 65535)
    nextport[address] = 1024;

  int curport = nextport[address];
  while (portsInUse[address].find(nextport[address]) != portsInUse[address].end() || portsToChange.find(nextport[address]) != portsToChange.end() || reservedPorts.find(nextport[address]) != reservedPorts.end())
    {
      nextport[address]++;
      // Nothing was free at the moment
      if (nextport[address] > 65535)
	nextport[address] = 1024;
      if (curport == nextport[address])
	{
	  std::cout<<"Ran out of ports for "<<address<<std::endl;
	  return 0;
	}
    }
  portsInUse[address].insert(nextport[address]);
  //std::cout<<" Inserted "<<address<<" port "<<nextport[address]<<std::endl;
    
  return nextport[address];
}


// Global vars and constants
double old_ts = 0;
double start_ts = 0;
bool orig = true;
std::string client, server;
double gap;


// When a flow is inactive, close it after this
// time. It will be started again if traffic shows up.
int DELTHRESH = 60;

// Collect samples for RTT calculation.
// Structures below help us store the samples.
// This is useful for network emulation; it is currently
// unused.
double bucketlimits[] = {0, 0.001, 0.01, 0.1, 1, 10};

class avgpair
{
public:
  double limit;
  int count;
  double sum;
};

class bucket
{
public:
  avgpair pairs[6];
  double Mbytes;
  int conns;
  
  ~bucket()
  {
  }
};


bool lessthanpair(avgpair a,  avgpair b)
{
  return a.count < b.count;
}

std::map <uint32_t, bucket> host_stats;

FILE *hosts;

void hostinsert(uint32_t ip)
{
  int i;
  for (i=0;i<=5;i++)
    {
      host_stats[ip].pairs[i].limit = bucketlimits[i];
      host_stats[ip].pairs[i].count = 0;
      host_stats[ip].pairs[i].sum = 0;
    }		    
  host_stats[ip].Mbytes = 0;
  host_stats[ip].conns = 0;
}

// Check if a is less than b, assuming they are both seq or ack numbers so they can wrap around
// and assuming that a came before b on the same flow
 bool lessthan(uint32_t a, uint32_t b)
 {   
   if (a < b)
     return true;
   if (a > b && a > UINT_MAX/2 && b < UINT_MAX/2 && ((long int) b - a < JUMBO_MAX))
       return true;
   return false;
 }

// Check if a is less than or equal to b, assuming they are both seq or ack numbers so they can wrap around
// and assuming that a came before b on the same flow
 bool lessoreq(uint32_t a, uint32_t b)
 {   
   if (a <= b)
     return true;
   if (a > b && a > UINT_MAX/2 && b < UINT_MAX/2 && ((long int) b - a < JUMBO_MAX))
     return true;
   return false;
 }

// Calculate diff between two seq or ack numbers but take into account
// possible wraparound
uint32_t difference(uint32_t a, uint32_t b)
{
  long int diff = (long int) b - a;
  if (diff < 0 && lessthan(a, b))
    diff += UINT_MAX;
  return (uint32_t) diff;
}

// Event structure helps us identify ADUs
class event
{
public:
  double time;
  uint32_t src_ip;
  uint16_t src_port;
  uint32_t dst_ip;
  uint16_t dst_port;
  char type[5];
  int bytes;
  double think_time;
  uint32_t seq;
  uint32_t ack;
  
  ~event()
  {
  }
};

// This class helps us identify duplicates
class packet
{

public:
  uint32_t sseq;
  uint32_t eseq;
  int16_t id;
  int acked;
  double ts;

  packet()
  {
    ts = -1;
  }

  ~packet()
  {
  }
  
  packet(uint32_t s, uint32_t e, int16_t i, int a, double t)
  {
    sseq = s;
    eseq = e;
    id = i;
    acked = a;
    ts = t;
  }
};

// Structure that stores flow identifier
class flow_id
{
public:
  uint32_t srcIP;
  uint32_t dstIP;
  uint16_t sport;
  uint16_t dport;
  

  flow_id(uint32_t si, uint32_t di, uint16_t sp, uint32_t dp) 
  {
    srcIP = si;
    dstIP = di;
    sport = sp;
    dport = dp;
  }

  const bool operator==(const flow_id& g) const
  {
    return (srcIP == g.srcIP && dstIP == g.dstIP && sport == g.sport && dport == g.dport);
  }

  const bool operator<(const flow_id& g) const
  {
    return (srcIP < g.srcIP || (srcIP == g.srcIP && dstIP < g.dstIP) ||
	    (srcIP == g.srcIP && dstIP == g.dstIP && sport < g.sport) || 
	    (srcIP == g.srcIP && dstIP == g.dstIP && sport == g.sport && dport < g.dport));
  }

};

std::ostream& operator<<(std::ostream& os, const flow_id& fid)
{
  std::string src_str = inet_ntoa(*(struct in_addr *)&(fid.srcIP));
  std::string dst_str = inet_ntoa(*(struct in_addr *)&(fid.dstIP));
  os <<" host "<< src_str << " and port " <<fid.sport<<" and host "<<dst_str<<" and port "<<fid.dport;
  return os;
}

class bunch
{

public:
  bunch()
  {
    bytes = 0;
    ts = 0;
    src = "";
  }

  std::string src;
  double ts;
  long int bytes;
};



// Structure that stores state of a flow
class flow_stats
{
public:
  std::map <std::string, double> last_event_ts;
  double start_ts;
  long int event_id;
  uint32_t src_seq, dst_seq, src_ack, dst_ack, src_lastack, dst_lastack, src_lastseq, dst_lastseq;
  long int src_toack, dst_toack, src_sent, dst_sent, src_waited, dst_waited;
  double src_ack_ts, dst_ack_ts, last_ts;
  enum states state;
  long int conn_id;
  std::string src_str;
  std::string dst_str;
  std::string conn_str;
  bunch stored;

  
  flow_stats()
  {
    event_id = 0;
    src_seq = dst_seq = src_ack = dst_ack = src_lastack = dst_lastack = src_lastseq = dst_lastseq = 0;
    src_sent = dst_sent = src_toack = dst_toack = src_waited = dst_waited = 0;
    src_toack = dst_toack = 0;
    src_ack_ts = dst_ack_ts = 0;
    conn_str = "";
    state = OPEN;
    last_ts = 0;
  }

  ~flow_stats()
  {
    last_event_ts.clear();
  }

  flow_stats& operator=(const flow_stats& f)
  {
    if (this != &f)
      {
	src_str = f.src_str;
	dst_str = f.dst_str;
	stored = f.stored;
	event_id = f.event_id;
	src_seq = f.src_seq;
	dst_seq = f.dst_seq;
	src_ack = f.src_ack;	
	src_lastack = f.src_lastack;
	src_lastseq = f.src_lastseq;
	src_toack = f.src_toack;
	src_waited = f.src_waited;
	dst_ack = f.dst_ack;
	dst_lastack = f.dst_lastack;
	dst_lastseq = f.dst_lastseq;
	dst_toack = f.dst_toack;
	dst_waited = f.dst_waited;
	src_ack_ts = f.src_ack_ts;
	dst_ack_ts = f.dst_ack_ts;
	conn_id = f.conn_id;
	conn_str = f.conn_str;
      }
    return *this;
  }

};

// Main structure that stores information about flows
std::map <flow_id, flow_stats> flowmap;

// Blocked flows
std::map <flow_id, double> blocklist;


// This function updates flow stats (e.g., current seq number)
// and returns 1 if the packet is a duplicate, 0 otherwise
/*
int checkDuplicate(flow_id fid, int dir, uint32_t src, uint32_t dst,
		   uint32_t sseq, uint32_t eseq, uint32_t ack, int16_t id,
		   double ts, int syn, int fin, int psh)
{
  std::map <uint32_t, packet> *seqs;
  std::map <uint32_t, packet> *acks;
  std::map <uint32_t, packet> *racks;
  std::map <uint32_t, int> *IDs;
  int duplicate = 0;
  uint32_t host;
  if (dir == 0)
    {
      seqs = &flowmap[fid].src_seqs;
      acks = &flowmap[fid].src_acks;
      racks = &flowmap[fid].dst_acks;
      IDs = &flowmap[fid].src_IDs;
      host = src;
    }
  else
    {
      seqs = &flowmap[fid].dst_seqs;
      acks = &flowmap[fid].dst_acks;
      racks = &flowmap[fid].src_acks;
      IDs = &flowmap[fid].dst_IDs;
      host = dst;
    }
  // Hardware repetition, ignore
  if ((*IDs).find(id) != (*IDs).end())
    {
      return 2;
    }
  else
    {
      // Found duplicate sequence number
      std::map <uint32_t, packet>::iterator it = (*seqs).find(sseq);
      if (it != (*seqs).end())
	{
	  if (eseq == it->second.eseq && (syn || fin || psh))
	    {
	      it->second.ts = ts;
	      duplicate = 1;
	    }
	}
    }
  // Not a duplicate and is a packet that could
  // later be duplicated - remember it
  if (!duplicate && (syn || fin || psh))
    {
      packet P(sseq, eseq, id, 0, ts);
      (*seqs)[sseq] = P;
      (*acks)[eseq] = P;
    }

  std::map <uint32_t, packet>::iterator it1 = (*racks).find(ack);
  std::map <uint32_t, packet>::iterator it2 = (*racks).find(ack-1);
  std::map <uint32_t, packet>::iterator it;
  int found = 0;

  // Is this a new ack for an existing, yet
  // unacked packet?
  if (it1 != (*racks).end())
    {
      it = it1;
      found = 1;
    }
  if (it2 != (*racks).end())
    {
      it = it2;
      found = 2;
    }
  if (found)
    {
      packet *p = &it->second;
      if (!p->acked)
	{
	  // First ack of this packet, calculate RTT
	  double RTTsample = ts - p->ts;
	  std::map <uint32_t, bucket>::iterator hit;
	  int i;
	  
	  hit = host_stats.find(host);
	  p->acked = 1;

	  // Store RTT sample
	  if (hit == host_stats.end())
	    hostinsert(host);
	  
	  for (i=0; i<5; i++)
	    {
	      if (RTTsample < bucketlimits[i])
		break;
	    }
	  host_stats[host].pairs[i].count++;
	  host_stats[host].pairs[i].sum += RTTsample;
	}    
    }
  return duplicate;
}
*/

int checkDuplicate(flow_id fid, int dir, uint32_t src, uint32_t dst,
		   uint32_t sseq, uint32_t eseq, uint32_t ack, int16_t id,
		   double ts, int syn, int fin, int psh)
{
  //std::cout<<"Check duplicate "<<flowmap[fid].src_lastseq<<" against "<<eseq<<std::endl;
  if (src == fid.srcIP && lessthan(flowmap[fid].src_lastseq, eseq))
    return 0;
  if (src == fid.dstIP && lessthan(flowmap[fid].dst_lastseq, eseq))
    return 0;
  return 1;
}

// Start the flow, rewrite ports if needed
bool startFlow(flow_id fid, double ts, std::string src_str, std::string dst_str, uint32_t seq,
	       uint32_t ack, int payload_size, bool orig)
{
  flow_stats FS;
  flows++;
  sflows++;
  uint16_t src_port = fid.sport;
  uint16_t dst_port = fid.dport;
  flowmap[fid] = FS;
  flowmap[fid].conn_id = conn_id_counter++;
  flowmap[fid].start_ts = ts;
  flowmap[fid].src_str = src_str;
  flowmap[fid].dst_str = dst_str;
  flowmap[fid].src_seq = flowmap[fid].src_lastseq = seq-1;
  flowmap[fid].dst_lastseq = ack;
  flowmap[fid].src_ack = flowmap[fid].src_lastack = ack;
  flowmap[fid].src_lastack = ack;
  long int diff = (long int) seq - payload_size;
  if (diff >= 0)
    flowmap[fid].dst_lastack = diff;
  else
    flowmap[fid].dst_lastack = UINT_MAX + diff;
    
  if (src_port >= dst_port)
    {
      if (!orig)
	{
	  src_port = checkUsed(src_str, src_port);
	  if (src_port == 0)
	    {
	      std::cout<<"Ran out of ports for fid "<<fid<<" for source "<<fid.srcIP<<"\n";
	      exit(1);
	    }
	}
      flowmap[fid].conn_str = "CONN,"+std::to_string(flowmap[fid].conn_id)+","+src_str+
	","+std::to_string(src_port)+",->,"+dst_str+","+std::to_string(dst_port)+","
	+std::to_string(ts-start_ts+SRV_SHIFT);
    }
  else
    {
      if (!orig)
	{
	  dst_port = checkUsed(src_str, dst_port);
	  if (dst_port == 0)
	    {
	      std::cout<<"Ran out of ports for fid "<<fid<<" for source "<<fid.dstIP<<"\n";
	      exit(1);
	    }
	}
      flowmap[fid].conn_str = "CONN,"+std::to_string(flowmap[fid].conn_id)+","+
	dst_str+","+std::to_string(dst_port)+",->,"+src_str+","+std::to_string(src_port)+","
	+std::to_string(ts-start_ts+SRV_SHIFT);
    }
  return true;
}

void handleState(flow_id fid, libtrace_tcp_t * tcp)
{
  if (tcp->fin)
    if (flowmap[fid].state == OPEN)
      flowmap[fid].state = HALFCLOSED;
    else
      flowmap[fid].state = CLOSED;
  else if(tcp->ack && flowmap[fid].state == CLOSED)
    flowmap[fid].state = TBD;
}

// Close the flow, generate waits for any outstanding data, free the client port
void closeFlow(flow_id fid)
{
  //std::cout<<"Closing flow\n";
  // Reverse ID
  flow_id rid(fid.dstIP, fid.srcIP, fid.dport,fid.sport);

  // If the flow had any data exchange then print out the closing WAIT and CLOSE statements
  if (flowmap[fid].event_id > 0 || flowmap[fid].stored.bytes > 0)
    {
      // print stored SEND if any
      if (flowmap[fid].stored.bytes > 0)
	{
	  if (flowmap[fid].event_id == 0)
	    std::cout<<flowmap[fid].conn_str<<std::endl;
	  double diff = 0;
	  if (flowmap[fid].last_event_ts.find(flowmap[fid].stored.src) != flowmap[fid].last_event_ts.end() && flowmap[fid].last_event_ts[flowmap[fid].stored.src] > 0)
	    diff =  flowmap[fid].stored.ts - flowmap[fid].last_event_ts[flowmap[fid].stored.src];
	  std::cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].stored.src
		   <<",SEND,"<<flowmap[fid].stored.bytes<<","<<diff<<"0,"<<flowmap[fid].stored.ts-start_ts+SHIFT<<std::endl;
	}
      // Print WAIT and CLOSE events
      if (flowmap[fid].src_toack > 0)
	std::cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].src_str<<",WAIT,"<<flowmap[fid].src_toack<<",0,"<<flowmap[fid].last_ts-start_ts+SHIFT<<std::endl;
      std::cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].src_str<<",CLOSE,0,0.0,"<<flowmap[fid].last_ts-start_ts+SHIFT+THRESH<<std::endl;
      if (flowmap[fid].dst_toack > 0)
	std::cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].dst_str<<",WAIT,"<<flowmap[fid].dst_toack<<",0.0,"<<flowmap[fid].last_ts-start_ts+SHIFT<<std::endl;
      std::cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].dst_str<<",CLOSE,0,0.0,"<<flowmap[fid].last_ts-start_ts+SHIFT+THRESH<<std::endl;
    }

  //std::cout<<flowmap[fid].last_ts-start_ts<<" fid "<<fid<<" erased ports "<<fid.sport<<" and "<<fid.dport<<" conn "<<flowmap[fid].conn_id<<std::endl;

  // Free up ports
  portsInUse[flowmap[fid].src_str].erase(fid.sport);
  //std::cout<<"Erased "<<fid.srcIP<<" port "<<fid.sport<<std::endl;
  portsInUse[flowmap[fid].dst_str].erase(fid.dport);
  //std::cout<<"Erased "<<fid.dstIP<<" port "<<fid.dport<<std::endl;

  flowmap.erase(fid);
  flowmap.erase(rid);
  flows--;
  cflows++;
  

}


double timenow = 0;

// Call this handler on CTRL-C                                                                                                  
void signal_callback_handler(int signum) {
  for (auto fit = flowmap.begin(); fit != flowmap.end(); fit++)
    {
      std::cout<<"Flow "<<fit->first<<" start "<<fit->second.start_ts<<" last time "<<fit->second.last_ts<<" time now "<<timenow<<" events "<<fit->second.event_id<<std::endl;
    }
  exit(signum);
}

// Every so often go through all the flows
// and close those that are idle
void cleanFlows(double ts, bool force)
{
  //std::cout<<"Flows "<<flows<<" started "<<sflows<<" ended "<<cflows<<std::endl;
  sflows = cflows = 0;
  for (auto fit = flowmap.begin(); fit != flowmap.end();)
    {
      double diff = ts - fit->second.last_ts;
      if (fit->second.state == TBD || ts-fit->second.last_ts > DELTHRESH || force)
	{
	  auto it = fit;
	  fit++;
	  closeFlow(it->first);
	}
      else
	fit++;
    }
  for (auto bit = blocklist.begin(); bit != blocklist.end();)
    {
      if(ts - bit->second >= TIME_WAIT)
	{
	  auto it = bit;
	  bit++;
	  blocklist.erase(it);
	}
      else
	bit++;
    }
}

// Main function that processes each packet
void processPacket(libtrace_packet_t *packet) {
  int payload_size;
  int16_t id;
  
  libtrace_tcp_t *tcp = NULL;
  libtrace_ip_t *ip = NULL;
  double ts;
  
  uint16_t l3_type;
  int src_port, dst_port;

  // Register a signal handler
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  ip = (libtrace_ip_t *)trace_get_layer3(packet, &l3_type, NULL);
  if (l3_type != 0x0800) return;
  if (ip == NULL) return;
  
  tcp = trace_get_tcp(packet);
  ts = trace_get_seconds(packet);

  timenow = ts;
  if (start_ts == 0)
    start_ts = ts;
  if (ts - old_ts > 1)
    {
      cleanFlows(ts, false);
      old_ts = ts;
    }
  id = ip->ip_id;
  
  // Only handle tcp packets 
  if (tcp) {
    
    double last_ack_ts;
    uint32_t src, dst;
    std::string src_str, dst_str;
    src = ip->ip_src.s_addr;
    dst = ip->ip_dst.s_addr;

    // Are we keeping original IPs/ports or rewriting?
    if (orig)
      {
	src_str = inet_ntoa(ip->ip_src);
	dst_str = inet_ntoa(ip->ip_dst);
      }
    else
      {
	if (src < dst)
	  {
	    src_str = client;
	    dst_str = server;
	  }
	else
	  {
	    src_str = server;
	    dst_str = client;
	  }
      }
    src_port = trace_get_source_port(packet);
    dst_port = trace_get_destination_port(packet);
    payload_size = trace_get_payload_length(packet);
    //std::cout.setf(std::ios::fixed);
    //std::cout<<"Time "<<ts<<" flow "<<src_str<<":"<<src_port<<"->"<<dst_str<<":"<<dst_port<<" payload "<<payload_size<<std::endl;
    
    // We will change some ports that are specified in a
    // file ports.csv. These are reserved ports on replay
    // machines, e.g., 22
    if (portsToChange.find(src_port) != portsToChange.end())
      src_port = portsToChange[src_port];
    if (portsToChange.find(dst_port) != portsToChange.end())
      dst_port = portsToChange[dst_port];

    
    flow_id did(src, dst, src_port, dst_port);
    flow_id rid(dst, src, dst_port, src_port);

    // Seq and ack number
    uint32_t oseq = ntohl(tcp->seq);
    uint32_t seq = oseq;
    if ((long int) ntohl(tcp->seq)+payload_size > UINT_MAX)
      seq = (long int) ntohl(tcp->seq)+payload_size - UINT_MAX;
    else
      seq = ntohl(tcp->seq)+payload_size;
    
    uint32_t ack;
    if (ntohl(tcp->ack_seq) != 0)
      ack = ntohl(tcp->ack_seq) - 1;
    else
      ack = UINT_MAX;


    //std::cout.setf(std::ios::fixed);
    //std::cout<<"oseq "<<oseq<<" seq "<<seq<<" ack "<<ack<<std::endl;

    // A new flow
    if (flowmap.find(did)==flowmap.end() && flowmap.find(rid)==flowmap.end())
      {
	// Only start a new flow on payload
	// so we don't remember flows that are only SYNs
	if (payload_size > 0) 
	  {
	    // Still in TIME_WAIT
	    if(blocklist.find(did) != blocklist.end())
	      return;
	    bool started = startFlow(did, ts, src_str, dst_str, seq, ack, payload_size, orig);
	    //std::cout<<ts-start_ts<<" Started flow, sport "<<did.sport<<" conn "<<flowmap[did].conn_id<<std::endl;
	    if (!started)
	      return;
	  }	      
	else
	  return;
      }
    // Find the flow in the map
    flow_id fid(0,0,0,0);
    if (flowmap.find(did) != flowmap.end())
      {
	fid = did;
      }
    else
      {
	fid = rid;
      }
    if (flowmap[fid].dst_lastseq == 0)
      flowmap[fid].dst_lastseq = ack;

    // Close the flow if needed    
    if ((tcp->fin || tcp->rst) && payload_size == 0)
      {
	blocklist[did] = ts;
	blocklist[rid] = ts;
	closeFlow(fid);
	return;
      }
    // New connection with same fid so we close the old one
    if (tcp->syn) 
      {
	closeFlow(fid);
	return;
      }
    
    long int acked = 0;
    
    // Process packet and generate SEND/WAIT records
    int duplicate = 0;
    if (src == fid.srcIP)
      {
	duplicate = checkDuplicate(fid, 0, src, dst, oseq, seq, ack, id, ts,
				   tcp->syn, tcp->fin, payload_size);
	last_ack_ts = flowmap[fid].src_ack_ts;

	//std::cout<<" Duplicate "<<duplicate<<" last ack ts "<<last_ack_ts<<" current ack "<<ack<<std::endl;
	// If this is not a hardware duplicate
	if (duplicate < 2 && lessthan(flowmap[fid].src_lastack, ack))
	  {
	    if (payload_size == 0)
	      {
		acked = difference(flowmap[fid].src_lastack, ack);
		if (acked > flowmap[fid].src_toack)
		  acked = flowmap[fid].src_toack;
		if (lessthan(flowmap[fid].src_lastack, ack))
		  flowmap[fid].src_lastack = ack;
	      }
	    flowmap[fid].src_ack = ack;
	    flowmap[fid].src_ack_ts = ts;
	    handleState(fid, tcp);
	  }
      }
    else
      {
	duplicate = checkDuplicate(fid, 0, dst, src, oseq, seq, ack, id, ts,
				   tcp->syn, tcp->fin, payload_size);
	
	last_ack_ts = flowmap[fid].dst_ack_ts;
	// If this is not a hardware duplicate 
	if (duplicate < 2 && lessthan(flowmap[fid].dst_lastack,ack))
	  {
	    if (payload_size == 0)
	      {
		acked = difference(flowmap[fid].dst_lastack, ack);
		if (acked > flowmap[fid].dst_toack)
		  acked = flowmap[fid].dst_toack;
		if (lessthan(flowmap[fid].dst_lastack, ack))
		  flowmap[fid].dst_lastack = ack;
	      }
	    flowmap[fid].dst_ack = ack;
	    flowmap[fid].dst_ack_ts = ts;
	    handleState(fid, tcp);
	  }
      }
	  
    // Ignore duplicates at transport level 
    // We only care about app level events, if no payload data was transferred, we don't care 
    // But we will generate an event if this was just an ack because that denotes that one party
    // had no data to send 
    if(!duplicate) {		
      // Generate an event
      double wait = ts - last_ack_ts;      
      if (last_ack_ts == 0)
	wait = 0;

      bool isgap = false;
      // Generate SEND event
      if (payload_size != 0)
	{
	  flowmap[fid].last_ts = ts;
	  // Adjust payload if we have to, app-faithful behavior
	  unsigned int *lastseq;
	  if (src == fid.srcIP)
	    lastseq = &flowmap[fid].src_lastseq;
	  else
	    lastseq = &flowmap[fid].dst_lastseq;
	  // Is there a gap?
	  if (lessthan(*lastseq, oseq))
	    {
	      //std::cout<<ts<<" There is gap between "<<seq<<" and "<<*lastseq<<" diff "<<seq-*lastseq<<std::endl;
	      long int diff = difference(*lastseq, oseq);
	      payload_size += diff;
	      isgap = true;
	      if (flowmap[fid].last_event_ts[src_str] > 0)
		ts = flowmap[fid].last_event_ts[src_str];
	    }
	  else
	    isgap = false;
	  *lastseq = seq;
	  // Always store it, but possibly print out what has been stored
	  //std::cout.setf(std::ios::fixed);
	  //std::cout<<ts<<" stored bytes "<<flowmap[fid].stored.bytes<<" src "<<flowmap[fid].stored.src<<" src str"<<src_str<<" ts "<<(double)ts<<" stored ts "<<(double)flowmap[fid].stored.ts<<" diff "<<ts-flowmap[fid].stored.ts<<" gap"<<gap<<std::endl;
	  if (flowmap[fid].stored.bytes > 0 && (flowmap[fid].stored.src != src_str || (flowmap[fid].stored.src == src_str && ts - flowmap[fid].stored.ts >= gap)))
	    {
	      // Could be the first record, so print conn std::string before it
	      if (flowmap[fid].event_id == 0)
		std::cout<<flowmap[fid].conn_str<<std::endl;
	      double diff = 0;
	      if (flowmap[fid].last_event_ts.find(flowmap[fid].stored.src) != flowmap[fid].last_event_ts.end() && flowmap[fid].last_event_ts[flowmap[fid].stored.src] > 0)
		diff =  flowmap[fid].stored.ts - flowmap[fid].last_event_ts[flowmap[fid].stored.src];
	      std::cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].stored.src
		  <<",SEND,"<<flowmap[fid].stored.bytes<<",0,"<<std::fixed<<flowmap[fid].stored.ts-start_ts+SHIFT<<std::endl;
	      flowmap[fid].last_event_ts[flowmap[fid].stored.src] = flowmap[fid].stored.ts;
		
	      flowmap[fid].stored.ts = ts;
	      flowmap[fid].stored.bytes = payload_size;
	      flowmap[fid].stored.src = src_str;
	    }
	  else if (flowmap[fid].stored.bytes == 0)
	    {
	      flowmap[fid].stored.ts = ts;
	      flowmap[fid].stored.bytes = payload_size;
	      flowmap[fid].stored.src = src_str;
	    }
	  else
	    {
	      flowmap[fid].stored.bytes += payload_size;
	    }

	  // Adjust ack numbers
	  if (src == fid.srcIP)
	    {
	      flowmap[fid].dst_toack += payload_size;
	      flowmap[fid].src_sent += payload_size;
	    }
	  else
	    {
	      flowmap[fid].src_toack += payload_size;
	      flowmap[fid].dst_sent += payload_size;
	    }
	}
      else
	{
	  // Generate a WAIT event. We only do so for zero-payload packets
	  // because this tells us that the peer was waiting for some ADU
	  // and could not send data without it.
		      
	  if (!tcp->syn && payload_size == 0 && acked > 0)
	    {
	      flowmap[fid].last_ts = ts;
	      
	      if (acked > 0)
		{
		  // Print stored bytes if any
		  if (flowmap[fid].stored.bytes > 0)
		    {
		      // Could be the first event, so print conn std::string too
		      if (flowmap[fid].event_id == 0)
			std::cout<<flowmap[fid].conn_str<<std::endl;
		      double diff = 0;
		      if (flowmap[fid].last_event_ts.find(flowmap[fid].stored.src) != flowmap[fid].last_event_ts.end() && flowmap[fid].last_event_ts[flowmap[fid].stored.src] > 0)
			diff =  flowmap[fid].stored.ts - flowmap[fid].last_event_ts[flowmap[fid].stored.src];
		      std::cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].stored.src
			       <<",SEND,"<<flowmap[fid].stored.bytes<<","<<diff<<","<<flowmap[fid].stored.ts-start_ts+SHIFT<<" "<<std::endl;
		      // Reset what was stored
		      flowmap[fid].last_event_ts[flowmap[fid].stored.src] = flowmap[fid].stored.ts;
		      flowmap[fid].stored.ts = 0;
		      flowmap[fid].stored.bytes = 0;
		      flowmap[fid].stored.src = "";
		    }
		  // Print out the WAIT event
		  std::cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","
		      <<src_str<<",WAIT,"<<acked<<",0,"<<ts-start_ts+SHIFT<<std::endl;
		  flowmap[fid].last_event_ts[src_str] = ts;
		}
	      // Adjust what has to be acked
	      if (src == fid.srcIP)
		{
		  flowmap[fid].src_toack -= acked;
		  flowmap[fid].src_waited += acked;
		}
	      else
		{
		  flowmap[fid].dst_toack -= acked;
		  flowmap[fid].dst_waited += acked;
		}
	    }
	}
    }
  }
}

// Print help message about program usage
void printHelp(std::string prog)
{
  std::cout<<"\n\tUsage: "<<prog<<" [-c oneIP -s otherIP] [-h] [-a GAP] pcapfile\n\n"

    "\tIn the absence of -c and -s flags, original ports and IPs will be mined.\n"
    "\tOtherwise, IPs will be overwritten with the IPs you have specified\n"
    "\tand duplicate client ports will be replaced by random other client ports.\n"
    "\tThis process is deterministic, so running the code on two different\n"
    "\tmachines will produce identical outputs.\n\n"

    "\tAdditionally, if there are any ports on replay machines that are\n"
    "\treserved (e.g., 22), you can specify them in ports.csv file and they will be\n"
    "\tautomatically replaced.\n\n"

    "\tFlag -h prints the help message.\n\n"

    "\tFlag -a followed by GAP, which is a number in decimal notation, denoting that consecutive\n"
    "\tSEND events by the same IP within time GAP seconds should be aggregated into one\n\n";
}
 
 
// Main program
int main(int argc, char *argv[])
{
  
  libtrace_t *trace;
  libtrace_packet_t *packet;
  
  int i, opt;
   
  while ((opt = getopt(argc, argv, "c:s:a:h")) != -1) {
    switch (opt) {
    case 'c':
      client = optarg;
      orig = false;
      break;
    case 's':
      server = optarg;
      orig = false;
      break;
    case 'a':
      gap = std::stod(optarg);
      break;
    case 'h':
      printHelp(argv[0]);
      exit(0);
    default:
      break;
    }
  }
  if (orig == false && (client == "" || server == ""))
    {
      std::cerr<<"Both client and server IPs must be specified\n";
      exit(0);	    
    }
  
  packet = trace_create_packet();
  if (packet == NULL) {
    perror("Creating libtrace packet");
    return -1;
  }
  
  if (optind >= argc) {
    std::cerr<<"Expected filename to process after options\n";
    exit(0);
  }
  
  
  // Read any ports whose numbers should be changed
  // because they are reserved on machines that will
  // perform replay
  std::ifstream ports;
  ports.open ("ports.csv");
  int a, b;
  
  while(ports >> a >> b)
    {
      portsToChange[a] = b;
      reservedPorts.insert(b);
    }
  
  ports.close();

  // Read from trace
  double ts;
  
  for (i = optind; i < argc; i++) {
    
    trace = trace_create(argv[i]);
    
    if (!trace) {
      perror("Creating libtrace trace");
      return -1;
    }
    
    if (trace_is_err(trace)) {
      trace_perror(trace, "Opening trace file");
      trace_destroy(trace);
      continue;
    }
    
    if (trace_start(trace) == -1) {
      trace_perror(trace, "Starting trace");
      trace_destroy(trace);
      continue;
    }
    
    while (trace_read_packet(trace, packet) > 0) {
      ts = trace_get_seconds(packet);
      processPacket(packet);		       
    }
    
    if (trace_is_err(trace)) {
      trace_perror(trace, "Reading packets");
      trace_destroy(trace);
      continue;
    }
    
    trace_destroy(trace);
    cleanFlows(ts-start_ts, true);
  }
  trace_destroy_packet(packet);
  cleanFlows(ts-start_ts, true);
  
  // Iterate through map and print host RTTs
  hosts = fopen("hosts.csv", "w");
  fprintf(hosts, "HOST,DELAY,DROP,CONNS,MBYTES\n");

  for (auto it = host_stats.begin(); it != host_stats.end(); it++) {
    int i, total, count=0;
    double sum;
    std::vector<avgpair> sortedcounts;
    std::vector<avgpair>::reverse_iterator sit;
    for (i=0; i<6;i++)
      {
	count += it->second.pairs[i].count;
	sortedcounts.push_back(it->second.pairs[i]);
      }
    sort(sortedcounts.begin(), sortedcounts.end(), lessthanpair);
    total = 0;
    sum = 0;
    
    // Go until you reach the 80% of total count 
    for(sit = sortedcounts.rbegin(); sit != sortedcounts.rend(); sit++)
      {
	total += sit->count;
	sum += sit->sum;
	if (total > 0.8*count)
	  break;
      }
    if (total == 0)
      total = 1;
    fprintf(hosts, "%s,%lf,0,%d,%lf\n", inet_ntoa(*(struct in_addr *)&(it->first)), sum/total, it->second.conns, it->second.Mbytes);
  }
  fflush(hosts);
  fclose(hosts);
  return 0;
}

