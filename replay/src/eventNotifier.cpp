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


#include "eventNotifier.h"

// Create file descriptor
int createEventFD() {
  unsigned int val = 0;
  int fd = eventfd(val, O_NONBLOCK);
  if(fd == -1) {
    std::cerr << strerror(errno) << std::endl;
    return -1;
  }
  return fd;
}

// Create the object
EventNotifier::EventNotifier(int myfd, std::string name) {
  fd = myfd;
  myName = name;
}

// Destructor
EventNotifier::~EventNotifier() {
}

// Return file descriptor
int EventNotifier::myFD() {
  return fd;
}

// Write to fd
bool EventNotifier::sendSignal() {
  uint64_t value = 1;
  int i = write(fd, &value, sizeof(value));  
  if( i != sizeof(value)) {
    return false;
  }
  return true;
}

// read from fd
bool EventNotifier::readSignal() {
  uint64_t value; 
  int i = read(fd, &value, sizeof(value));
  if(i != 1) {
    return false;
  }
  return true;
}


