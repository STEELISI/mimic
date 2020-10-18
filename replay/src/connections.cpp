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

#include "connections.h"
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>

// Convert address string into sockaddr_in
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

// Compare two addresses
bool cmpSockAddrIn(const sockaddr_in* a, const sockaddr_in* b) {
  
    if(a->sin_family == b->sin_family) {
        if(ntohl(a->sin_addr.s_addr) == ntohl(b->sin_addr.s_addr)) {
            if(a->sin_port == b->sin_port) {
                return true;
            }
        }
    }
    return false;       
}

// Convert sockaddr_in into string
std::string getIPPortString(const struct sockaddr_in* sa) {
  
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sa->sin_addr), str, INET_ADDRSTRLEN);
    int port = ntohs(sa->sin_port);
    
    std::ostringstream stringStream;
    stringStream.clear();
    
    stringStream << str << ":" << port;
    return stringStream.str();
}

// Get full connection string - 4 tuple               
std::string getConnString(const struct sockaddr_in* src, const struct sockaddr_in* dst, bool* success) {
  
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

    return stringStream.str();
}

// Set non-blocking attribute on a socket
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

// Create a socket and bind to address/port
int getIPv4TCPSock(const struct sockaddr_in * sa) {

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

