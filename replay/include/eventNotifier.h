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

/* Helps sync fileWorker and eventHandler */

#ifndef EVENTNOTIFIER_H
#define EVENTNOTIFIER_H 

#include <unistd.h>
#include <sys/eventfd.h>
#include "utils.h"

int createEventFD();

class EventNotifier {
    private:
      int fd;
        
    public:
      std::string myName;
      EventNotifier(int myfd, std::string name="");
      ~EventNotifier();
      bool isMe(int readfd);
      int myFD();
      bool sendSignal();
      bool readSignal();
};


#endif
