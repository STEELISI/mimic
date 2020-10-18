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

/* This class implements connection states and collection of
   various statistics */

#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include "utils.h"
#include <list>


// Each connection goes through these states
// INIT - LISTENING - EST - DONE or
// INIT - CONNECTING - EST - DONE
enum conn_state {INIT, LISTENING, CONNECTING, EST, DONE};

// We remeber these statistics per connection
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

// We assume each flow ID (4-tuple) maps to a unique connection ID. This isn't true
// since we can have multiple connections with same flowID. This is why we also remember
// pending connections for a given listening port.
typedef std::unordered_map<std::string, long int> stringToConnIDMap;
std::string getConnString(const struct sockaddr_in* src, const struct sockaddr_in* dst, bool* success);
std::string getIPPortString(const struct sockaddr_in* sa);
void getAddrFromString(std::string servString, struct sockaddr_in* addr);
bool cmpSockAddrIn(const sockaddr_in* a, const sockaddr_in* b);
                            

struct sockaddr_in getAddressFromString(std::string addrString);

// Some short socket helpers.
int setIPv4TCPNonBlocking(int sockfd);
int getIPv4TCPSock(const struct sockaddr_in * sa);



#endif



