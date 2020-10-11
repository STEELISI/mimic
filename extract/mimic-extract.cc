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


using namespace std;

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

#include <map>
#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

// Maximum bytes to send in a pkt
// We use this to ad-hoc detect new conns
// on encapsulated IPv6
const int MAX_GAP = 1000000;

// How long to wait to start a new conn w same fid
// This is to avoid starting new conns when there is
// a RST and a peer keeps sending data after it
const int TIME_WAIT = 2;

// Counter for flows
int conn_id_counter = 0;

// If we are rewriting IPs then we have to
// rewrite ports too. Start from this port number
// and cycle through.
int nextport=1024;
unordered_set<int> portsInUse;

int checkUsed(int port)
{
  if (portsInUse.find(port) == portsInUse.end())
    {
      portsInUse.insert(port);
      return port;
    }
  if (nextport > 65535)
    nextport = 1024;
  
  while (portsInUse.find(nextport) != portsInUse.end())
    {
      nextport++;
    }
  // Nothing was free at the moment
  if (nextport > 65535)
    return -1;
  portsInUse.insert(nextport);
  return nextport;
}


// Global vars and constants
double old_ts = 0;
double start_ts = 0;
bool orig = true;
string client, server;
double gap;
unordered_map<int, int> portsToChange;


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


bool lessthan(avgpair a,  avgpair b)
{
  return a.count < b.count;
}

map<uint32_t, bucket> host_stats;

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

class bunch
{

public:
  bunch()
  {
    bytes = 0;
    ts = 0;
    src = "";
  }

  string src;
  double ts;
  long int bytes;
};



// Structure that stores state of a flow
class flow_stats
{
public:
  map <uint32_t, packet> src_seqs;
  map <uint32_t, packet> dst_seqs;
  map <uint32_t, packet> src_acks;
  map <uint32_t, packet> dst_acks;
  map <uint32_t, int> src_IDs;
  map <uint32_t, int> dst_IDs;
  map <int, event> flow_events;
  long int event_id;
  uint32_t src_seq, dst_seq, src_ack, dst_ack, src_lastack, dst_lastack, src_lastseq, dst_lastseq;
  long int src_toack, dst_toack, src_sent, dst_sent, src_waited, dst_waited;
  double src_ack_ts, dst_ack_ts, last_ts;
  enum states state;
  long int conn_id;
  string src_str;
  string dst_str;
  string conn_str;
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
    conn_id = conn_id_counter++;
  }

  ~flow_stats()
  {
    src_seqs.clear();
    src_acks.clear();
    dst_seqs.clear();
    dst_acks.clear();

    src_IDs.clear();
    dst_IDs.clear();

    flow_events.clear();
  }

  flow_stats& operator=(const flow_stats& f)
  {
    if (this != &f)
      {
	src_str = f.src_str;
	dst_str = f.dst_str;
	src_IDs = f.src_IDs;
	dst_IDs = f.dst_IDs;
	src_seqs = f.src_seqs;
	dst_seqs = f.dst_seqs;
	src_acks = f.src_acks;
	dst_acks = f.dst_acks;
	stored = f.stored;
	flow_events = f.flow_events;
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
map <flow_id, flow_stats> flowmap;

// Blocked flows
map <flow_id, double> blocklist;


// This function updates flow stats (e.g., current seq number)
// and returns 1 if the packet is a duplicate, 0 otherwise
int checkDuplicate(flow_id fid, int dir, uint32_t src, uint32_t dst,
		   uint32_t sseq, uint32_t eseq, uint32_t ack, int16_t id,
		   double ts, int syn, int fin, int psh)
{
  map <uint32_t, packet> *seqs;
  map <uint32_t, packet> *acks;
  map <uint32_t, packet> *racks;
  map <uint32_t, int> *IDs;
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
      map <uint32_t, packet>::iterator it = (*seqs).find(sseq);
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

  map <uint32_t, packet>::iterator it1 = (*racks).find(ack);
  map <uint32_t, packet>::iterator it2 = (*racks).find(ack-1);
  map <uint32_t, packet>::iterator it;
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
	  map<uint32_t, bucket>::iterator hit;
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

// Start the flow, rewrite ports if needed
bool startFlow(flow_id fid, double ts, string src_str, string dst_str, uint32_t seq,
	       uint32_t ack, int payload_size, bool orig)
{
  flow_stats FS;
  uint16_t src_port = fid.sport;
  uint16_t dst_port = fid.dport;
  flowmap[fid] = FS;
  flowmap[fid].src_str = src_str;
  flowmap[fid].dst_str = dst_str;
  flowmap[fid].src_seq = flowmap[fid].src_lastseq = seq-payload_size;
  flowmap[fid].dst_lastseq = ack;
  flowmap[fid].src_ack = flowmap[fid].src_lastack = ack;
  if (src_port >= dst_port)
    {
      if (!orig)
	{
	  src_port = checkUsed(src_port);
	  if (src_port == -1)
	    return false;
	}
      flowmap[fid].conn_str = "CONN,"+to_string(flowmap[fid].conn_id)+","+src_str+
	","+to_string(src_port)+",->,"+dst_str+","+to_string(dst_port)+","
	+to_string(ts-start_ts+SRV_SHIFT);
    }
  else
    {
      if (!orig)
	{
	  dst_port = checkUsed(dst_port);
	  if (dst_port == -1)
	    return false;
	}
      flowmap[fid].conn_str = "CONN,"+to_string(flowmap[fid].conn_id)+","+
	dst_str+","+to_string(dst_port)+",->,"+src_str+","+to_string(src_port)+","
	+to_string(ts-start_ts+SRV_SHIFT);
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
  // Reverse ID
  flow_id rid(fid.dstIP, fid.srcIP, fid.dport,fid.sport);

  // If the flow had any data exchange then print out the closing WAIT and CLOSE statements
  if (flowmap[fid].event_id > 0 || flowmap[fid].stored.bytes > 0)
    {
      // print stored SEND if any
      if (flowmap[fid].stored.bytes > 0)
	{
	  if (flowmap[fid].event_id == 0)
	    cout<<flowmap[fid].conn_str<<endl;
	  cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].stored.src
	      <<",SEND,"<<flowmap[fid].stored.bytes<<",0,"<<flowmap[fid].stored.ts-start_ts+SHIFT<<endl;
	}
      // Print WAIT and CLOSE events
      if (flowmap[fid].src_toack > 0)
	cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].src_str<<",WAIT,"<<flowmap[fid].src_toack<<",0,"<<flowmap[fid].last_ts-start_ts+SHIFT<<endl;
      cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].src_str<<",CLOSE,0,0.0,"<<flowmap[fid].last_ts-start_ts+SHIFT+THRESH<<endl;
      if (flowmap[fid].dst_toack > 0)
	cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].dst_str<<",WAIT,"<<flowmap[fid].dst_toack<<",0.0,"<<flowmap[fid].last_ts-start_ts+SHIFT<<endl;
      cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].dst_str<<",CLOSE,0,0.0,"<<flowmap[fid].last_ts-start_ts+SHIFT+THRESH<<endl;
    }
  flowmap.erase(fid);
  flowmap.erase(rid);

  // Free up ports
  portsInUse.erase(fid.sport);
  portsInUse.erase(fid.dport);			      
}

// Every so often go through all the flows
// and close those that are idle
void cleanFlows(double ts, bool force)
{
  map<flow_id, flow_stats>::iterator fit;
  int i = 0;

  for (fit = flowmap.begin(); fit != flowmap.end();)
    {
      if (fit->second.state == TBD || ts-fit->second.last_ts > DELTHRESH || force)
	{
	  map<flow_id, flow_stats>::iterator it = fit;
	  fit++;
	  closeFlow(it->first);
	}
      else
	fit++;
    }
  map<flow_id, double>::iterator bit;
  for (bit = blocklist.begin(); bit != blocklist.end();)
    {
      if(ts - bit->second >= TIME_WAIT)
	{
	  map<flow_id, double>::iterator it = bit;
	  bit++;
	  blocklist.erase(it);
	}
      else
	bit++;
    }
}

// Main function that processes each packet
void processPacket(libtrace_packet_t *packet) {

  uint8_t dir, proto;
  int payload_size;
  int16_t id;
  
  libtrace_tcp_t *tcp = NULL;
  libtrace_ip_t *ip = NULL;
  double ts;
  
  uint16_t l3_type;
  uint32_t remaining;
  int src_port, dst_port;
  
  ip = (libtrace_ip_t *)trace_get_layer3(packet, &l3_type, NULL);
  if (l3_type != 0x0800) return;
  if (ip == NULL) return;
  
  tcp = trace_get_tcp(packet);
  ts = trace_get_seconds(packet);
  if (start_ts == 0)
    start_ts = ts;
  if (ts - old_ts > 1)
    {
      cleanFlows(ts-start_ts, false);
      old_ts = ts;
    }
  id = ip->ip_id;
  
  // Only handle tcp packets 
  if (tcp) {
    
    double last_ack_ts;
    uint32_t src, dst;
    string src_str, dst_str;
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

    // We will change some ports that are specified in a
    // file ports.csv. These are reserved ports on replay
    // machines, e.g., 22
    if (portsToChange.find(src_port) != portsToChange.end())
      src_port = portsToChange[src_port];
    if (portsToChange.find(dst_port) != portsToChange.end())
      dst_port = portsToChange[dst_port];
    payload_size = trace_get_payload_length(packet);
    
    flow_id did(src, dst, src_port, dst_port);
    flow_id rid(dst, src, dst_port, src_port);
    map<flow_id, flow_stats>::iterator fit;

    // Seq and ack number
    uint32_t seq = ntohl(tcp->seq)+payload_size;
    uint32_t ack = ntohl(tcp->ack_seq);

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
    int duplicate;
    if (src == fid.srcIP)
      {
	duplicate = checkDuplicate(fid, 0, src, dst, seq-payload_size, seq, ack, id, ts,
				   tcp->syn, tcp->fin, payload_size);
	last_ack_ts = flowmap[fid].src_ack_ts;
	
	// If this is not a hardware duplicate 
	if (duplicate < 2 && ack > flowmap[fid].src_ack)
	  {
	    if (payload_size == 0)
	      flowmap[fid].src_lastack = ack;
	    flowmap[fid].src_ack = ack;
	    flowmap[fid].src_ack_ts = ts;
	    handleState(fid, tcp);
	  }
      }
    else
      {
	
	last_ack_ts = flowmap[fid].dst_ack_ts;
	
	// If this is not a hardware duplicate 
	if (duplicate < 2 && ack > flowmap[fid].dst_ack)
	  {
	    if (payload_size == 0)
	      flowmap[fid].dst_lastack = ack;
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

      // Generate SEND event
      if (payload_size != 0)
	{
	  flowmap[fid].last_ts = ts;
	  // Always store it, but possibly print out what has been stored
	  if (flowmap[fid].stored.bytes > 0 && (flowmap[fid].stored.src != src_str || (flowmap[fid].stored.src == src_str && ts - flowmap[fid].stored.ts >= gap)))
	    {
	      // Could be the first record, so print conn string before it
	      if (flowmap[fid].event_id == 0)
		cout<<flowmap[fid].conn_str<<endl;
	      cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].stored.src
		  <<",SEND,"<<flowmap[fid].stored.bytes<<",0,"<<std::fixed<<flowmap[fid].stored.ts-start_ts+SHIFT<<endl;
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
	    flowmap[fid].stored.bytes += payload_size;

	  // Adjust ack numbers
	  if (src == fid.srcIP)
	    {
	      flowmap[fid].src_lastseq = seq;
	      flowmap[fid].dst_toack += payload_size;
	      flowmap[fid].src_sent += payload_size;
	    }
	  else
	    {
	      flowmap[fid].dst_lastseq = seq;
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
		      // Could be the first event, so print conn string too
		      if (flowmap[fid].event_id == 0)
			cout<<flowmap[fid].conn_str<<endl;
		      cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","<<flowmap[fid].stored.src
			  <<",SEND,"<<flowmap[fid].stored.bytes<<",0,"<<flowmap[fid].stored.ts-start_ts+SHIFT<<endl;
		      // Reset what was stored
		      flowmap[fid].stored.ts = 0;
		      flowmap[fid].stored.bytes = 0;
		      flowmap[fid].stored.src = "";
		    }
		  // Print out the WAIT event
		  cout<<"EVENT,"<<flowmap[fid].conn_id<<","<<flowmap[fid].event_id++<<","
		      <<src_str<<",WAIT,"<<acked<<","<<wait<<","<<ts-start_ts+SHIFT<<endl;
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
void printHelp(string prog)
{
  cout<<"\n\tUsage: "<<prog<<" [-c oneIP -s otherIP] [-h] [-a GAP] pcapfile\n\n"

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
    "\tSEND events by the same IP within time GAP should be aggregated into one\n\n";
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
      gap = stod(optarg);
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
      cerr<<"Both client and server IPs must be specified\n";
      exit(0);	    
    }
  
  packet = trace_create_packet();
  if (packet == NULL) {
    perror("Creating libtrace packet");
    return -1;
  }
  
  if (optind >= argc) {
    cerr<<"Expected filename to process after options\n";
    exit(0);
  }
  
  
  // Read any ports whose numbers should be changed
  // because they are reserved on machines that will
  // perform replay
  ifstream ports;
  ports.open ("ports.csv");
  int a, b;
  
  while(ports >> a >> b)
    portsToChange[a] = b;
  
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
  map<uint32_t, bucket>::iterator it;
  for (it = host_stats.begin(); it != host_stats.end(); it++) {
    int i, total, count=0;
    double sum;
    vector<avgpair> sortedcounts;
    vector<avgpair>::reverse_iterator sit;
    for (i=0; i<6;i++)
      {
	count += it->second.pairs[i].count;
	sortedcounts.push_back(it->second.pairs[i]);
      }
    sort(sortedcounts.begin(), sortedcounts.end(), lessthan);
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

