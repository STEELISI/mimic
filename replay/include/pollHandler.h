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

/* This class watches for epoll events and passes them on 
   to fileWorker and eventHandler */

#ifndef POLLHANDLER_H
#define POLLHANDLER_H  

#include <unistd.h>
#include <set>
#include "utils.h"

// Should be a relatively large number
#define MAX_EPOLL_EVENTS 1000


enum epollWatchType {
  READ,
  WRITE
};

class PollHandler {
 private:
  
  std::set<int> fdsToWatchForRead;
  int epollfd;
  struct epoll_event events[MAX_EPOLL_EVENTS];
  int eventIndex = 0;    
  int currentEventCount = 0;
  void watch(int fd, epollWatchType type);
  bool DEBUG = false;
  
 public:
  PollHandler(bool debug);
  ~PollHandler();
  int rssize();
  bool checkForRead(int fd);
  void watchForRead(int fd);
  void watchForWrite(int fd);
  void stopWatching(int fd);
  int waitForEvents(int timeout=-1);
  bool nextEvent(struct epoll_event *e);
};

#endif

