#ifndef CONNECTIONS_H
#define CONNECTIONS_H
#include "mimic.h"

enum conn_state {INIT, LISTENING, CONNECTING, EST, DONE};

struct stats
{
  enum conn_state state = INIT;
  long int started = 0;
  long int completed = 0;
  int total_events = 0;
  int last_completed = -1;
  int thread = -1;
  int delay = 0;
};

typedef std::unordered_map<std::string, long int> stringToConnIDMap;

std::string getConnString(const struct sockaddr_in* src, const struct sockaddr_in* dst, bool* success);
std::string getIPPortString(const struct sockaddr_in* sa);

void getAddrFromString(std::string servString, struct sockaddr_in* addr);
bool cmpSockAddrIn(const sockaddr_in* a, const sockaddr_in* b);
                            
struct connectionPair {
  struct sockaddr_in src; 
  struct sockaddr_in dst; 
  connectionPair(std::string srcIP, int sport, std::string dstIP, int dport);    
  bool operator==(const connectionPair a) const;
  
};         

struct sockaddr_in getAddressFromString(std::string addrString);

typedef std::unordered_map<long int, std::shared_ptr<connectionPair>> ConnectionPairMap;

// Some short socket helpers.
int setIPv4TCPNonBlocking(int sockfd);
int getIPv4TCPSock(const struct sockaddr_in * sa);



#endif



