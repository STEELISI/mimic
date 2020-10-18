#include "connections.h"
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>


struct sockaddr_in getAddressFromString(std::string addrString)
{
  char c[MEDLEN];
  strcpy(c,addrString.c_str());
  int i;
  for(i=0; i<strlen(c); i++)
    {
      if (c[i] == ':')
	{
	  c[i] = 0;
	  break;
	}
    }
  struct sockaddr_in saddr;
  saddr.sin_family=AF_INET;
  saddr.sin_port = htons(atoi(c+i+1));
  inet_aton(c, &saddr.sin_addr);
  bzero(saddr.sin_zero, 8);
  return saddr;
}


bool cmpSockAddrIn(const sockaddr_in* a, const sockaddr_in* b) {
    //if (memcmp(a, b, sizeof(struct sockaddr_in)) == 0) return true;        
    if(a->sin_family == b->sin_family) {
        if(ntohl(a->sin_addr.s_addr) == ntohl(b->sin_addr.s_addr)) {
            if(a->sin_port == b->sin_port) {
                return true;
            }
        }
    }
    return false;       
}

std::string getIPPortString(const struct sockaddr_in* sa) {
    /* XXX Should use this in getConnString */
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sa->sin_addr), str, INET_ADDRSTRLEN);
    int port = ntohs(sa->sin_port);
    
    std::ostringstream stringStream;
    stringStream.clear();
    
    stringStream << str << ":" << port;
    return stringStream.str();
}

               
std::string getConnString(const struct sockaddr_in* src, const struct sockaddr_in* dst, bool* success) {
    /* XXX should set success based off of result. */
    *success = true;

    char srcStr[INET_ADDRSTRLEN];
    char dstStr[INET_ADDRSTRLEN];
    
    inet_ntop(AF_INET, &(src->sin_addr), srcStr, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(dst->sin_addr), dstStr, INET_ADDRSTRLEN);
    std::ostringstream stringStream;
    stringStream.clear();

    int sport= 0,  dport  = 0;

    if (src->sin_family == AF_INET) {
        sport = ntohs(src->sin_port);
        dport = ntohs(dst->sin_port);
    }

    stringStream << srcStr << ":" << sport << "," << dstStr << ":" << dport;

    //if (DEBUG)
    //cout << "String stream: " << stringStream.str() << endl;

    return stringStream.str();
    return "";
}

int setIPv4TCPNonBlocking(int sockfd) {
    int status = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
    if(status == -1) {
      char errmsg[200];
      sprintf(errmsg, "Had trouble getting non-blocking socket for %d\n", sockfd);
      perror(errmsg);
      return(-1);
    }
    return status;
}

int getIPv4TCPSock(const struct sockaddr_in * sa) {
    /* Get non-blocking socket. */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1)
      {
	perror("Ran out of sockets\n");
      }
    setIPv4TCPNonBlocking(s);

    if(s == -1) 
        perror("Set sockopt failed.");

    /* If we were given an address, bind to it. */
    if(sa != NULL) {
      int optval = 1;
      setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

      int b = bind(s, (const struct sockaddr *)sa, sizeof(struct sockaddr_in));
      if(b <0) {
	char msg[100];
	sprintf(msg, "bind failed %d\n", ntohs(sa->sin_port));
	perror(msg);
	return(-1);
      }
    }

    return s;
}

#define MS 30

void getAddrFromString(std::string servString, struct sockaddr_in* addr)
{
  char s[MS];
  strcpy(s, servString.c_str());
  int i;
  for(i=0; i<strlen(s); i++)
    {
      if (s[i] == ':')
	{
	  s[i] = 0;
	  break;
	}
    }
  addr->sin_family = AF_INET;
  addr->sin_port = htons(atoi(s+i+1));
  inet_aton(s, &addr->sin_addr);
}
