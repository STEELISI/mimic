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


#include "utils.h"

// Should we load more events?
bool returnLoadMoreFileEvents() {
  return(loadMoreFileEvents | ! isRunning.load());
}

// Time elapsed in millis from the start
long int msSinceStart(std::chrono::high_resolution_clock::time_point startTime) {

  std::chrono::high_resolution_clock::time_point timePoint = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> sinceStart = timePoint-startTime;
  auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sinceStart);
  long int now = (long int)(int_ms.count());
  
  return(now);

}

// Create an event
Event::Event(std::string ss, std::string cs, int fd, long int mfs, long int mfle, EventType t, long int cid, long int eid, long int w, long int v)
{
  serverString = ss;
  connString= cs;
  sockfd = fd;
  ms_from_start = mfs;
  ms_from_last_event = mfle;
  type = t;
  conn_id = cid;
  event_id = eid;
  wait = w;
  value = v;
}

// Create an event
Event::Event()
{
  serverString = "";
  connString = "";
  sockfd = -1;
  ms_from_start = 0;
  ms_from_last_event = 0;
  type = NONE;
  conn_id = -1;
  event_id = -1;
  value = 0;
}

