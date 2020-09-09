#include <fstream>
#include <exception>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "eventHandler.h"
#include "connections.h"

#define MAX_BACKLOG_PER_SRV 1000

//Trying a commit

/* We start 3 threads */
/*	- a server thread (takes in start/stop req, produces accepted events.)  */
/*	  out: client/serv addr (sockfd) map: addrs->connid, add sockfd		*/
/* 	- a recv thread (produces events of how much is received from each socket.) */
/*	  out: sockfd (value) map: sockfd->connid, add value		*/
/* 	- a send thread (takes in send/connect req, produces sent event). 	*/
/* 	  out: connid (value) map: none, add value		*/
        
/* We consume these. 
EventQueue* incomingFileEvents;
EventQueue* incomingAcceptedEvents;
EventQueue* incomingRECVedEvents;
EventQueue* incomingSentEvents;
        
We produce these. 
 EventQueue* serverStartnStopReq;
 EventQueue* sendReq;
        
Data management structures. 
std::unordered_map<int, Connection*> connIDToConnectionMap;
std::unordered_map<int, int> sockfdToConnIDMap;
std::unordered_map<int, int> connToSockfdIDMap;
std::priority_queue <Event, std::vector<Event>, compareEvents> waitHeap;
std::priority_queue <Event, std::vector<Event>, compareEvents> expectedClients;
*/

void EventHandler::processAcceptEvents(long int now) {

      std::shared_ptr<Event> job;
      //out<< "EH:pae: Event handler TRYING TO GET JOB" << std::endl;
      while((*incomingAcceptedEvents).getEvent(job)){
	Event dispatchJob = *job;
	if (DEBUG)
	  (*out)<< "pae: Event handler GOT JOB " << EventNames[dispatchJob.type] <<" conn "<<dispatchJob.conn_id<<" event "<<dispatchJob.event_id<<std::endl;
	dispatch(dispatchJob, now);
      }
}

bool EventHandler::readyForEvent(long int connID, long int delta, long int now) {
   
    /* Make sure we have a valid socket for this event. */
    auto it0 = connToSockfdIDMap.find(connID);
    if(it0 == connToSockfdIDMap.end()) return false;

    /* Check if we're waiting on events. */
    auto it1 = connToWaitingToRecv.find(connID);
    if(it1 != connToWaitingToRecv.end()) {
        if(it1->second > 0) return false;
    }
    
    /* Check if we're waiting to send. */
    auto it2 = connToWaitingToSend.find(connID);
    if(it2 != connToWaitingToSend.end()) {
        if(it2->second > 0) return false;
    } 
    
    /* Check on the time. */
    auto it3 = connToLastCompletedEvent.find(connID);
    if(it3 != connToLastCompletedEvent.end()) {
        if(now - it3->second < delta) return false;
    }
    
    return true;
}

void EventHandler::newConnectionUpdate(int sockfd, long int connID, long int planned, long int now) {
  
  sockfdToConnIDMap[sockfd] = connID;
  connToSockfdIDMap[connID] = sockfd;
  connToWaitingToRecv[connID] = 0;
  connToWaitingToSend[connID] = 0;
  connToStalled[connID] = false;
  
  if (planned > 0)
    connToLastPlannedEvent[connID] = planned;
  else
    {
      int delay = (now - connToLastPlannedEvent[connID]);
      if (delay > 0)
	{
	  connToDelay[connID] += delay;	  
	  (*connStats)[connID].delay = connToDelay[connID];
	  (*out)<<" new data 1 for conn "<<connID<<" state "<<(*connStats)[connID].state<<std::endl;
	}
      connToLastPlannedEvent[connID] = now;
    }
  connToLastCompletedEvent[connID] = now;
  if (DEBUG)
    (*out)<<"Conn "<<connID<<" time now "<<now<<" planned time "<<connToLastPlannedEvent[connID]<<" delay "<< connToDelay[connID]<<std::endl;
}


void EventHandler::connectionUpdate(long int connID, long int planned, long int now) {

  if (planned > 0)
    connToLastPlannedEvent[connID] = planned;
  else
    {
      connToDelay[connID] += (now - connToLastPlannedEvent[connID]);
      connToLastPlannedEvent[connID] = now;      
      (*connStats)[connID].delay = connToDelay[connID];
      (*out)<<" new data 2 for conn "<<connID<<" state "<<(*connStats)[connID].state<<std::endl;
    }
  connToLastCompletedEvent[connID] = now;
  if (DEBUG)
    (*out)<<"EConn "<<connID<<" time now "<<now<<" planned time "<<connToLastPlannedEvent[connID]<<" delay "<< connToDelay[connID]<<std::endl;
}

#define MAXLEN 1000000

void EventHandler::dispatch(Event dispatchJob, long int now) {
    /* 	EventQueue* incomingFileEvents;
        --> OLD: EventQueue* incomingAcceptedEvents;
        EventQueue* incomingRECVedEvents;
        EventQueue* incomingSentEvents;
    */
  char buf[MAXLEN];

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  
  if (DEBUG)
    (*out)<<ms<<" EH: dispatch job type "<<EventNames[dispatchJob.type]<<" conn "<<dispatchJob.conn_id<<" value "<<dispatchJob.value<<" now "<<now<<std::endl;
    switch(dispatchJob.type) {
        /* We note these as events in our connection structure. */
        case ACCEPT: {
	  if (serverToCounter.find(dispatchJob.serverString) == serverToCounter.end())
	    serverToCounter[dispatchJob.serverString] = 0;
	  serverToCounter[dispatchJob.serverString]++;
	  if (DEBUG)
	    (*out)<<"Server "<<dispatchJob.serverString<<" connections "<<serverToCounter[dispatchJob.serverString]<<std::endl;
	  newConnectionUpdate(dispatchJob.sockfd, dispatchJob.conn_id, dispatchJob.ms_from_start, now);
	  connToServerString[dispatchJob.conn_id] = dispatchJob.serverString;
	  myPollHandler->watchForWrite(connToSockfdIDMap[dispatchJob.conn_id]);
	  if (DEBUG)
	    (*out)<<"PH will watch for write on "<<connToSockfdIDMap[dispatchJob.conn_id]<<" for conn "<<dispatchJob.conn_id<<std::endl;
	  break;
        }
        case WAIT: {
            break;
        }
        case RECV: {
	  (*out)<<" new data 29 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
	  connectionUpdate(dispatchJob.conn_id, dispatchJob.ms_from_start, now);
	  (*out)<<" new data 30 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
	  if (DEBUG)
	    (*out)<<"RECV JOB waiting to recv "<<connToWaitingToRecv[dispatchJob.conn_id]<<" on conn "<<dispatchJob.conn_id<<" job value "<<dispatchJob.value<<std::endl;
	   connToWaitingToRecv[dispatchJob.conn_id] = connToWaitingToRecv[dispatchJob.conn_id] + dispatchJob.value;

	   if (connToWaitingToRecv[dispatchJob.conn_id] <= 0)
	     {
	       (*connStats)[dispatchJob.conn_id].last_completed++;
	       (*out)<<" new data 3 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
	       connectionUpdate(dispatchJob.conn_id, 0, now);
	       getNewEvents(dispatchJob.conn_id);
	       if (DEBUG)
		 (*out)<<"For conn "<<dispatchJob.conn_id<<" last completed 7 "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
	       break;
	     }
	   while(connToWaitingToRecv[dispatchJob.conn_id] > 0)
	     {
	       if (DEBUG)
		 (*out)<<"Waiting for conn "<<dispatchJob.conn_id<<" b to recv "<<connToWaitingToRecv[dispatchJob.conn_id]<<std::endl;
	       try
		 {
		   int n = recv(dispatchJob.sockfd, buf, MAXLEN, 0);
		   int total = 0;
		   while (n == MAXLEN)
		     {
		       total += n;
		       n = recv(dispatchJob.sockfd, buf, MAXLEN, 0);
		     }
		   total += n;

		   if (total > 0)
		     {
		       if (DEBUG)
			 (*out)<<"RECVd 1 "<<total<<" bytes for conn "<<dispatchJob.conn_id<<std::endl;
		       connToWaitingToRecv[dispatchJob.conn_id] -= total;
		       // if (connToWaitingToRecv[dispatchJob.conn_id] < 0) // weird case
		       //connToWaitingToRecv[dispatchJob.conn_id] = 0;
		       if (DEBUG)
			 (*out)<<"RECV waiting now for "<<connToWaitingToRecv[dispatchJob.conn_id]<<" conn "<<dispatchJob.conn_id<<std::endl;
		       // Check if lower than 0 or 0 move new event ahead
		       
		       if (connToWaitingToRecv[dispatchJob.conn_id] <= 0)
			 {
			   (*connStats)[dispatchJob.conn_id].last_completed++;
			   (*out)<<" new data 4 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
			   if (DEBUG)
			     (*out)<<"For conn "<<dispatchJob.conn_id<<" last completed 8 "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
			   connectionUpdate(dispatchJob.conn_id, 0, now);
			   getNewEvents(dispatchJob.conn_id);		      
			   break;
			 }
		     }
		   else
		     {
		       if (DEBUG)
			 (*out)<<"Will wait to RECV "<<connToWaitingToRecv[dispatchJob.conn_id]<<" for conn "<<dispatchJob.conn_id<<" on sock "<<dispatchJob.sockfd<<std::endl;
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
            // From file events. We should dispatch this.
            break;
        }
        /* We handle the connection and update our socket<->connid maps. */
        case CONNECT: {
            /* Get our address. */
	  connToLastPlannedEvent[dispatchJob.conn_id] = dispatchJob.ms_from_start;
	  //auto it = connIDToConnectionMap->find(dispatchJob.conn_id);
	  if(true)
	    {
	      //it != connIDToConnectionMap->end()) {
	      connState[dispatchJob.conn_id] = CONNECTING;
	      (*connStats)[dispatchJob.conn_id].state = CONNECTING;
	      (*out)<<" new data 10 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
	      struct sockaddr_in caddr = getAddressFromString(dispatchJob.connString);
	      struct sockaddr_in saddr = getAddressFromString(dispatchJob.serverString);
	      int sockfd = getIPv4TCPSock((const struct sockaddr_in *)&caddr);
	      if (DEBUG)
		(*out)<<"Connecting on sock "<<sockfd<<" for conn "<<dispatchJob.conn_id<<" state "<<connState[dispatchJob.conn_id]<<std::endl;
	      if(connect(sockfd, (const struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == -1) {
		if (DEBUG)
		  (*out)<<"Didn't connect right away\n";
		if (errno != EINPROGRESS)
		  {
		    close(sockfd); // should return to pool and try later Jelena
		    char errmsg[200];
		    sprintf(errmsg, " connecting failed, conn %d src %s",dispatchJob.conn_id,dispatchJob.serverString.c_str());
		    perror(errmsg);
		    return;
		  }
		else
		  {
		    myPollHandler->watchForWrite(sockfd);
		    newConnectionUpdate(sockfd, dispatchJob.conn_id, dispatchJob.ms_from_start, now);
		  }
	      }
	      else
		{
		  connState[dispatchJob.conn_id] = EST;
		  newConnectionUpdate(sockfd, dispatchJob.conn_id, dispatchJob.ms_from_start, now);
		  (*connStats)[dispatchJob.conn_id].last_completed++;
		  (*out)<<" new data 11 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
		  if (DEBUG)
		    (*out)<<"Connected successfully 1 for conn "<<dispatchJob.conn_id<<" state is now "<<connState[dispatchJob.conn_id]<<" last completed 9 "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
		  getNewEvents(dispatchJob.conn_id);
		  }
	    }  
	      else {
                std::cerr << "Could not find connection info for connID " << dispatchJob.conn_id << std::endl;
                return;
	      }
            break;
        }
        /* Send thread handles these. */
    case SEND: {
      connectionUpdate(dispatchJob.conn_id, dispatchJob.ms_from_start, now);
      connToWaitingToSend[dispatchJob.conn_id] += dispatchJob.value;
      //if (DEBUG)
	(*out)<<"Handling SEND event waiting to send "<<connToWaitingToSend[dispatchJob.conn_id]<<" on sock "<<dispatchJob.sockfd<<std::endl;

      // Try to send

      while (connToWaitingToSend[dispatchJob.conn_id] > 0)
	{
	  if (DEBUG)
	    (*out)<<"Went into send for conn "<<dispatchJob.conn_id<<"\n";
	  try{
	    long int tosend = connToWaitingToSend[dispatchJob.conn_id];
	    if (tosend > MAXLEN)
	      tosend = MAXLEN;
	    if (DEBUG)
	      (*out)<<dispatchJob.conn_id<<" will try to send "<<tosend<<"\n";
	    int n = send(dispatchJob.sockfd, buf, tosend, 0);
	    if (DEBUG)
	      (*out)<<"n is "<<n<<"\n";
	    if (n < 0)
	      {
		myPollHandler->watchForWrite(dispatchJob.sockfd);
		if (DEBUG)
		  (*out)<<"Did not manage to send, but set write flag\n";
		break;
	      }
	    else
	      {
		connToWaitingToSend[dispatchJob.conn_id] -= n;
		if (DEBUG)
		  (*out)<<"Successfuly handled SEND event for conn "<<dispatchJob.conn_id<<" for "<<n<<" bytes\n";
	      }
	  }
	  catch(int e)
	    {
	      std::cerr<<"Errored out while sending for "<<dispatchJob.conn_id<<std::endl;
	      break;
	    }
	}
      //if (connToWaitingToSend[dispatchJob.conn_id] < 0)
      //connToWaitingToSend[dispatchJob.conn_id] = 0; // weird case
      if (connToWaitingToSend[dispatchJob.conn_id] == 0)
	{
	  (*connStats)[dispatchJob.conn_id].last_completed++;
	  (*out)<<" new data 12 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
	  if (DEBUG)
	    (*out)<<"For conn "<<dispatchJob.conn_id<<" last completed 1 "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
	}
      break;
	}
	  
      /* We handle these. */
    case SRV_START: {
      /* Check if the server is already started */
      if (srvStarted.find(dispatchJob.serverString) != srvStarted.end())
	{
	  if(strToConnID.find(dispatchJob.connString) == strToConnID.end())
	    {
	      strToConnID[dispatchJob.connString] = dispatchJob.conn_id;
	      if (DEBUG)
		(*out)<<"Associated conn "<<dispatchJob.conn_id<<" with "<<dispatchJob.connString<<std::endl;
	    }
	  break;
	}
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
	std::cerr << "ERROR: Failed to bind to " << servString << std::endl;
	return;
      }
      //if (DEBUG)
      //(*out)<<"Update listening socket "<<sockfd<<" for conn "<<dispatchJob.conn_id<<std::endl;
      //newConnectionUpdate(sockfd, dispatchJob.conn_id, dispatchJob.ms_from_start+SRV_UPSTART, now);
      sockfdToConnIDMap[sockfd] = -1; // Generic listening sock
      serverToSockfd[dispatchJob.serverString] = sockfd;
      if(listen(sockfd, MAX_BACKLOG_PER_SRV) == -1) {
	perror("Listen failed");
	return;
      }
      srvStarted[dispatchJob.serverString] = now;
      if (DEBUG)
	(*out)<<"Listening on sock "<<sockfd<<" for server "<<dispatchJob.serverString<<std::endl;
      myPollHandler->watchForRead(sockfd);
      break;
    }

    case SRV_STARTED: {

      // Server has already started, just note the connection
      if(strToConnID.find(dispatchJob.connString) == strToConnID.end())
	{
	  strToConnID[dispatchJob.connString] = dispatchJob.conn_id;
	  if (DEBUG)
	    (*out)<<"Associated conn "<<dispatchJob.conn_id<<" with "<<dispatchJob.connString<<std::endl;
	}
      // newConnectionUpdate(-1, dispatchJob.conn_id, dispatchJob.ms_from_start, now);
      break;
    }

    case CLOSE:{
      long int conn_id = dispatchJob.conn_id;
      // Check if we are ready
      if (connToWaitingToSend[conn_id] > 0  || connToWaitingToRecv[conn_id] > 0)
	{
	  if (DEBUG)
	    (*out)<<"Conn "<<conn_id<<" not ready, waiting to send "<<connToWaitingToSend[conn_id]<<" and to receive "<<connToWaitingToRecv[conn_id]<<std::endl;
	  // Not ready, return to queue
	  dispatchJob.ms_from_start += SRV_UPSTART;
	  eventsToHandle->addEvent(dispatchJob);
	}
      else
	{
	  if (DEBUG)
	    (*out)<<"Received CLOSE for conn "<<dispatchJob.conn_id<<" event "<<dispatchJob.event_id<<std::endl;
	  close(dispatchJob.sockfd);
	  (*connStats)[dispatchJob.conn_id].state = DONE;
	  (*connStats)[dispatchJob.conn_id].last_completed++;
	  (*out)<<" new data 13 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
	  if (DEBUG)
	    (*out)<<"For conn "<<dispatchJob.conn_id<<" last completed 2 "<<(*connStats)[dispatchJob.conn_id].last_completed<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;

	  if (connToServerString.find(dispatchJob.conn_id) != connToServerString.end())
	    {
	      if (DEBUG)
		(*out)<<" Deleting stats for conn "<<dispatchJob.conn_id<<"\n";
	      serverToCounter[connToServerString[dispatchJob.conn_id]] --;
	      connToServerString.erase(dispatchJob.conn_id); 
	      sockfdToConnIDMap.erase(dispatchJob.sockfd);
	      connToSockfdIDMap.erase(dispatchJob.conn_id);
	      connToWaitingToRecv.erase(dispatchJob.conn_id);
	      connToWaitingToSend.erase(dispatchJob.conn_id);
	      connToStalled.erase(dispatchJob.conn_id);
	      connToLastPlannedEvent.erase(dispatchJob.conn_id);
	    }
	  if (DEBUG)
	    (*out)<<"Closed sock "<<dispatchJob.sockfd<<" for conn "<<dispatchJob.conn_id<<" last completed "<<(*connStats)[dispatchJob.conn_id].last_completed<<std::endl;
	  (*out)<<" new data 15 for conn "<<dispatchJob.conn_id<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;
	}
      // Jelena: clean all the connection state here but not stats
      break;
    }
    case SRV_END: {
	  if (serverToCounter[dispatchJob.serverString] == 0)
	    {
	      if (DEBUG)
		(*out)<<"Stopping server "<<dispatchJob.serverString<<" time "<<now<<" sock "<<serverToSockfd[dispatchJob.serverString]<<std::endl;
	      close(serverToSockfd[dispatchJob.serverString]); // should account for delays in connections
	    }
	  // Try again after a while
	  else
	    {
	      if (DEBUG)
		(*out)<<"Would like to stop server "<<dispatchJob.serverString<<" time "<<now<<" sock "<<serverToSockfd[dispatchJob.serverString]<<" but counter is "<<serverToCounter[dispatchJob.serverString]<<std::endl;
	      dispatchJob.ms_from_start = now + SRV_UPSTART;
	      eventsToHandle->addEvent(dispatchJob);
	    }
	  break;
        }
        /* Not sure how we got here. */
        default: {
            break;
        }
    }
    if (DEBUG)
      (*out)<<"Getting out of dispatch\n";
    //dispatchJob.reset();
}

void EventHandler::storeConnections()
{
  for(const auto& pair:*connIDToConnectionMap) {
    long int connID = pair.first;
    bool success = false;
    std::string constring = getConnString(&(pair.second->src), &(pair.second->dst), &success);
    if(success) {
      strToConnID[constring] = connID;
      if (DEBUG)
	(*out)<< "Adding " << constring << ":" << connID << std::endl;
      constring.clear();
    }
    else {
      std::cerr << "Problem creating connection string for server map of connIDs->connection strings." << std::endl;
    }
    constring.clear();
  }
}

bool EventHandler::startup() {
  if (DEBUG)
    (*out)<<"Event handler starting\n";
  storeConnections();
  return true;
}

void EventHandler::checkStalledConns(long int now)
{
  // Go through conns and try to load more events if there are any
  for (auto it = connToSockfdIDMap.begin(); it != connToSockfdIDMap.end(); it++)
    {
      if (DEBUG)
	(*out)<<"Checking conn "<<it->first<<" waiting to send "<<connToWaitingToSend[it->first]<<" and to recv "<<connToWaitingToRecv[it->first]<<" state "<<connState[it->first]<<" stalled "<<connToStalled[it->first]<<std::endl;
      if (connToWaitingToSend[it->first] > 0)
	myPollHandler->watchForWrite(connToSockfdIDMap[it->first]);
      if (connToWaitingToSend[it->first] <= 0 &&  connToWaitingToRecv[it->first] <= 0 && connState[it->first] != DONE && connToStalled[it->first])
	{
	  getNewEvents(it->first);
	}
    }
}

void EventHandler::checkOrphanConns(long int now)
{
  // Go through conns and try to load more events if there are any
  for (auto it = orphanConn.begin(); it != orphanConn.end(); )
    {
      if (DEBUG)
	(*out)<<"Checking orphaned conn "<<it->first<<std::endl;
      auto sit = strToConnID.find(it->first);
      if (sit != strToConnID.end())
	{
	  long int conn_id = sit->second;
	  if (DEBUG)
	    (*out)<<"Found conn "<<conn_id<<" on socket "<<it->second<<" time "<<now<<std::endl;
	  connToLastPlannedEvent[conn_id] = now;
	  newConnectionUpdate(it->second, conn_id, 0, now);
	  connState[conn_id] = EST;
	  (*connStats)[conn_id].state = EST;
	  (*connStats)[conn_id].last_completed++;
	  (*out)<<" new data 16 for conn "<<conn_id<<" state "<<(*connStats)[conn_id].state<<std::endl;
	  getNewEvents(sit->second);
	  auto dit = it;
	  it++;
	  orphanConn.erase(dit);
	}
      else
	it++;
    }
}

void EventHandler::loop(std::chrono::high_resolution_clock::time_point startTime) {
  long int now = msSinceStart(startTime);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(startTime.time_since_epoch()).count();
  if (DEBUG)
    (*out)<<"EH: looping, start time is "<<ms<<" now is "<<now<<std::endl;
  // Allocate a really big buffer filled with a's
  char* buf = (char*)malloc(MAXLEN);
  memset(buf, 'a', MAXLEN);
  if (DEBUG)
  (*out)<<"EH: looping, incoming file events "<<incomingFileEvents<<"\n";
  if (DEBUG)
  (*out)<<"EH: Is running is "<<isRunning.load()<<std::endl;
  long int fileEvents = 0;
  bool requested;
  long int processedFileEvents = 0;
  int eventsHandled = 0;
  int idle = 0;
  int ITHRESH = 10;
  
  while(isRunning.load()) {

    fileEvents = incomingFileEvents->getLength();
    
    if (DEBUG)
      (*out)<<"There are "<<fileEvents<<" file events, processed "<<processedFileEvents<<std::endl;
    long int nextEventTime = incomingFileEvents->nextEventTime();
    long int thisChunk = 0;
    //(*out)<<"Next zevent time "<<nextEventTime<<" now "<<now<<std::endl;
    //(*out)<<"EH: Beginning of loop time " <<now<<std::endl;
    // Put a chunk of incomingFileEvents into connection-specific queues
    while(nextEventTime >= 0 && thisChunk <= 1000) { // was maxQueuedFileEvents
      eventsHandled++;
      std::shared_ptr<Event> job;
      //(*out)<< "EH: Event handler TRYING TO GET JOB" << std::endl;
      if(incomingFileEvents->getEvent(job)){
	thisChunk++;
	processedFileEvents++;
	if (processedFileEvents/fileEvents > 0.9)
	  {
	    requested = false;
	    processedFileEvents = 0;
	  }
	/* Check if we've processed a fair chunk (maxQueuedFileEvents/10 events) and	*/
	/* warn the FileWorker that it should top off the file event queue. 		*/
	Event dispatchJob = *job;
	if (DEBUG)
	  (*out)<< "File Event handler GOT JOB " << EventNames[dispatchJob.type] <<" serverstring "<<dispatchJob.serverString<<" conn "<<dispatchJob.conn_id<<" event id "<<dispatchJob.event_id<<" ms from start "<<dispatchJob.ms_from_start<<" now "<<now<<" value "<<dispatchJob.value<<" server "<<dispatchJob.serverString<<" left in queue "<<incomingFileEvents->getLength()<<std::endl;
	if (dispatchJob.type == SEND || dispatchJob.type == RECV || dispatchJob.type == CLOSE)
	  connToEventQueue[dispatchJob.conn_id].addEvent(dispatchJob);
	else
	  {
	    eventsToHandle->addEvent(dispatchJob);
	    if (DEBUG)
	      (*out)<<"Added job to eventsToHandle "<<std::endl;
	  }
	nextEventTime = incomingFileEvents->nextEventTime();
	if (DEBUG)
	  (*out)<< "EVENT HANDLER: Pulled " << thisChunk << " events. Next event time is " << nextEventTime << std::endl;
      }
      else {
	if (DEBUG)
	  (*out)<< "We think we have a job, but failed to pull it? " << std::endl;
      }
      job.reset();
    }
    if (DEBUG)
      (*out)<< "EVENT HANDLER: Next event time is: " << nextEventTime << " Now is " << now << std::endl; 
        
    if(fileEventsHandledCount > maxQueuedFileEvents/2) {
      fileEventsHandledCount = 0;
      std::unique_lock<std::mutex> lck(fileHandlerMTX);
      if (DEBUG)
	(*out)<< "Sending wake to fileWorker." << std::endl;
      loadMoreFileEvents = true;
      fileHandlerCV.notify_one();
      lck.unlock();
    }

    long int nextHeapEventTime = eventsToHandle->nextEventTime();
    (*out)<<"Next heap time "<<nextHeapEventTime<<" now "<<now<<std::endl;
    
  
	while(nextHeapEventTime <= now && nextHeapEventTime >= 0) {
	  Event dispatchJob = eventsToHandle->nextEvent();
	  eventsHandled++;
	  fileEventsHandledCount++;
	  if(true){ // this was if (bool = got a job)
	    if (DEBUG)
	      (*out)<< "Heap Event handler GOT JOB " << EventNames[dispatchJob.type] <<" server "<<dispatchJob.serverString<<" conn "<<dispatchJob.conn_id<<" event "<<dispatchJob.event_id<<" ms from start "<<dispatchJob.ms_from_start<<" now "<<now<<" value "<<dispatchJob.value<<" events handled "<<fileEventsHandledCount<<" state "<<(*connStats)[dispatchJob.conn_id].state<<std::endl;

                dispatch(dispatchJob, now);
                nextHeapEventTime = eventsToHandle->nextEventTime();
                //(*out)<< "EVENT HANDLER: Pulled " << fileEventsHandledCount << " events. Next event time is " << nextEventTime << std::endl;
            }
            else {
	      if (DEBUG)
                (*out)<< "We think we have a job, but failed to pull it? " << std::endl;
            }
	}

	// Should account for eventsHandled here too
        processAcceptEvents(now);
        now = msSinceStart(startTime);

	// Check if we should ask for more events
	// if we handled 1/10th of what is max for our thread
	
	//(*out)<<"Handled "<<fileEventsHandledCount<<" max "<<maxQueuedFileEvents<<" last event "<<lastEventCountWhenRequestingForMore<<" fehc "<<fileEventsHandledCount<<" left in queue "<<incomingFileEvents->getLength()<<std::endl;
	//if((fileEventsHandledCount > (maxQueuedFileEvents/10) || incomingFileEvents->getLength() < maxQueuedFileEvents/2) && requested == false) this works
	if (DEBUG)
	  (*out)<<"fileEventsHandledCount "<<fileEventsHandledCount<<" file events "<<fileEvents<<" incoming length "<<incomingFileEvents->getLength()<<" nextEventtime "<<nextEventTime<<std::endl;
	if((fileEventsHandledCount > fileEvents/2 || incomingFileEvents->getLength() < fileEvents/2 || nextEventTime < 0))//  && requested == false)
	  {//
	  //if((fileEventsHandledCount > (fileEvents/2) || incomingFileEvents->getLength() < maxQueuedFileEvents/2 && lastEventCountWhenRequestingForMore + fileEvents/10 < fileEventsHandledCount)) {
	  lastEventCountWhenRequestingForMore += fileEventsHandledCount;
	  if (DEBUG)
	    (*out)<<"requesting more events, last "<<lastEventCountWhenRequestingForMore<<" file events "<<fileEvents<<" handled "<<fileEventsHandledCount<<" comparisong between "<<fileEventsHandledCount<<" and "<<fileEvents/2<<" is "<<(fileEventsHandledCount > fileEvents/2)<<" requested is "<<requested<<std::endl;
	  fileEventsHandledCount = 0;
	  requested = true;
	  if (DEBUG)
	    (*out)<<"sending signal "<<std::endl;
	  requestMoreFileEvents->sendSignal();
	  if (DEBUG)
	    (*out)<<"signal sent "<<std::endl;
	}

        /* If the last time we checked the time in the events queue it was empty, redo our check now. */
	// Check epoll events.
        int timeout = 1;
	
	if (DEBUG)
	  (*out)<<"Checking poll handler with timeout "<<timeout<<std::endl;
	
        myPollHandler->waitForEvents(timeout);
        
        /* Handle any events from poll. Could be 			*/
        /*    - a notification from send or recv threads.		*/
        /*    - a new connection to our server socket.			*/
        struct epoll_event *poll_e = (struct epoll_event*) calloc(1, sizeof(struct epoll_event));
        while(myPollHandler->nextEvent(poll_e)) {
            // XXX Handle notifications.
	  /* Figure out what we want to do with this event */
	  eventsHandled++;
	  int fd = poll_e->data.fd;
	  long int conn_id = sockfdToConnIDMap[fd];
	  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	  if (DEBUG)
	    (*out)<<ms<<" Got event on sock "<<fd<<" w flags "<<poll_e->events<<" epoll in "<<EPOLLIN<<" out "<<EPOLLOUT<<" on conn "<<conn_id<<std::endl;
	  if (conn_id == -1 && ((poll_e->events & EPOLLIN) > 0))
	    {
	      if (DEBUG)
	      (*out)<<"EH got ACCEPT event and should accept connection\n";
	      /* New connection to one of our servers. */
	      /* it could be more than one ACCEPT */
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
		      connState[conn_id] = EST;
		      (*connStats)[conn_id].state = EST;
		      (*connStats)[conn_id].last_completed++;
		      (*out)<<" new data 5 for conn "<<conn_id<<" state "<<(*connStats)[conn_id].state<<std::endl;
		      if (DEBUG)
			(*out)<<"For conn "<<conn_id<<" State is now "<<connState[conn_id]<<" last completed 3 "<<(*connStats)[conn_id].last_completed<<std::endl;
		      getNewEvents(conn_id);
		    }
		};
	      continue;
	    }
	  if (connState[conn_id] == CONNECTING) // && (poll_e->events & EPOLLOUT > 0))
	    {
	      // Check if we errored out
	      if ((poll_e->events & EPOLLHUP) || (poll_e->events & EPOLLERR))
		{
		  // Give up on this conn somehow, Jelena
		  continue;
		}
	      // Check for error if (getsockopt (socketFD, SOL_SOCKET, SO_ERROR, &retVal, &retValLen) < 0)
	      // ERROR, fail somehow, close socket
	      //if (retVal != 0) 
	      // ERROR: connect did not "go through"
	      newConnectionUpdate(fd, conn_id, 0, now);
	      connState[conn_id] = EST;
	      (*connStats)[conn_id].state = EST;
	      (*connStats)[conn_id].last_completed++;
	      (*out)<<" new data 6 for conn "<<conn_id<<" state "<<(*connStats)[conn_id].state<<std::endl;
	      if (DEBUG)
		(*out)<<"Connected successfully, conn "<<conn_id<<" state is now "<<connState[conn_id]<<" last completed 4 "<<(*connStats)[conn_id].last_completed<<std::endl;
	      getNewEvents(conn_id);
	      continue;
	   }
	  if (connState[conn_id] == EST && ((poll_e->events & EPOLLOUT) > 0))
	    {
	      if ((poll_e->events & EPOLLHUP) || (poll_e->events & EPOLLERR))
		{
		  // Give up on this conn somehow, Jelena
		  continue;
		}
	      int len = connToWaitingToSend[conn_id];
	      if (DEBUG)
	      (*out)<<"EH possibly got SEND event for conn "<<conn_id<<" flags "<<poll_e->events<<" epollout "<<EPOLLOUT<<" comparison "<<((poll_e->events & EPOLLOUT) > 0)<<" should send "<<len<<std::endl;
	      /* New connection to one of our servers. */
	      if (len > 0)
		{
		  if (DEBUG)
		  (*out)<<"Waiting to send "<<connToWaitingToSend[conn_id]<<" on socket "<<fd<<std::endl;
		  try
		    {
		      int n = send(fd, buf, len, 0);
		      if (n > 0)
			{
			  if (DEBUG)
			    (*out)<<"Successfully handled SEND for conn "<<conn_id<<" for "<<n<<" bytes\n";
			  connToWaitingToSend[conn_id] -= n;
			  if (connToWaitingToSend[conn_id] > 0)
			    {
			      if (DEBUG)
				(*out)<<"Still have to send "<<connToWaitingToSend[conn_id]<<" bytes\n";
			      myPollHandler->watchForWrite(fd);
			    }
			  else
			    {
			      connectionUpdate(conn_id, 0, now);
			      (*connStats)[conn_id].last_completed++; // here we could remember the event id instead of count
			      (*out)<<" new data 7 for conn "<<conn_id<<" state "<<(*connStats)[conn_id].state<<std::endl;
			      if (DEBUG)
				(*out)<<"For conn "<<conn_id<<" last completed 5 "<<(*connStats)[conn_id].last_completed<<std::endl;
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
	  if (connState[conn_id] == EST && ((poll_e->events & EPOLLIN) > 0))
	    {
	      if ((poll_e->events & EPOLLHUP) || (poll_e->events & EPOLLERR))
		{
		  // Give up on this conn somehow, Jelena, try to receive because EPOLLIN was set
		  continue;
		}
	      if (DEBUG)
		(*out)<<"Possibly handling a RECV event for conn "<<conn_id<<" on sock "<<fd<<std::endl;
	      try
		{
		   int n = recv(fd, buf, MAXLEN, 0);
		   int total = 0;
		   while (n == MAXLEN)
		     {
		       total += n;
		       n = recv(fd, buf, MAXLEN, 0);
		     }
		   total += n;

		   if (DEBUG)
		     (*out)<<"RECVd 2 "<<total<<" bytes for conn "<<conn_id<<std::endl;

		  if (total > 0)		
		    {
		      long int waited = connToWaitingToRecv[conn_id];
		      connToWaitingToRecv[conn_id] -= total;

		      if (DEBUG)
			(*out)<<"RECV waiting now for "<<connToWaitingToRecv[conn_id]<<" on conn "<<conn_id<<std::endl;

		      if (connToWaitingToRecv[conn_id] == 0 ||
			  (connToWaitingToRecv[conn_id] < 0 && waited > 0))
			{		     
			  connectionUpdate(conn_id, 0, now);
			  (*connStats)[conn_id].last_completed++; // here we could remember the event id instead of count Jelena check
			  (*out)<<" new data 8 for conn "<<conn_id<<" state "<<(*connStats)[conn_id].state<<std::endl;
			  if (DEBUG)
			    (*out)<<"For conn "<<conn_id<<" last completed 6 "<<(*connStats)[conn_id].last_completed<<std::endl;
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
	if (DEBUG)
	  (*out)<<"Checking stalled conns "<<std::endl;
	checkStalledConns(now);
	checkOrphanConns(now);
	if (eventsHandled == 0)
	  {
	    idle++;
	    if (idle > ITHRESH)
	      {
		usleep(ITHRESH*1000);
	      }
	  }
	else
	  idle = 0;
	if (DEBUG)
	  (*out)<< "Relooping, time now " <<now<<" events handled "<<eventsHandled<< std::endl;
	eventsHandled = 0;
  }
}

void EventHandler::getNewEvents(long int conn_id)
{
  EventHeap* e = &connToEventQueue[conn_id];
  int nextEventTime = e->nextEventTime();
  /* Jelena
  char myName[SHORTLEN], filename[MEDLEN];
  gethostname(myName, SHORTLEN);
  sprintf(filename, "connstats.%s.%d.txt", myName, conn_id);
  std::ofstream myfile;
  myfile.open(filename, std::ios_base::app);
  e->printToFile(myfile);
  myfile.close();*/
  // Jelena
  if (DEBUG)
    (*out)<<"Getting new events for conn "<<conn_id<<" next event time is "<<nextEventTime<<" state "<<(*connStats)[conn_id].state<<std::endl;


  if (nextEventTime >= 0) // ||  (*connStats)[conn_id].state == DONE)
    connToStalled[conn_id] = false;
  else //if (nextEventTime < 0 &&  (*connStats)[conn_id].state != DONE)
    connToStalled[conn_id] = true;
  
  while (nextEventTime >= 0)
    {
      Event job = e->nextEvent();
      job.sockfd = connToSockfdIDMap[conn_id];
      if (DEBUG)
	(*out)<< "Event handler moved new JOB " << EventNames[job.type] <<" conn "<<job.conn_id<<" event "<<job.event_id<<" for time "<<job.ms_from_start<<" to send "<<job.value<<" now moved to time "<<(job.ms_from_start+connToDelay[conn_id])<<" because of delay "<<connToDelay[conn_id]<<std::endl;
      job.ms_from_start += connToDelay[conn_id];
      eventsToHandle->addEvent(job);
      nextEventTime = e->nextEventTime();
      if (nextEventTime < 0)
	{
	  /*
	  Event job;
	  job.sockfd = connToSockfdIDMap[conn_id];
	  job.ms_from_start = (*connTime)[conn_id] + connToDelay[conn_id] + 1;
	  job.type = CLOSE;
	  job.conn_id = conn_id;
	  job.event_id = -1;
	  job.value = -1;
	  job.ms_from_last_event = 0;
	  eventsToHandle->addEvent(job);
	  (*connStats)[conn_id].total_events++;
	  */
	  connToStalled[conn_id] = true;
	  return;
	}
      if (job.type == RECV)
	break;
    }
  // Here we could perhaps close the connection if we're out of the events Jelena
}

long int EventHandler::acceptNewConnection(struct epoll_event *poll_e, long int now) {
    int newSockfd = -1;
    struct sockaddr in_addr;
    int in_addr_size = sizeof(in_addr);
    int fd = poll_e->data.fd;
    
    /* Accept new connection. */ 
    newSockfd = accept(fd, &in_addr, (socklen_t*)&in_addr_size);
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
    /* Set nonblocking. */
    int status = 0;
    status = setIPv4TCPNonBlocking(newSockfd);
    if (DEBUG)
      (*out)<<"EH setting nonblocking on socket "<<newSockfd<<std::endl;
    if(status < 0) {
        return -1;
    }
    
    /* Now figure out which connection we accepted. */
    /* Get info on the server socket we accepted on. */
    struct sockaddr sa_srv;
    unsigned int sa_len;
    sa_len = sizeof(sa_srv);
    
    // XXX We assume this is IPv4/TCP for now.
    if(getsockname(fd, (sockaddr *)&sa_srv, (unsigned int *)&sa_len) == -1) {
        perror("getsockname() failed");
        return -1;
    }
    bool success = false;
    // XXX We assume this is IPv4/TCP for now.
    std::string connString = getConnString((const struct sockaddr_in *)&in_addr, (const struct sockaddr_in*)&sa_srv, &success);
    if(!success) return -1;

    if (DEBUG)
      (*out)<< "Got connection from: " << connString << std::endl;

    /* Map names to a conn. */
    auto it = strToConnID.find(connString);
    if(it == strToConnID.end()) {
      (*out) << "Got connection but could not look up connID." << std::endl;

      orphanConn[connString] = newSockfd;
      return -2; 
    }
    long int conn_id = it->second;
    
    /* Update our data structures. */
    connToLastPlannedEvent[conn_id] = now;
    newConnectionUpdate(newSockfd, conn_id, 0, now);
    /* XXX Add this to the watched sockets for reads. */
    if (DEBUG)
    (*out)<<"Updated new sock "<<newSockfd<<" for connection "<<conn_id<<std::endl;
    return conn_id; // Jelena    
}

EventHandler::EventHandler(EventNotifier* loadMoreNotifier, std::unordered_map<long int, long int>* c2time, std::unordered_map<std::string, long int>* l2time, EventQueue* fe, EventQueue* ae, EventQueue* re, EventQueue* se, EventQueue * outserverQ, EventQueue * outSendQ, ConnectionPairMap* ConnMap, std::map<long int, struct stats>* cs, int id, bool debug, std::string myname) {

    fileEventsHandledCount = 0;
    lastEventCountWhenRequestingForMore = 0;
    out = new std::ofstream(myname);
    myID = id;
    
    connIDToConnectionMap = ConnMap;
    incomingFileEvents = fe;
    requestMoreFileEvents = loadMoreNotifier;
    incomingAcceptedEvents = ae;
    incomingRECVedEvents = re;
    incomingSentEvents = se;
    serverStartnStopReq = outserverQ;
    sendReq = outSendQ;
    connStats = cs;
    DEBUG = debug;

    srvStarted = {};
    sockfdToConnIDMap = {};
    connToSockfdIDMap = {};
    connToStalled = {};
    connToEventQueue = {};
    connState = {};
    connToWaitingToRecv = {};
    connToWaitingToSend = {};
    connToLastCompletedEvent = {};
    serverToCounter = {};
    connToServerString = {};
    connTime = c2time;
    listenerTime = l2time;
    
    eventsToHandle = new EventHeap();
    myPollHandler = new PollHandler(DEBUG);

}	

EventHandler::~EventHandler() {
}


/* For printing/logging only. 
std::string Connection::dstAddr() {
    socklen_t len;
    struct sockaddr addr;
    int port;
    int PORT_MAX_LEN = 5;
    char ipstr[INET6_ADDRSTRLEN];

    len = sizeof addr;
    if(getpeername(sockfd, (struct sockaddr*)&addr, &len) == -1) {
        return std::string("");
    }

    if (addr.sa_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    } 
    else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }

    std::ostringstream addrStream;
    addrStream << ipstr << ":" << port;

    std::string addrStr = addrStream.str();
    return addrStr;
}

*/

