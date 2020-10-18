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
#include <fstream> 
#include <unordered_map>
#include "eventQueue.h"

// Ordering events
int compareEvents::operator()(const Event& e1, const Event& e2) {
  if (e1.ms_from_start != e2.ms_from_start)
    return e1.ms_from_start > e2.ms_from_start;
  else
    return e1.event_id > e2.event_id;
}


// Based on Herb Sutter's lockless queue article.
// See http://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448 for details


// Each EventQueue object can have a single producer and a separate single consumer.
// In practice, we probably want to have a single producer thread for multiple EventQueue, and 
// consumer threads which consume from multiple EventQueues.

EventQueue::EventQueue(std::string name) {
#ifndef __cpp_lib_atomic_is_always_lock_free
  std::cout << "Exiting due to lack of atomic lock-free support. Check that your compiler is ISO C++ 2017 or 2020 compliant.\n" ;
  exit(1);
#endif
            
  std::shared_ptr<Event> dummy = nullptr;
  eventJob * d = new eventJob(dummy);
  last.store(d, std::memory_order_relaxed); 
  divider.store(d, std::memory_order_relaxed);
  first = d;
  qName = name;
}

// Destructor
EventQueue::~EventQueue() {
  while(first != nullptr) {
    eventJob * tmp = first;
    first = tmp->next;
    delete tmp;
  }
}

// Clean up
int EventQueue::cleanUp() {
  while(first != divider.load()) {
    eventJob * tmp = first;
    first = first->next;
    
    tmp->eptr.reset();
    delete tmp; 
    numEvents--;
  }
  return numEvents;
}

// How many events are in the queue
int EventQueue::getLength() {
  return numEvents;
}

// Add event to queue
void EventQueue::addEvent(std::shared_ptr<Event> e) {
  eventJob * lastNode = last.load();
  lastNode->next = new eventJob(e);
  
  last.store(lastNode->next);
  numEvents++;
  cleanUp();
  
}
        
// The below two functions should only be called by the consumer.
// Dequeue an event and return 
bool EventQueue::getEvent(std::shared_ptr<Event>& job) {
  eventJob * dividerNode = divider.load();
  if(dividerNode != last.load()) {
    if (dividerNode->next != nullptr) {
      job = dividerNode->next->eptr;
      divider.store(dividerNode->next);
      
      return true;
    }
  }
  return false;
}

// Peak into next event time
long int EventQueue::nextEventTime() {
  eventJob * dividerNode = divider.load();
  if(dividerNode != last.load()) {
    if(dividerNode->next != nullptr) {
      long int t = dividerNode->next->eptr->ms_from_start;
      return t;
    }
  }
  return -1;
}

// Constructor
EventHeap::EventHeap() {
}

// Destructor
EventHeap::~EventHeap() {
}

// Add event to heap
void EventHeap::addEvent(Event e) {
    eventHeap.push(e);
}

// Get time of next event
long int EventHeap::nextEventTime() {
    if(eventHeap.empty() == true) {
        return -1;
    }
    long int t = eventHeap.top().ms_from_start;
    return t;
}

// Remove next event and return it
Event EventHeap::nextEvent() {
    Event e = eventHeap.top();
    eventHeap.pop();
    return e;
}

// How many events are in the heap
int EventHeap::getLength() {
  return eventHeap.size();
}

// For debugging purposes
void EventHeap::printToFile(std::ofstream &myFile)
{
   jobHeap temp;
   while(!eventHeap.empty()) {
     Event e = eventHeap.top();
     eventHeap.pop();
     myFile<<e.conn_id<<":"<<e.event_id<<":"<<EventNames[e.type]<<":"<<e.ms_from_start<<" ";
     temp.push(e);
   }
   myFile << '\n';
   eventHeap = temp;
}

// For debugging purposes
void EventHeap::print() {
  jobHeap temp;
   while(!eventHeap.empty()) {
     Event e = eventHeap.top();
     eventHeap.pop();
     std::cout <<e.conn_id<<":"<<e.event_id<<":"<<EventNames[e.type]<<":"<<e.ms_from_start<<" ";
     temp.push(e);
   }
   std::cout << '\n';
   eventHeap = temp;
}

