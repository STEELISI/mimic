/* Takes input trace as PCAP and outputs 
   comma separated data about connections and events  */

#define __STDC_FORMAT_MACROS

using namespace std;

#include <stdio.h>
#include <string.h>
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

enum states{OPEN, HALFCLOSED, CLOSED, TBD};

const double THRESH = 0.00001;
const double SHIFT = 10;
const double SRV_SHIFT = 6;

uint64_t flow_counter = 0;

int conn_id_counter = 0;

double bucketlimits[] = {0, 0.001, 0.01, 0.1, 1, 10};
double old_ts = 0;
double start_ts = 0;
int DELTHRESH = 60;
int CBUF = 200;
int BUF = 4000000;
int MSS = 1500;
int MAXMSS = 15000;

#define STRLEN 50

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

FILE *events, *hosts;

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

class printable_event
{
public:
  double time;
  uint32_t src_ip;
  char type[5];
  int bytes;
  double think_time;
  printable_event()
  {
  }
  printable_event(double t, uint32_t ip, const char* ty, int b, int tt)
  {
    time = t;
    src_ip = ip;
    strcpy(type, ty);
    bytes = b;
    think_time = tt;
    
  }
};

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

template <typename M> void FreeClear(M & amap) {
  for (typename M::iterator it = amap.begin(); it != amap.end(); ++it) {
    delete it->second;
  }
amap.clear();
}

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
  uint32_t src_seq, dst_seq, src_ack, dst_ack, src_lastack, dst_lastack;
  long int src_toack, dst_toack;
  double src_ack_ts, dst_ack_ts, last_ts;
  enum states state;
  long int conn_id;

  flow_stats()
  {
    event_id = 0;
    src_seq = dst_seq = src_ack = dst_ack = 0;
    src_toack = dst_toack = 0;
    src_ack_ts = dst_ack_ts = 0;
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
	src_IDs = f.src_IDs;
	dst_IDs = f.dst_IDs;
	src_seqs = f.src_seqs;
	dst_seqs = f.dst_seqs;
	src_acks = f.src_acks;
	dst_acks = f.dst_acks;
	flow_events = f.flow_events;
	event_id = f.event_id;
	src_seq = f.src_seq;
	dst_seq = f.dst_seq;
	src_ack = f.src_ack;
	dst_ack = f.dst_ack;
	src_ack_ts = f.src_ack_ts;
	dst_ack_ts = f.dst_ack_ts;
	conn_id = f.conn_id;
      }
    return *this;
  }

};

map <flow_id, flow_stats> flowmap;


int process_packet(flow_id fid, int dir, uint32_t src, uint32_t dst, uint32_t sseq, uint32_t eseq, uint32_t ack, int16_t id, double ts, int syn, int fin, int psh)
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
	  double RTTsample = ts - p->ts;
	  map<uint32_t, bucket>::iterator hit;
	  int i;
	  
	  hit = host_stats.find(host);
	  p->acked = 1;

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


/* A very crude test for wrap-around */
int follows(uint32_t base, uint32_t seq)
{
  return (seq > base || (seq < base && base > INT_MAX/2));
}


void close_flow(flow_id fid)
{
  flow_id rid(fid.dstIP, fid.srcIP, fid.dport,fid.sport);

  char str[STRLEN];
  strcpy(str, inet_ntoa(*(struct in_addr *)&(fid.srcIP)));
  if (flowmap[fid].src_toack > 1)
      printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, str, "WAIT", flowmap[fid].src_toack, 0.0, flowmap[fid].last_ts+SHIFT);
  printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, str, "CLOSE", 0, 0.0, flowmap[fid].last_ts+SHIFT+THRESH);
  strcpy(str, inet_ntoa(*(struct in_addr *)&(fid.dstIP)));
  if (flowmap[fid].dst_toack > 1)
      printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, str, "WAIT", flowmap[fid].dst_toack, 0.0, flowmap[fid].last_ts+SHIFT);
  printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, str, "CLOSE", 0, 0.0, flowmap[fid].last_ts+SHIFT+THRESH);
  flowmap.erase(fid);
  flowmap.erase(rid);
}

void cleanflows(double ts, bool force)
{
  map<flow_id, flow_stats>::iterator fit;
  int i = 0;

  for (fit = flowmap.begin(); fit != flowmap.end();)
    {
      if (fit->second.state == TBD || ts-fit->second.last_ts > DELTHRESH || force)
	{
	  map<flow_id, flow_stats>::iterator it = fit;
	  fit++;
	  close_flow(it->first);
	}
      else
	fit++;
    }
}

int rounds = 0;

void per_packet(libtrace_packet_t *packet) {

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
	    cleanflows(ts-start_ts, false);
	    old_ts = ts;
	  }
	id = ip->ip_id;

	/* Only handle tcp packets */
        if (tcp) {
	        
	  double last_ack_ts;
	  uint32_t src, dst;
	  char src_str[STRLEN], dst_str[STRLEN];
	  src = ip->ip_src.s_addr;
	  dst = ip->ip_dst.s_addr;
	  strcpy(src_str, inet_ntoa(ip->ip_src));
	  strcpy(dst_str, inet_ntoa(ip->ip_dst));
	  src_port = trace_get_source_port(packet);
	  dst_port = trace_get_destination_port(packet);
	  payload_size = trace_get_payload_length(packet);

	  flow_id did(src, dst, src_port, dst_port);
	  flow_id rid(dst, src, dst_port, src_port);
	  map<flow_id, flow_stats>::iterator fit;

	  /* Seq and ack number */
	  uint32_t seq = ntohl(tcp->seq)+payload_size;
	  uint32_t ack = ntohl(tcp->ack_seq);

	  if (flowmap.find(did)==flowmap.end() && flowmap.find(rid)==flowmap.end())
	    {
	      if (payload_size > 0) // || tcp->syn)
		{
		  flow_stats FS;
		  flowmap[did] = FS;
		  event E = {ts-start_ts, src, src_port,  dst, dst_port, "CONN", payload_size, 0, seq-payload_size, ack};
		  flowmap[did].src_seq = seq-payload_size;
		  flowmap[did].src_ack = flowmap[did].src_lastack = ack;
		  printf("CONN,%d,%s,%d,->,%s,%d,%f\n", flowmap[did].conn_id, src_str, src_port, dst_str, dst_port, (ts-start_ts+SRV_SHIFT));
		  //printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[did].conn_id, flowmap[did].event_id++, src_str, "SEND", payload_size, 0, ts-start_ts+SHIFT);
		  //flowmap[did].dst_toack += payload_size;
		}	      
	      else
		return;
	    }
	  flow_id fid(0,0,0,0);
	  if (flowmap.find(did) != flowmap.end())
	      fid = did;
	  else
	      fid = rid;

	  long int acked = 0;
	  if ((tcp->fin || tcp->rst) && payload_size == 0)
	    {
	      close_flow(fid);
	      return;
	    }
	  /* Insert hosts into map if not there */
	  /* Calculate RTTs from the vantage point */
	  int duplicate;
	  if (src == fid.srcIP)
	    {
	      duplicate = process_packet(fid, 0, src, dst, seq-payload_size, seq, ack, id, ts, tcp->syn, tcp->fin, payload_size);
	      last_ack_ts = flowmap[fid].src_ack_ts;

	      /* If this is not a hardware duplicate */
	      if (duplicate < 2 && ack > flowmap[fid].src_ack)
		{
		  if (flowmap[fid].src_lastack > 0)
		    {
		      /* One-way traffic, need to generate send first */
		      if(flowmap[fid].src_toack < ack - flowmap[fid].src_ack)
			{
			  long int diff = ack - flowmap[fid].src_ack - flowmap[fid].src_toack;
			  if (diff > 1)
			    {
			      printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, dst_str, "SEND", diff, 0, ts-start_ts+SHIFT);
			      flowmap[fid].src_toack += diff;
			    }
			}
		      acked = ack - flowmap[fid].src_lastack;
		      // Jelena
		      if (flowmap[fid].src_toack < acked)
			{
			  long int diff = acked - flowmap[fid].src_toack;
			  if (diff > 1)
			    {
			      printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, dst_str, "SEND", diff, 0, ts-start_ts+SHIFT);
			      flowmap[fid].src_toack += diff;
			    }
			}
		    }
		  if (payload_size == 0)
		    flowmap[fid].src_lastack = ack;
		  flowmap[fid].src_ack = ack;
		  flowmap[fid].src_ack_ts = ts;
		  if (tcp->fin)
		    if (flowmap[fid].state == OPEN)
		      {
			flowmap[fid].state = HALFCLOSED;
		      }
		    else
		      {
			flowmap[fid].state = CLOSED;
		      }
		  else if(tcp->ack && flowmap[fid].state == CLOSED)
		    {
		      flowmap[fid].state = TBD;
		    }
		}
	    }
	  else
	    {
	      duplicate = process_packet(fid, 1, dst, src, seq-payload_size, seq, ack, id, ts, tcp->syn, tcp->fin, payload_size);

	      last_ack_ts = flowmap[fid].dst_ack_ts;

	      /* If this is not a hardware duplicate */
	      if (duplicate < 2 && ack > flowmap[fid].dst_ack)
		{
		  if (flowmap[fid].dst_lastack > 0)
		    {
		      /* One-way traffic, need to generate send first */
		      if(flowmap[fid].dst_toack < ack - flowmap[fid].dst_ack)
			{
			  long int diff = ack - flowmap[fid].dst_ack - flowmap[fid].dst_toack;
			  if (diff > 1)
			    {
			      printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, src_str, "SEND", diff, 0, ts-start_ts+SHIFT);
			      flowmap[fid].dst_toack += diff;
			    }
			}		      
		      acked = ack - flowmap[fid].dst_lastack;
		      // Jelena
		      if (flowmap[fid].dst_toack < acked)
			{
			  long int diff = acked - flowmap[fid].dst_toack;
			  if (diff > 1)
			    {
			      printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, src_str, "SEND", diff, 0, ts-start_ts+SHIFT);
			      flowmap[fid].dst_toack += diff;
			    }
			}
		    }
		  if (payload_size == 0)
		    flowmap[fid].dst_lastack = ack;
		  flowmap[fid].dst_ack = ack;
		  flowmap[fid].dst_ack_ts = ts;
		  if (tcp->fin)
		    if (flowmap[fid].state == OPEN)
		      {
			flowmap[fid].state = HALFCLOSED;
		      }
		    else
		      {
			flowmap[fid].state = CLOSED;
		      }
		  else if(tcp->ack && flowmap[fid].state == CLOSED)
		    {
		      flowmap[fid].state = TBD;
		    }
		}
	    }
	  
	  /* Ignore duplicates at transport level */ 
	  /* We only care about app level events, if no payload data was transferred, we don't care */
	  /* But we will generate an event if this was just an ack because that denotes that one party
	     had no data to send */
	  if(!duplicate) {		
	    /* Generate an event */
	    double wait = ts - last_ack_ts;
	    if (last_ack_ts == 0)
	      wait = 0;
	    
	    if (payload_size != 0)
	      {
		event E = {ts-start_ts, src, src_port,  dst, dst_port, "SEND", payload_size, wait, seq, ack};
		flowmap[fid].last_ts = ts-start_ts;
		printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, src_str, "SEND", payload_size, wait, ts-start_ts+SHIFT);
		if (src == fid.srcIP)
		  flowmap[fid].dst_toack += payload_size;
		else
		  flowmap[fid].src_toack += payload_size;
	      }
	    else
	      {
		if (!tcp->syn && payload_size == 0 && acked > 0)
		  {
		    flowmap[fid].last_ts = ts-start_ts;
		    event E = {ts-start_ts, src, src_port,  dst, dst_port, "ACK", payload_size, wait, seq, ack};
		    if (acked > 1)
		      printf("EVENT,%d,%d,%s,%s,%d,%lf,%lf\n", flowmap[fid].conn_id, flowmap[fid].event_id++, src_str, "WAIT", acked, wait, ts-start_ts+SHIFT);
		    if (src == fid.srcIP)
		      flowmap[fid].src_toack -= acked;
		    else
		      flowmap[fid].dst_toack -= acked;
		  }
	      }
	  }
	}
}


int main(int argc, char *argv[]) {

        libtrace_t *trace;
        libtrace_packet_t *packet;

        bool opt_true = true;
        bool opt_false = false;

        int i;

	/* Open output files */
	events = fopen("events.csv", "w");
	
	fprintf(events, "X,CONNID,SEQ_NUM,ACTOR,STATE,VALUE,TIME,ATIME\n");

        packet = trace_create_packet();
        if (packet == NULL) {
                perror("Creating libtrace packet");
                return -1;
        }

        optind = 1;
	double ts;
		
        for (i = optind; i < argc; i++) {

		/* Bog-standard libtrace stuff for reading trace files */
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
			per_packet(packet);		       
                }

                if (trace_is_err(trace)) {
                        trace_perror(trace, "Reading packets");
                        trace_destroy(trace);
                        continue;
                }

                trace_destroy(trace);
		cleanflows(ts-start_ts, true);
        }
	trace_destroy_packet(packet);
	cleanflows(ts-start_ts, true);

	/* Iterate through map and print host delays */
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
	  /* Go until you reach the 80% of total count */
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

