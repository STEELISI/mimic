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

/* This is the EventHandler that receives events from 
   FileWorker and enacts them */

#include <fstream>
#include <exception>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "eventHandler.h"
#include "connections.h"

// Max length of a message
// We can send more than this but we'll do it in chunks
#define MAXLEN 1000000
// How many file events we process in one loop cycle
#define CHUNK 1000


// Update stats on a new connection
void EventHandler::newConnectionUpdate(int sockfd, long int conn_id, long int planned, long int now)
{  
  sockfdToConnIDMap[sockfd] = conn_id;
  myConns[conn_id].sockfd = sockfd;
  myConns[conn_id].waitingToRecv = 0;
  myConns[conn_id].waitingToSend = 0;
  myConns[conn_id].stalled = false;
  
  if (planned > 0)
    myConns[conn_id].lastPlannedEvent = planned;
  else
    {
      // Calculate how far we are from planned timing
      int delay = (now - myConns[conn_id].lastPlannedEvent);
      if (delay > 0)
	{
	  myConns[conn_id].delay += delay;	  
	  (*connStats)[conn_id].delay = myConns[conn_id].delay;
	}
      myConns[conn_id].lastPlannedEvent = now;
    }
  if (DEBUG)
    (*out)<<"Conn "<<conn_id<<" time now "<<now<<" planned time "<<myConns[conn_id].lastPlannedEvent<<" delay "<< myConns[conn_id].delay<<std::endl;
}

// Update stats on an existing connection
void EventHandler::connectionUpdate(long int conn_id, long int planned, long int now) {

  if (planned > 0)
    myConns[conn_id].lastPlannedEvent = planned;
  else
    {
      // Calculate how far we are from planned timing
      myConns[conn_id].delay += (now - myConns[conn_id].lastPlannedEvent);
      myConns[conn_id].lastPlannedEvent = now;     
      (*connStats)[conn_id].delay = myConns[conn_id].delay;
    }
  if (DEBUG)
    (*out)<<"Conn "<<conn_id<<" time now "<<now<<" planned time "<<myConns[conn_id].lastPlannedEvent<<" delay "<<myConns[conn_id].delay<<std::endl;
}


// Dispatch a new job
void EventHandler::dispatch(Event dispatchJob, long int now) {
  char buf[MAXLEN];

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  if (DEBUG)
    (*out)<<ms<<" EH: dispatch job type "<<EventNames[dispatchJob.type]<<" conn "<<dispatchJob.conn_id<<" value "<<dispatchJob.value<<" now "<<now<<std::endl;
  
  switch(dispatchJob.type) {
    
  case ACCEPT: {
    // Right now we allow started servers to sit forever but this structure will
    // help us close them when they are not in use. This is future work.
    if (serverToCounter.find(dispatchJob.serverString) == serverToCounter.end())
      serverToCounter[dispatchJob.serverString] = 0;
    serverToCounter[dispatchJob.serverString]++;
    if (DEBUG)
      (*out)<<"Server "<<dispatchJob.serverString<<" connections "<<serverToCounter[dispatchJob.serverString]<<std::endl;
    
    // Store some stats on this connection and watch the socket for new connection we expect
    newConnectionUpdate(dispatchJob.sockfd, dispatchJob.conn_id, dispatchJob.ms_from_start, now);
    myConns[dispatchJob.conn_id].serverString = dispatchJob.serverString;
    myPollHandler->watchForWrite(myConns[dispatchJob.conn_id].sockfd);
    break;
  }
  case RECV: {
    // Waiting to receive some number of bytes
    connectionUpdate(dispatchJob.conn_id, dispatchJob.ms_from_start, now);
    if (DEBUG)
      (*out)<<"RECV JOB waiting to recv "<<myConns[dispatchJob.conn_id].waitingToRecv<<" on conn "<<dispatchJob.conn_id<<" job value "<<dispatchJob.value<<std::endl;
    myConns[dispatchJob.conn_id].waitingToRecv = myConns[dispatchJob.conn_id].waitingToRecv + dispatchJob.value;
    
    // Could be that we received bytes earlier and now just have to advance counter
    // This advancement is imprecise, since we don't check if we received bytes for one or
    // several events
    if (myConns[dispatchJob.conn_id].waitingToRecv <= 0)
      {
	(*connStats)[dispatchJob.conn_id].last_completed++;
	connectionUpdate(dispatchJob.conn_id, 0, now);
	getNewEvents(dispatchJob.conn_id);
	if (DEBUG)
	  (*out)<<"For conn "<<dispatchJob.conn_id<<" last completed "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
	break;
      }
    // We have to try to receive bytes
    while(myConns[dispatchJob.conn_id].waitingToRecv > 0)
      {
	if (DEBUG)
	  (*out)<<"Waiting for conn "<<dispatchJob.conn_id<<" to recv "<<myConns[dispatchJob.conn_id].waitingToRecv<<std::endl;
	// Wrap recv in try / catch since it sometimes may error out
	try
	  {
	    // Receive in chunks and piece it together
	    int n = recv(dispatchJob.sockfd, buf, MAXLEN, 0);
	    int total = 0;
	    while (n == MAXLEN)
	      {
		total += n;
		n = recv(dispatchJob.sockfd, buf, MAXLEN, 0);
	      }
	    total += n;
	    
	    // If we received anything
	    if (total > 0)
	      {
		if (DEBUG)
		  (*out)<<"RECVd "<<total<<" bytes for conn "<<dispatchJob.conn_id<<std::endl;
		
		// Update peer time, this helps us to sync speed between two peers
		if (dispatchJob.origTime > peerTime)
		  peerTime = dispatchJob.origTime;
		
		// Adjust how much we're waiting to receive
		myConns[dispatchJob.conn_id].waitingToRecv -= total;
		
		// We need this for throughput calculation
		my_bytes += total;
		if (DEBUG)
		  (*out)<<"RECV waiting now for "<<myConns[dispatchJob.conn_id].waitingToRecv<<" conn "<<dispatchJob.conn_id<<std::endl;
		
		// Check if lower than 0 or 0 move new event ahead
		if (myConns[dispatchJob.conn_id].waitingToRecv <= 0)
		  {
		    (*connStats)[dispatchJob.conn_id].last_completed++;
		    if (DEBUG)
		      (*out)<<"For conn "<<dispatchJob.conn_id<<" last completed "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
		    connectionUpdate(dispatchJob.conn_id, 0, now);
		    getNewEvents(dispatchJob.conn_id);		      
		    break;
		  }
	      }
	    else
	      {
		// We wanted to receive something but failed. Set a watch on the socket
		if (DEBUG)
		  (*out)<<"Will wait to RECV "<<myConns[dispatchJob.conn_id].waitingToRecv<<" for conn "<<dispatchJob.conn_id<<" on sock "<<dispatchJob.sockfd<<std::endl;
		myPollHandler->watchForRead(dispatchJob.sockfd);
		break;
	      }
	  }
	catch(std::exception& e)
	  {
	    std::cerr<<"Errored out while receiving for "<<dispatchJob.conn_id<<" exception "<<e.what()<<std::endl;
	    break;
	  }
      }
    // Finished handling RECV
    break;
  }
  case CONNECT: {

    // Handle a connection start event
    // We may be ahead of the peer
    if (myTime - peerTime > SRV_UPSTART && peerTime > 0 && dispatchJob.origTime > peerTime)
      {
	if (DEBUG)
	  (*out)<<"Postpone connecting for conn "<<dispatchJob.conn_id<<" connstring "<<dispatchJob.connString<<" serverstring "<<dispatchJob.serverString<<" state "<<myConns[dispatchJob.conn_id].state<<" conn time "<<dispatchJob.origTime<<" my time "<<myTime<<" peer "<<peerTime<<std::endl;
	// Wait a second and try again
	dispatchJob.ms_from_start += 1000;
	eventsToHandle->addEvent(dispatchJob);
	break;
      }
    else
      {
	// Time to try to connect
	if (dispatchJob.ms_from_start > myTime)
	  {
	    myTime = dispatchJob.ms_from_start;
	    if (DEBUG)
	      (*out)<<"Starting conn "<<dispatchJob.conn_id<<" my time "<<myTime<<std::endl;
	  }
	// Some bookkeeping
	long int conn_id = dispatchJob.conn_id;
	(*connStats)[conn_id].thread = myID;
	myConns[conn_id].lastPlannedEvent = dispatchJob.ms_from_start;
	myConns[conn_id].origTime = dispatchJob.ms_from_start;

	myConns[dispatchJob.conn_id].state = CONNECTING;
	(*connStats)[dispatchJob.conn_id].state = CONNECTING;
	struct sockaddr_in caddr = getAddressFromString(dispatchJob.connString);
	struct sockaddr_in saddr = getAddressFromString(dispatchJob.serverString);

	// Get a socket
	int sockfd = getIPv4TCPSock((const struct sockaddr_in *)&caddr);
	if (DEBUG)
	  (*out)<<"Connecting on sock "<<sockfd<<" for conn "<<dispatchJob.conn_id<<" connstring "<<dispatchJob.connString<<" serverstring "<<dispatchJob.serverString<<" state "<<myConns[dispatchJob.conn_id].state<<std::endl;
	// Try to connect
	if(connect(sockfd, (const struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == -1) {
	  if (DEBUG)
	    (*out)<<"Didn't connect right away\n";
	  if (errno != EINPROGRESS)
	    {
	      // Failed but not bc of non-blocking socket
	      close(sockfd);
	      if (DEBUG)
		{
		  char errmsg[200];
		  sprintf(errmsg, " connecting failed, conn %ld src %s",dispatchJob.conn_id,dispatchJob.serverString.c_str());
		  (*out)<<errmsg<<std::endl;
		}
	      // Not ready, return to queue and try again
	      dispatchJob.ms_from_start += 1000;
	      eventsToHandle->addEvent(dispatchJob);
	      return;
	    }
	  else
	    {
	      // Failed bc of non-blocking socket, set a watch 
	      myPollHandler->watchForWrite(sockfd);
	      newConnectionUpdate(sockfd, dispatchJob.conn_id, dispatchJob.ms_from_start, now);
	    }
	}
	else
	  {
	    // Managed to connect
	    myConns[dispatchJob.conn_id].state = EST;
	    newConnectionUpdate(sockfd, dispatchJob.conn_id, dispatchJob.ms_from_start, now);
	    (*connStats)[dispatchJob.conn_id].last_completed++;
	    (*connStats)[dispatchJob.conn_id].started = now;
	    if (DEBUG)
	      (*out)<<"Connected successfully for conn "<<dispatchJob.conn_id<<" state is now "<<myConns[dispatchJob.conn_id].state<<" last completed "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
	    getNewEvents(dispatchJob.conn_id);
	  }
      }  
    break;
  }
  case SEND: {
    // Some bookkeeping
    connectionUpdate(dispatchJob.conn_id, dispatchJob.ms_from_start, now);
    myConns[dispatchJob.conn_id].waitingToSend += dispatchJob.value;
    if (DEBUG)
      (*out)<<"Handling SEND event waiting to send "<<myConns[dispatchJob.conn_id].waitingToSend<<" on sock "<<dispatchJob.sockfd<<std::endl;
    
    // Try to send, until all is sent or we error out
    while (myConns[dispatchJob.conn_id].waitingToSend > 0)
      {
	try{
	  long int tosend = myConns[dispatchJob.conn_id].waitingToSend;
	  if (tosend > MAXLEN)
	    tosend = MAXLEN;
	  if (DEBUG)
	    (*out)<<"Conn "<<dispatchJob.conn_id<<" will try to send "<<tosend<<"\n";
	  int n = send(dispatchJob.sockfd, buf, tosend, 0);
	  if (n < 0)
	    {
	      // Tried to send and failed, watch the socket
	      myPollHandler->watchForWrite(dispatchJob.sockfd);
	      if (DEBUG)
		(*out)<<"Did not manage to send, but set write flag\n";
	      break;
	    }
	  else
	    {
	      long int conn_id = dispatchJob.conn_id;
	      myConns[conn_id].waitingToSend -= n;
	      if (DEBUG)
		(*out)<<"Successfuly handled SEND event for conn "<<conn_id<<" for "<<n<<" bytes\n";
	      if (myConns[conn_id].origTime < dispatchJob.origTime)
		myConns[conn_id].origTime = dispatchJob.origTime;
	      if (myConns[conn_id].origTime > myTime)
		myTime = myConns[conn_id].origTime;
	      my_bytes += n;
	    }
	}
	catch(int e)
	  {
	    // Give up but don't quit the program
	    std::cerr<<"Errored out while sending for "<<dispatchJob.conn_id<<std::endl;
	    break;
	  }
      }

    // We sent everything
    if (myConns[dispatchJob.conn_id].waitingToSend == 0)
      {
	(*connStats)[dispatchJob.conn_id].last_completed++;
	if (DEBUG)
	  (*out)<<"For conn "<<dispatchJob.conn_id<<" last completed "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
      }
    break;
  }
    
  case SRV_START: {
    // Check if the server is already started 
    if (srvStarted.find(dispatchJob.serverString) != srvStarted.end())
      {
	// Remember which connection we're dealing with
	if(strToConnID.find(dispatchJob.connString) == strToConnID.end())
	  {
	    strToConnID[dispatchJob.connString] = dispatchJob.conn_id;
	    
	    if (DEBUG)
	      (*out)<<"Associated conn "<<dispatchJob.conn_id<<" with "<<dispatchJob.connString<<std::endl;
	    myConns[dispatchJob.conn_id].origStart = dispatchJob.ms_from_start + SRV_UPSTART;
	  }
	else
	  {
	    // Another connection with same connString, save it
	    pendingConns[dispatchJob.connString].push_back(dispatchJob.conn_id);
	    if (DEBUG)
	      (*out)<<"Associated pending conn "<<dispatchJob.conn_id<<" with "<<dispatchJob.connString<<std::endl;
	    myConns[dispatchJob.conn_id].origStart = dispatchJob.ms_from_start + SRV_UPSTART;
	  }
	break;
      }
    // Try to start the server
    if (DEBUG)
      (*out)<<"Starting server "<<dispatchJob.serverString<<std::endl;
    if(strToConnID.find(dispatchJob.connString) == strToConnID.end())
      {
	strToConnID[dispatchJob.connString] = dispatchJob.conn_id;
	if (DEBUG)
	  (*out)<<"Associated conn "<<dispatchJob.conn_id<<" with "<<dispatchJob.connString<<std::endl;
	}
    std::string servString = dispatchJob.serverString;
    struct sockaddr_in addr;
    getAddrFromString(servString, &addr);
    int sockfd = getIPv4TCPSock((const struct sockaddr_in*)&addr);
    if(sockfd == -1) {
      // Couldn't start it
      std::cerr << "Failed to bind to " << servString << std::endl;
      return;
    }

    // Generic connection, accept will create new sock for each new conn
    sockfdToConnIDMap[sockfd] = -1; 
    serverToSockfd[dispatchJob.serverString] = sockfd;
    if(listen(sockfd, MAX_BACKLOG_PER_SRV) == -1) {
      perror("Listen failed");
      return;
    }
    srvStarted[dispatchJob.serverString] = now;
    if (DEBUG)
      (*out)<<"Listening on sock "<<sockfd<<" for server "<<dispatchJob.serverString<<std::endl;

    // Watch for new connection attempts
    myPollHandler->watchForRead(sockfd);
    break;
  }
    
  case CLOSE:{
    long int conn_id = dispatchJob.conn_id;
    // Check if we are ready
    if (myConns[conn_id].waitingToSend > 0  || myConns[conn_id].waitingToRecv > 0)
	{
	  if (DEBUG)
	    (*out)<<"Conn "<<conn_id<<" not ready, waiting to send "<<myConns[conn_id].waitingToSend<<" and to receive "<<myConns[conn_id].waitingToRecv<<std::endl;
	  // Not ready, return to queue and try again later
	  dispatchJob.ms_from_start += 1000;
	  eventsToHandle->addEvent(dispatchJob);
	}
    else
      {
	if (DEBUG)
	  (*out)<<"Received CLOSE for conn "<<conn_id<<" event "<<dispatchJob.event_id<<std::endl;

	close(dispatchJob.sockfd);
	(*connStats)[conn_id].state = DONE;
	(*connStats)[conn_id].last_completed++;
	(*connStats)[conn_id].completed = now;
	
	if (myConns[conn_id].serverString != "")
	  serverToCounter[myConns[conn_id].serverString] --;
	
	myConns.erase(conn_id);
	sockfdToConnIDMap.erase(dispatchJob.sockfd);
	if (strToConnID.find(dispatchJob.connString) != strToConnID.end())
	  strToConnID.erase(dispatchJob.connString);
	
	if (DEBUG)
	  (*out)<<"Closed sock "<<dispatchJob.sockfd<<" for conn "<<conn_id<<" last completed "<<(*connStats)[conn_id].last_completed<<std::endl;
      }
    break;
  }
    
  case SRV_END: {
    // We don't do this now but should put it back in the future
    if (serverToCounter[dispatchJob.serverString] == 0)
      {
	if (DEBUG)
	  (*out)<<"Stopping server "<<dispatchJob.serverString<<" time "<<now<<" sock "<<serverToSockfd[dispatchJob.serverString]<<std::endl;
	close(serverToSockfd[dispatchJob.serverString]);
	serverToCounter.erase(dispatchJob.serverString);
      }
    // Try again after a while
    else
      {
	if (DEBUG)
	  (*out)<<"Would like to stop server "<<dispatchJob.serverString<<" time "<<now<<" sock "<<serverToSockfd[dispatchJob.serverString]<<" but counter is "<<serverToCounter[dispatchJob.serverString]<<std::endl;
	dispatchJob.ms_from_start = now + 1000;
	eventsToHandle->addEvent(dispatchJob);
      }
    break;
  }
    // Default case, should not happen
  default: {
    break;
  }
  }
  //dispatchJob.reset(); // perhaps this should be put back
}

// Startup function, doesn't do anything
bool EventHandler::startup() {
  if (DEBUG)
    (*out)<<"Event handler starting\n";
  return true;
}

// Check if there are connections that were waiting for file events
void EventHandler::checkStalledConns(long int now)
{
  // Go through conns and try to load more events if there are any
  for (auto it = myConns.begin(); it != myConns.end();)
    {
      // Re-set watch for write events
      if (myConns[it->first].waitingToSend > 0)
	myPollHandler->watchForWrite(myConns[it->first].sockfd);
      // If connection is ready to proceed try to get more events
      if (myConns[it->first].waitingToSend <= 0 &&  myConns[it->first].waitingToRecv <= 0 && myConns[it->first].state != DONE && myConns[it->first].stalled)
	  getNewEvents(it->first);

      // This could be a connection that multiple threads were waiting on and one won
      // Erase it from other threads
      if ((*connStats)[it->first].thread != -1 && (*connStats)[it->first].thread != myID)
	{
	  auto eit = it;
	  it++;
	  myConns.erase(eit);
	}
      else
	it++;
    }
}

// Check if there are connections whose conn_id we could not
// establish on ACCEPT, but now we can bc we loaded more file events
void EventHandler::checkOrphanConns(long int now)
{
  // Go through conns and try to load more events if there are any
  for (auto it = orphanConn.begin(); it != orphanConn.end(); )
    {
      auto sit = strToConnID.find(it->first);
      if (sit != strToConnID.end())
	{
	  // Found a new connection for that connection string
	  long int conn_id = sit->second;
	  if (DEBUG)
	    (*out)<<"Found conn "<<conn_id<<" on socket "<<it->second<<" time "<<now<<std::endl;
	  myConns[conn_id].lastPlannedEvent = now;
	  newConnectionUpdate(it->second, conn_id, 0, now);
	  myConns[conn_id].state = EST;
	  (*connStats)[conn_id].state = EST;
	  (*connStats)[conn_id].last_completed++;
	  (*connStats)[conn_id].started = now;
	  (*connStats)[conn_id].thread = myID;
	  getNewEvents(conn_id);
	  long int ftime = myConns[conn_id].origStart;

	  // Update peer time
	  if (ftime > peerTime)
	    peerTime = ftime;

	  auto dit = it;
	  it++;

	  // Not an orphan anymore
	  orphanConn.erase(dit);
	}
      else
	it++;
    }
}

// Main loop where we process all events
void EventHandler::loop(std::chrono::high_resolution_clock::time_point startTime) {
  // Relative time
  long int now = msSinceStart(startTime);  

  long int lastStats = now;
  myTime = peerTime = 0;


  // Allocate a really big buffer filled with a's
  char* buf = (char*)malloc(MAXLEN);
  memset(buf, 'a', MAXLEN);

  long int fileEvents = 0;
  bool requested;
  long int processedFileEvents = 0;
  int eventsHandled = 0;
  int rounds = 0;

  // While we're running (i.e., forever)
  while(isRunning.load()) {
    rounds++;
    fileEvents = incomingFileEvents->getLength();
    

    long int nextEventTime = incomingFileEvents->nextEventTime();
    long int thisChunk = 0;

    // Put a chunk of incomingFileEvents into connection-specific queues
    while(nextEventTime >= 0 && thisChunk <= CHUNK) {

      std::shared_ptr<Event> job;
      if(incomingFileEvents->getEvent(job)){
	thisChunk++;
	processedFileEvents++;
	if (processedFileEvents/fileEvents > 0.9)
	  {
	    requested = false;
	    processedFileEvents = 0;
	  }
	
	Event dispatchJob = *job;

	if (DEBUG)
	  (*out)<< "File Event handler GOT JOB " << EventNames[dispatchJob.type] <<" serverstring "<<dispatchJob.serverString<<" conn "<<dispatchJob.conn_id<<" event id "<<dispatchJob.event_id<<" ms from start "<<dispatchJob.ms_from_start<<" now "<<now<<" value "<<dispatchJob.value<<" server "<<dispatchJob.serverString<<" left in queue "<<incomingFileEvents->getLength()<<std::endl;
	// Add job to connection-specific queue
	if (dispatchJob.type == SEND || dispatchJob.type == RECV || dispatchJob.type == CLOSE)
	  myConns[dispatchJob.conn_id].eventQueue.addEvent(dispatchJob);
	else
	  {
	    // CONNECT or SRV_START, we have to handle it
	    eventsToHandle->addEvent(dispatchJob);
	    if (DEBUG)
	      (*out)<<"Added job to eventsToHandle "<<std::endl;
	  }
	nextEventTime = incomingFileEvents->nextEventTime();
      }
      else {
	if (DEBUG)
	  (*out)<< "We think we have a job, but failed to pull it? " << std::endl;
      }
      job.reset();
    }
    if (DEBUG)
      (*out)<< "EVENT HANDLER: Next event time is: " << nextEventTime << " Now is " << now << std::endl; 

    // If we handled more than half of maximum file events we can ask for more
    if(fileEventsHandledCount > maxQueuedFileEvents/2) {
      fileEventsHandledCount = 0;
      std::unique_lock<std::mutex> lck(fileHandlerMTX);
      if (DEBUG)
	(*out)<< "Sending wake to fileWorker." << std::endl;
      loadMoreFileEvents = true;
      fileHandlerCV.notify_one();
      lck.unlock();
    }

    // Now handle events that became ready, such as CONNECT/SRV_START or events from connection queues
    long int nextHeapEventTime = eventsToHandle->nextEventTime();
  
    while(nextHeapEventTime <= now && nextHeapEventTime >= 0) {
      Event dispatchJob = eventsToHandle->nextEvent();
      eventsHandled++;
      
      fileEventsHandledCount++;
      
      if (DEBUG)
	(*out)<< "Heap Event handler GOT JOB " << EventNames[dispatchJob.type] <<" server "<<dispatchJob.serverString<<" conn "<<dispatchJob.conn_id<<" event "<<dispatchJob.event_id<<" ms from start "<<dispatchJob.ms_from_start<<" now "<<now<<" value "<<dispatchJob.value<<" events handled "<<fileEventsHandledCount<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
      
      dispatch(dispatchJob, now);
      nextHeapEventTime = eventsToHandle->nextEventTime();
    }

  // Check what time it is now
  now = msSinceStart(startTime);
    
  // Check if we should ask for more events
  if (DEBUG)
    (*out)<<"fileEventsHandledCount "<<fileEventsHandledCount<<" file events "<<fileEvents<<" incoming length "<<incomingFileEvents->getLength()<<" nextEventtime "<<nextEventTime<<" eventstohandle "<<eventsToHandle->getLength()<<std::endl;
  // If we're reading from file then once we process more than half of what we got, or we come to the end of the queue and we're not stuck on many events in
  // eventsToHandle queue we will ask for more
  // If we are making up events, we will not allow more than two full loads to pile up
  // These conditions are a bit ad hoc
  if((!makeup.load() && (incomingFileEvents->getLength() < fileEvents/2 || nextEventTime < 0) && eventsToHandle->getLength() < 5*maxQueuedFileEvents)
     || (makeup.load() && myConns.size() < 2*numconns.load()))
    {
      lastEventCountWhenRequestingForMore += fileEventsHandledCount;
      if (DEBUG)
	(*out)<<"Requesting more events, last "<<lastEventCountWhenRequestingForMore<<" file events "<<fileEvents<<" handled "<<fileEventsHandledCount<<" comparisong between "<<fileEventsHandledCount<<" and "<<fileEvents/2<<" is "<<(fileEventsHandledCount > fileEvents/2)<<" requested is "<<requested<<std::endl;
      fileEventsHandledCount = 0;
      requested = true;
      requestMoreFileEvents->sendSignal();
    }
    
  // Check epoll events.
  int timeout = 1;
  
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  int numevents = myPollHandler->waitForEvents(timeout);
    
  if (DEBUG)
    (*out)<<ms<<" checking poll handler with timeout "<<timeout<<" got "<<numevents<<std::endl;
  
  // Handle any events from poll. Could be 		   
  struct epoll_event *poll_e = (struct epoll_event*) calloc(1, sizeof(struct epoll_event));
  while(myPollHandler->nextEvent(poll_e)) {
  
    eventsHandled++;
    int sockfd = poll_e->data.fd;
    long int conn_id = sockfdToConnIDMap[sockfd];
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (DEBUG)
      (*out)<<ms<<" Got event on sock "<<sockfd<<" w flags "<<poll_e->events<<" epoll in "<<EPOLLIN<<" out "<<EPOLLOUT<<" on conn "<<conn_id<<std::endl;
    if (conn_id == -1 && ((poll_e->events & EPOLLIN) > 0))
      {
	if (DEBUG)
	  (*out)<<"Got ACCEPT event and should accept connection\n";
	
	// New connection to one of our servers.
	// It could be more than one ACCEPT so keep looking for more
	while(true)
	  {
	    conn_id = acceptNewConnection(poll_e, now);
	    if (conn_id == -1)
	      {
		if (DEBUG)
		  (*out)<<"Nothing more to accept\n";
		break;
	      }
	    else if (conn_id >= 0)
	      {
		// Accepted connection
		myConns[conn_id].state = EST;
		(*connStats)[conn_id].state = EST;
		(*connStats)[conn_id].last_completed++;
		(*connStats)[conn_id].started = now;
		(*connStats)[conn_id].thread = myID;
		long int ftime = myConns[conn_id].origStart;
		getNewEvents(conn_id);
		if (ftime > peerTime)
		  {
		    peerTime = ftime;
		  }
		if (DEBUG)
		  (*out)<<"Accepted conn "<<conn_id<<" state is now "<<myConns[conn_id].state<<" last completed "<<(*connStats)[conn_id].last_completed<<std::endl;
	      }
	  }
	continue;
      }
      if (myConns[conn_id].state == CONNECTING) 
	{
	  // New connection could've succeeded 
	  if ((poll_e->events & EPOLLHUP) || (poll_e->events & EPOLLERR))
	    {
	      // We errored out on this connection and should abandon it
	      close(sockfd);
	      
	      if (myConns[conn_id].serverString != "")
		serverToCounter[myConns[conn_id].serverString] --;
	      
	      myConns.erase(conn_id);
	      sockfdToConnIDMap.erase(sockfd);

	      continue;
	    }

	  newConnectionUpdate(sockfd, conn_id, 0, now);
	  myConns[conn_id].state = EST;
	  (*connStats)[conn_id].state = EST;
	  (*connStats)[conn_id].last_completed++;
	  (*connStats)[conn_id].started = now;
	  if (DEBUG)
	    (*out)<<"Connected successfully, conn "<<conn_id<<" state is now "<<myConns[conn_id].state<<" last completed "<<(*connStats)[conn_id].last_completed<<std::endl;
	  getNewEvents(conn_id);
	  continue;
	}
      
      if (myConns[conn_id].state == EST && ((poll_e->events & EPOLLOUT) > 0))
	{
	  // Socket ready for writing
	  if ((poll_e->events & EPOLLHUP) || (poll_e->events & EPOLLERR))
	    {
	      // Errored out (usually peer closed conn), abandon conn
	      
	      close(sockfd);
	      
              if (myConns[conn_id].serverString != "")
                serverToCounter[myConns[conn_id].serverString] --;
	      
              myConns.erase(conn_id);
              sockfdToConnIDMap.erase(sockfd);
	      // Should erase from strToConnID here too // Jelena
	      continue;
	    }
	  int len = myConns[conn_id].waitingToSend;
	  
	  if (DEBUG)
	    (*out)<<"Ready to SEND event for conn "<<conn_id<<" flags "<<poll_e->events<<" should send "<<len<<std::endl;
													       
	  if (len > 0)
	    {
	      if (DEBUG)
		(*out)<<"Waiting to send "<<myConns[conn_id].waitingToSend<<" on socket "<<sockfd<<std::endl;

	      // Try to send and keep trying
	      try
		{
		  int n = send(sockfd, buf, len, 0);
		  if (n > 0)
		    {
		      if (DEBUG)
			(*out)<<"Successfully handled SEND for conn "<<conn_id<<" for "<<n<<" bytes\n";
		      
		      if (myConns[conn_id].origTime > myTime)
			myTime = myConns[conn_id].origTime;
		      
		      my_bytes += n;
		      myConns[conn_id].waitingToSend -= n;
		      // Jelena: this should be in a while loop
		      if (myConns[conn_id].waitingToSend > 0)
			{
			  if (DEBUG)
			    (*out)<<"Still have to send "<<myConns[conn_id].waitingToSend<<" bytes\n";
			  myPollHandler->watchForWrite(sockfd);
			}
		      else
			{			  
			  // Sent everything we waited for
			  connectionUpdate(conn_id, 0, now);
			  (*connStats)[conn_id].last_completed++;
			  getNewEvents(conn_id);
			}
		    }
		}
	      catch(int e)
		{
		  std::cerr<<"Errored out while sending for "<<conn_id<<std::endl;
		}
	    }
	}
      
      if (myConns[conn_id].state == EST && ((poll_e->events & EPOLLIN) > 0))
	{
	  // Read event on a socket
	  if ((poll_e->events & EPOLLHUP) || (poll_e->events & EPOLLERR))
	    {
	      // Errored out (usually peer closed conn), abandon conn
	      close(sockfd);
	      
              if (myConns[conn_id].serverString != "")
                serverToCounter[myConns[conn_id].serverString] --;
	      
              myConns.erase(conn_id);
              sockfdToConnIDMap.erase(sockfd);
	      // Should erase from strToConnID here too // Jelena
	      continue;
	      
	    }
	  if (DEBUG)
	    (*out)<<"Possibly handling a RECV event for conn "<<conn_id<<" on sock "<<sockfd<<std::endl;
	  try
	    {
	      int n = recv(sockfd, buf, MAXLEN, 0);
	      int total = 0;
	      // Jelena should be also n>0
	      while (n == MAXLEN)
		{
		  total += n;
		  n = recv(sockfd, buf, MAXLEN, 0);
		}
	      total += n;

	      if (DEBUG)
		(*out)<<"RECVd "<<total<<" bytes for conn "<<conn_id<<" orig time "<<myConns[conn_id].origTime<<" peer time "<<peerTime<<std::endl;
	      if (myConns[conn_id].origTime > peerTime)
		{
		  peerTime = myConns[conn_id].origTime;
		}
		      
	      if (total > 0)		
		{
		  // Finished w RECV, let's see what we got
		  long int waited = myConns[conn_id].waitingToRecv;
		  myConns[conn_id].waitingToRecv -= total;
		  my_bytes += total;
		  
		  if (DEBUG)
		    (*out)<<"RECV waiting now for "<<myConns[conn_id].waitingToRecv<<" on conn "<<conn_id<<std::endl;
		  
		  if (myConns[conn_id].waitingToRecv == 0 ||
		      (myConns[conn_id].waitingToRecv < 0 && waited > 0))
		    {		     
		      connectionUpdate(conn_id, 0, now);
		      (*connStats)[conn_id].last_completed++; 
		      getNewEvents(conn_id);
		    }
		}
	    }
	  catch(int e)
	    {
	      std::cerr<<"Errored out while receiving for "<<conn_id<<std::endl;
	    }
	}
  }
  free(poll_e);
  
  checkStalledConns(now);
  checkOrphanConns(now);

  // Bookkeeping and global stats update
  my_events += eventsHandled;
  now = msSinceStart(startTime);
  if (now - lastStats >= 1000)
    {
      statsMTX.lock();
      global_throughput += my_bytes;
      global_events += my_events;
      my_events = 0;
      my_bytes = 0;
      statsMTX.unlock();
    }
  (*out)<< "Relooping, time now " <<now<<" events handled "<<eventsHandled<<" my time "<<myTime<<" peer time "<<peerTime<<std::endl;
  eventsHandled = 0;
  }
  // We're done, print out for this thread what is the status of each connection
  for (auto it = myConns.begin(); it != myConns.end(); it++)
    (*out)<<"My conns "<<myConns.size()<<" conn "<<it->first<<" state "<<it->second.state<<std::endl;
}

// Get new events for a connection
long int EventHandler::getNewEvents(long int conn_id)
{
  EventHeap* e = &myConns[conn_id].eventQueue;
  long int nextEventTime = e->nextEventTime();
  long int ftime = nextEventTime;

  // Check if stalled or not
  if (nextEventTime >= 0) 
    myConns[conn_id].stalled = false;
  else 
    myConns[conn_id].stalled = true;
  
  while (nextEventTime >= 0)
    {
      Event job = e->nextEvent();
      job.sockfd = myConns[conn_id].sockfd;
      if (DEBUG)
	(*out)<< "Event handler moved new JOB " << EventNames[job.type] <<" conn "<<job.conn_id<<" event "<<job.event_id<<" for time "<<job.ms_from_start<<" to send "<<job.value<<" now moved to time "<<(job.ms_from_start+myConns[conn_id].delay)<<" because of delay "<<myConns[conn_id].delay<<std::endl;
      job.origTime = job.ms_from_start;
      job.ms_from_start += myConns[conn_id].delay;
      
      // Delay CLOSE events by 1 second
      if (job.type == CLOSE)
	job.ms_from_start += 1000;
      eventsToHandle->addEvent(job);
      nextEventTime = e->nextEventTime();
      if (nextEventTime < 0)
	{
	  myConns[conn_id].stalled = true;
	  return ftime;
	}
      // Keep adding until we add a RECV. Then we have to wait for RECV to clear.
      if (job.type == RECV)
	break;
    }
  return ftime;
}

// Accept a new connection
long int EventHandler::acceptNewConnection(struct epoll_event *poll_e, long int now) {
  int newSockfd = -1;
  struct sockaddr in_addr;
  int in_addr_size = sizeof(in_addr);
  int fd = poll_e->data.fd;
  
  // Accept new connection
  newSockfd = accept(fd, &in_addr, (socklen_t*)&in_addr_size);
  // Didn't work out
  if (newSockfd == -1)
    return -1;
  if (DEBUG)
    (*out)<<"Accepted connection\n";
  std::string serverString = getIPPortString((struct sockaddr_in*)&in_addr);
  if (serverToCounter.find(serverString) == serverToCounter.end())
    serverToCounter[serverString] = 0;
  serverToCounter[serverString]++;
  if (DEBUG)
    (*out)<<"Server "<<serverString<<" connections "<<serverToCounter[serverString]<<std::endl;
  // Set nonblocking. 
  int status = 0;
  status = setIPv4TCPNonBlocking(newSockfd);

  if(status < 0) {
    return -1;
  }
  
  // Now figure out which connection we accepted 
  // Get info on the server socket we accepted on
  struct sockaddr sa_srv;
  unsigned int sa_len;
  sa_len = sizeof(sa_srv);
  
  // We assume this is IPv4/TCP for now.
  if(getsockname(fd, (sockaddr *)&sa_srv, (unsigned int *)&sa_len) == -1) {
    perror("getsockname() failed");
    return -1;
  }
  bool success = false;
  
  std::string connString = getConnString((const struct sockaddr_in *)&in_addr, (const struct sockaddr_in*)&sa_srv, &success);
  
  if(!success) return -1;
  
  if (DEBUG)
    (*out)<< "Got connection from: " << connString << std::endl;
  
  /* Map names to a conn. */
  auto it = strToConnID.find(connString);
  if(it == strToConnID.end())
    {
      if (DEBUG)
	(*out) << "Got connection but could not look up connID." << std::endl;
      
      orphanConn[connString] = newSockfd;
      return -2; 
    }
  else
    {
      long int conn_id =  strToConnID[connString];
      if (DEBUG)
	(*out) << "Got connection from "<<connString<<" currently there is conn "<<strToConnID[connString]<<" current state "<<(*connStats)[conn_id].state<<std::endl;
      if ((*connStats)[conn_id].state >= EST && pendingConns.find(connString) != pendingConns.end() && !pendingConns[connString].empty())
	  {
	    if (DEBUG)
	      (*out) << "Got connection from "<<connString<<" associated it with "<< pendingConns[connString].front()<<std::endl;
	    strToConnID[connString] = pendingConns[connString].front();
	    conn_id = strToConnID[connString];
	    pendingConns[connString].pop_front();
	  }
      if (DEBUG)
	(*out)<<"Got connection from "<<connString<<" conn "<<strToConnID[connString]<<std::endl;
      }
    long int conn_id = it->second;
    
    // Update our statistics
    myConns[conn_id].lastPlannedEvent = now;
    newConnectionUpdate(newSockfd, conn_id, 0, now);

    return conn_id; 
}

// Create new EventHandler
EventHandler::EventHandler(EventNotifier* loadMoreNotifier, EventQueue* fe,
			   std::map<long int, struct stats>* cs, int id, bool debug, std::string myname) {

  myConns = {};
  fileEventsHandledCount = 0;
  lastEventCountWhenRequestingForMore = 0;
  out = new std::ofstream(myname);
  myID = id;
  
  incomingFileEvents = fe;
  requestMoreFileEvents = loadMoreNotifier;
  connStats = cs;
  DEBUG = debug;
  
  srvStarted = {};
  sockfdToConnIDMap = {};
  serverToCounter = {};
  
  eventsToHandle = new EventHeap();
  myPollHandler = new PollHandler(DEBUG);
  
}	

EventHandler::~EventHandler() {
}

