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

/* Various constants and variables for synchronization */

#ifndef UTILS_H
#define UTILS_H

#include <ctime>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <string>
#include <condition_variable>
#include "connections.h"

// Number of simultaneous connections per port 
#define MAX_BACKLOG_PER_SRV 1000


// Std::Mutexes and condition variables.
extern std::mutex fileHandlerMTX;
extern std::mutex statsMTX;
extern long int global_throughput;
extern long int global_events;
extern std::condition_variable fileHandlerCV;
extern bool loadMoreFileEvents;
bool returnLoadMoreFileEvents();

// How many lines to read from file at once
#define maxQueuedFileEvents 1000000

// How much earlier to open ports
#define SRV_UPSTART 4000
#define START_TIME 10000

// String lengths
#define SHORTLEN 150
#define MEDLEN 255

// Sync variables
extern std::atomic<bool> isRunning;
extern std::atomic<bool> isInitd;
extern std::atomic<int> numThreads;
extern std::atomic<int> numconns;
extern std::atomic<int> numbytes;
extern std::atomic<int> numevents;
extern std::atomic<bool> isServer;
extern std::atomic<bool> makeup;

// Event types and human-readable version
static std::string EventNames[] = {"ACCEPT", "CONNECT", "CLOSE", "RECV","WAIT", "SEND", "SRV_START", "SRV_STARTED", "SRV_END", "NONE"};

enum EventType {
                ACCEPT, 	
                CONNECT,	
                CLOSE,		
                RECV, 		
                WAIT,		
                SEND, 		
                SRV_START,	
                SRV_STARTED, 	
                SRV_END, 	
                NONE		
                };

// Types of sockets
enum TransportType {
                TCP,
                UDP
                };
            
enum DomainType {
    IPV4,
    IPV6
};

  

class Event {
    public:
        std::string serverString = "";
	std::string connString = "";
        int sockfd = -1;
        long int ms_from_start = 1;	 /* Keep track of event time relative to start in ms. */
	long int origTime = 0;
        long int ms_from_last_event = -1;
        EventType type = NONE;        
        long int conn_id = -1;
        long int event_id = -1;
        long int value = 0;		/* What this value holds depends on what type of event this is. */
	Event();
	Event(std::string ss, std::string cs, int fd, long int mfs, long int mfle, EventType t, long int cid, long int eid, long int v);
};


long int msSinceStart(std::chrono::high_resolution_clock::time_point startTime);

#endif



