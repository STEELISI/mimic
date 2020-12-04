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

#ifndef EVENTHANDLER_H
#define EVENTHANDLER_H 

#include <stdlib.h>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <time.h>
#include <string>
#include <sstream>
#include <fcntl.h>
#include <vector>
#include <queue>
#include <map> 
#include <unordered_map>
#include <unordered_set>
#include "eventQueue.h"
#include "eventNotifier.h"
#include "pollHandler.h"
#include "utils.h"

// Data we store about connections
struct connData
{
  std::string serverString= "";
  int sockfd = -1;
  long int waitingToRecv = 0;
  long int waitingToSend = 0;
  long int delay = 0;
  long int samples = 0;
  long int lastPlannedEvent = 0;
  long int origStart = 0;
  long int origTime = 0;
  EventHeap eventQueue;
  long int time = 0;
  enum conn_state state = INIT;
  bool stalled = false;
};

// EventHandler class
class EventHandler {
  
 private:
  
  int myID;
  long int my_bytes;
  long int my_events;
  long int fileEventsHandledCount;       
  long int lastEventCountWhenRequestingForMore;

  // Helps us watch for socket events via epoll
  PollHandler* myPollHandler;

  // Coordination with fileWorker
  EventNotifier* requestMoreFileEvents;
  EventQueue* incomingFileEvents;

  // Internal events we handle
  EventHeap* eventsToHandle;
  
  // For logging 
  std::ofstream* out;
  
  // Data management structures. 
  stringToConnIDMap strToConnID;
  std::map<long int, connData> myConns;
  std::map<int, long int> sockfdToConnIDMap;
  std::map<std::string, long int> serverToSockfd;
  std::map<std::string, long int> serverToCounter;
  std::map<std::string, long int>* listenerTime;
  std::map<std::string, long int> srvStarted;
  std::map<std::string, long int> orphanConn;
  std::map<long int, struct stats>* connStats;
  std::map<std::string,std::list<long int>> pendingConns;

  // Synchronization with other peer
  long int myTime, peerTime;
  
  void processFileEvents();
  void addWait();
  void dispatch(Event dispatchJob, long int now);
  void newConnectionUpdate(int sockfd, long int connID, long int planned, long int now);
  void connectionUpdate(long int connID, long int planned, long int now);
  long int acceptNewConnection(struct epoll_event *poll_e, long int now);
  long int getNewEvents(long int conn_id);
  void checkStalledConns(long int now);
  void checkOrphanConns(long int now);
  void checkQueues();
  bool DEBUG = 0;
	
 public:
  EventHandler(EventNotifier* loadMoreNotifier, EventQueue* fe,
	       std::map<long int, struct stats>* cs, int id, bool debug, std::string myname);
  ~EventHandler();
  bool startup();
  void loop(std::chrono::high_resolution_clock::time_point startTime);
};
	

#endif
