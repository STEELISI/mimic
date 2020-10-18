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

/* EventQueue class stores events like a linked list would but in 
   a way that they can be locked and shared between threads. 

   On the other hand, EventHeap class stores events so that they 
   are always ordered 
*/

#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H 

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
#include <unordered_map>
#include "utils.h"



class compareEvents {
    public:
        int operator()(const Event& e1, const Event& e2);
};

typedef std::priority_queue <Event, std::vector<Event>, compareEvents> jobHeap;

class EventQueue {
 private:
  struct eventJob {
  eventJob(std::shared_ptr<Event> ptr): eptr(ptr), next(nullptr) {}
    std::shared_ptr<Event> eptr;
    eventJob* next;
  };
  eventJob* first;
#ifdef __cpp_lib_atomic_is_always_lock_free 
  static_assert(std::atomic<eventJob*>::is_always_lock_free, "We can't use eventJob* as a lock-free type.");
  std::atomic<eventJob*> divider;
  std::atomic<eventJob*> last;
#endif
  int numEvents = 1;
  
  std::string qName;
  
 public:
  EventQueue(std::string name="");
  ~EventQueue();
  int cleanUp();
  void addEvent(std::shared_ptr<Event> e);
  bool getEvent(std::shared_ptr<Event>& job);
  int getLength();
  
  long int nextEventTime();
};



// EventHeap class
class EventHeap {
 private:
  jobHeap eventHeap;
 public:
  EventHeap();
  ~EventHeap();
  void addEvent(Event e);
  long int nextEventTime();
  int getLength();
  void print();
  void printToFile(std::ofstream&);
  Event nextEvent();
};


#endif
