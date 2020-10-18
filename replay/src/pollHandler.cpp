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

/* pollHandler monitors read and write events on 
   non-blocking sockets using epoll */

#include "pollHandler.h"

// Constructor
PollHandler::PollHandler(bool debug) {  
  DEBUG = debug;
  epollfd = epoll_create1(0);
  if(epollfd == -1) { 	 
    throw std::runtime_error("Failure to create epoll.");
  }
}

PollHandler::~PollHandler() {
}

// Set a watch for read event
void PollHandler::watchForRead(int fd) {
  watch(fd, READ);
  fdsToWatchForRead.insert(fd);
}

// How many read events are we watching for
int PollHandler::rssize()
{
  return fdsToWatchForRead.size();
}

// Is the given socket watched for read
bool PollHandler::checkForRead(int fd)
{
  return (fdsToWatchForRead.find(fd) != fdsToWatchForRead.end()) ;
}

// Watch for write event
void PollHandler::watchForWrite(int fd) {
  // Should ONLY be called if we have attempted to write the socket, 	
  // and got EAGAIN! Should always remove write watch after. 		
  watch(fd, WRITE);
}

// Watch the socket for read or write
void PollHandler::watch(int fd, epollWatchType type) {
  // Set up epoll event structure. 
  struct epoll_event event = {0};
  event.data.fd = fd;
  
  // Are we looking for incoming data? Or the ability to write to an EAGAIN fd?
  if(type == READ)
    event.events = EPOLLIN |  EPOLLET;
  else if(type == WRITE)
    // If we're watching for a write, don't rearm the fd after an event. 
    event.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == -1) {
  
    // We might have failed because we're already watching this fd. 
    if(errno == EEXIST) {
      if(epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) != -1)
        return;
    }
    
    // We've failed to watch this fd, fail silently
  }
  
  return;
}

// Stop watching for an event
void PollHandler::stopWatching(int fd) {
  if(epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) != -1) return;
  perror("Removing fd from epoll.");
  fdsToWatchForRead.erase(fd);
}

// Wait for events for timeout milliseconds
int PollHandler::waitForEvents(int timeout) {
  // Wait for events, or timeout. 				
  eventIndex = 0;
  currentEventCount = epoll_wait(epollfd, events, MAX_EPOLL_EVENTS, timeout);
  return currentEventCount;
}       

// get next event
bool PollHandler::nextEvent(struct epoll_event *e) {
  // If we have no more events from this last return from epoll_wait, return false. 
  if(currentEventCount == 0) return false;

  // If we've still got event, copy the next event from the events array to the passed in pointer. 
  memcpy(e, events + eventIndex, sizeof(struct epoll_event));

  // We have EPOLLONESHOT with writes, so if we're also watching for reads on this fd, we need to rearm. 
  if(e->events & EPOLLOUT) {
    if(fdsToWatchForRead.find(e->data.fd) != fdsToWatchForRead.end()) {
      watchForRead(e->data.fd);
    }
  }

  // Update our array index and our current event count. 
  eventIndex++;
  currentEventCount--;
  return true;
}
