#ifndef EVENTHANDLER_H
#define EVENTHANDLER_H 
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
#include <map> 
#include <unordered_map>
#include <unordered_set>
#include "eventQueue.h"
#include "eventNotifier.h"
#include "pollHandler.h"
#include "mimic.h"

struct connData
{
  std::string serverString = "";
  int sockfd = -1;
  long int waitingToRecv = 0;
  long int waitingToSend = 0;
  long int delay = 0;
  long int lastPlannedEvent = 0;
  EventHeap eventQueue;
  long int time = 0;
  enum conn_state state = INIT;
  bool stalled = false;
};


class EventHandler {
    private:
        /* We start 3 threads */
        /*	- a server thread (takes in start/stop req, produces accepted events.)  */
        /*	  out: client/serv addr (sockfd) map: addrs->connid, add sockfd		*/
        /* 	- a recv thread (produces events of how much is received from each socket.) */
        /*	  out: sockfd (value) map: sockfd->connid, add value		*/
        /* 	- a send thread (takes in send/connect req, produces sent event). 	*/
        /* 	  out: connid (value) map: none, add value		*/

        int myID;
        long int fileEventsHandledCount;       
        long int lastEventCountWhenRequestingForMore;

        /* We listen to eventNotifiers and listen as servers for clients. */
        PollHandler* myPollHandler;

        /* We consume these. */
        EventNotifier* requestMoreFileEvents;
        EventQueue* incomingFileEvents;
        EventQueue* incomingAcceptedEvents;
        EventQueue* incomingRECVedEvents;
        EventQueue* incomingSentEvents;
        
        /* We produce these. */
        EventQueue* serverStartnStopReq;
        EventQueue* sendReq;
	EventHeap* eventsToHandle;

	/* For logging */
	std::ofstream* out;
	
        /* Data management structures. */
        ConnectionPairMap * connIDToConnectionMap;
        stringToConnIDMap strToConnID;
	std::unordered_map<long int, connData> myConns;
        std::unordered_map<int, long int> sockfdToConnIDMap;
	std::unordered_map<std::string, long int> serverToSockfd;
	std::unordered_map<std::string, long int> serverToCounter;
	std::unordered_map<std::string, long int>* listenerTime;
	std::unordered_map<std::string, long int> srvStarted;
	std::unordered_map<std::string, long int> orphanConn;
	std::map<long int, struct stats>* connStats;
	
        EventHeap waitHeap;

        void processAcceptEvents(long int);
	void storeConnections();        
        void processFileEvents();
        void addWait();
        void dispatch(Event dispatchJob, long int now);
        void newConnectionUpdate(int sockfd, long int connID, long int planned, long int now);
	void connectionUpdate(long int connID, long int planned, long int now);
	long int acceptNewConnection(struct epoll_event *poll_e, long int now);
	void getNewEvents(long int conn_id);
	void checkStalledConns(long int now);
	void checkOrphanConns(long int now);
	void checkQueues();
	bool DEBUG = 0;
	
    public:
        EventHandler(EventNotifier* loadMoreNotifier, std::unordered_map<long int, long int>* c2time, std::unordered_map<std::string, long int>* l2time, EventQueue* fe, EventQueue* ae, EventQueue* re, EventQueue* se, EventQueue * outserverQ, EventQueue * outSendQ, ConnectionPairMap* ConnMap, std::map<long int, struct stats>* cs, int id, bool debug, std::string myname);
        ~EventHandler();
        bool startup();
        void loop(std::chrono::high_resolution_clock::time_point startTime);
};

#endif
