#ifndef MIMIC_H
#define MIMIC_H

#include <ctime>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <string>
#include <condition_variable>
#include "connections.h"

#define MAX_BACKLOG_PER_SRV 1000


/* Mutexes and condition variables. */
extern std::mutex fileHandlerMTX;
extern std::mutex statsMTX;
extern long int global_throughput;
extern long int global_events;
extern std::condition_variable fileHandlerCV;
extern bool loadMoreFileEvents;
bool returnLoadMoreFileEvents();

#define maxQueuedFileEvents 100000

#define SRV_UPSTART 4000
#define SRV_GAP 10000

#define SHORTLEN 150
#define MEDLEN 255

extern std::atomic<bool> isRunning;
extern std::atomic<bool> isInitd;
extern std::atomic<int> numThreads;
extern std::atomic<int> numconns;
extern std::atomic<int> numbytes;
extern std::atomic<int> numevents;
extern std::atomic<bool> isServer;
extern std::atomic<bool> makeup;

static std::string EventNames[14] = {"ACCEPT","ACCEPTED", "CONNECT", "CONNECTED", "CLOSE", "RECV", "RECVD", "WAIT", "SEND", "SENT", "SRV_START", "SRV_STARTED", "SRV_END", "NONE"};

enum EventType {
                ACCEPT, 	/* We should expect to accept a connection from a client. */
                ACCEPTED, 	/* We accepted a connection from a client. */
                CONNECT,	/* As a client, connect to a server. */
                CONNECTED, 	/* We successfully connected to a server. */
                CLOSE,		/* Socket should be closed or is closed.	*/
                RECV, 		/* Wait for {value}bytes of  data. */
                RECVD, 		/* Accepted {value}bytes of data. */
                WAIT,		/* Wait {value}ms before next event. */
                SEND, 		/* Send {value}bytes of data. */
                SENT,		/* We successfully sent {value}bytes of data. */
                SRV_START,	/* Bring up a server. */ 
                SRV_STARTED, 	/* Server has been started. */
                SRV_END, 	/* Bring down a server. */
                NONE		/* Dummy value. */
                };

enum TransportType {
                TCP,
                UDP
                };
            
enum DomainType {
    IPV4,
    IPV6
};

  

class Event {
    public:
        std::string serverString = "";
	std::string connString = "";
        int sockfd = -1;
        long int ms_from_start = 1;	 /* Keep track of event time relative to start in ms. */
        long int ms_from_last_event = -1;
        EventType type = NONE;        
        long int conn_id = -1;
        long int event_id = -1;
        long int value = 0;		/* What this value holds depends on what type of event this is. */
	Event();
	Event(std::string ss, std::string cs, int fd, long int mfs, long int mfle, EventType t, long int cid, long int eid, long int v);
};


long int msSinceStart(std::chrono::high_resolution_clock::time_point startTime);

#endif



