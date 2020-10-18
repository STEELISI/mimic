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


#ifndef FILEWORKER_H
#define FILEWORKER_H 

#include <stdlib.h>
#include <iostream>
#include <thread>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <string.h>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include "connections.h"
#include "eventNotifier.h"
#include "pollHandler.h"
#include "eventQueue.h"
#include "utils.h"


class FileWorker {
 private:
  // Notifier so that EventHandler can request more events be loaded.
  EventNotifier* loadEventsNotifier;
  PollHandler* loadEventsPollHandler;
    
  // Internal data
  long int fileEventsAddedCount;
  long int my_conn_id;
  long int my_eid;
  long int my_time;
  long int my_runningtime;
  long int my_events;
  
  unsigned short int my_sport;
  unsigned short int my_cport;
  unsigned long int startTime;

  int rounds = 0;
  bool isDone = false;
  bool isProcessed = false;

  // For logging 
  std::ofstream* out;

  // Event queue to be sent to eventHandler
  EventQueue** outEvents;
  std::vector <std::vector <std::string>> loadFile(std::istream* infile, int numFields=3, int numRecords=-1);
  std::vector <std::vector <std::string>> loadMMapFile(void * mmapData, int numFields, int numRecords);
  
  bool isMyIP(std::string IP);
  bool isMyConnID(long int connID);
  void loadEvents(int howmany, int rounds);
  int findMin();
  void getFields(std::string bufPart, std::vector <std::string>* record, int numFields);
  
	
  std::unordered_set<std::string> myIPs;
  std::unordered_set<std::string> foreignIPs;
  std::unordered_set<long int> myConnIDs;
  std::unordered_map<long int, long int> connIDToLastEventTimeMap;
  std::unordered_map<std::string, bool> srvStringToStartBoolMap;
  std::unordered_map<long int, int> connIDToThread;
  std::unordered_map<long int, std::string> connIDToServString;
  std::unordered_map<std::string, std::unordered_set <int>> servStringToThread;
  std::unordered_map<long int, int> threadToEventCount;
  std::unordered_map<long int, int> threadToConnCount;

  // We store events here so they would get ordered
  EventHeap * shortTermHeap;
  
  // Event filenames. 
  std::vector<std::string>::iterator currentEventFile;
  std::vector<std::string> eventsFiles;
  
  // Event IF Stream.
  std::ifstream currentEventIFStream;
  std::vector<std::ifstream*>::iterator eventIFStreamsItr;
  std::vector<std::ifstream*> eventsIFStreams;
  
  // Event MMaps (if we're using mmap, bool useMMap).
  std::vector<void *> mmapedFiles;
  std::vector<void *>::iterator mmappedFilesItr;
  std::unordered_map<void *, int> mmapToSize;
  
  // Last line we read 
  unsigned long int lastLine = 0;

  // This holds data we read or made up
  std::vector <std::vector <std::string>> eventData; 
  
  std::string connectionFile;
  std::string IPListFile;
  std::string foreignIPFile;
  std::string trim(const std::string& str, const std::string& whitespace = " \t");
  int loopedCount = 0;
  long int loopDuration = 0;
  long int lastEventTime = 0;

  // Statistics about connections
  std::map<long int, struct stats>* connStats;
  
  bool useMMap;
  bool DEBUG=false;

  
 public:
  FileWorker(EventNotifier* loadMoreNotifier, EventQueue** out, std::string& ipFile, std::string& forFile,
	     std::vector<std::string>& eFiles, std::map<long int, struct stats>* cs, int nt, bool debug, std::string myname, bool useMMap=true);
  ~FileWorker();
  bool startup();
  void loop(std::chrono::high_resolution_clock::time_point startTime);
  void setStartTime(unsigned long int sTime);
};

#endif
