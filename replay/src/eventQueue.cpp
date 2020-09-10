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
#include "mimic.h"


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

//class EventQueue {
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
        EventQueue::~EventQueue() {
            while(first != nullptr) {
                eventJob * tmp = first;
                first = tmp->next;
                delete tmp;
            }
        }
        int EventQueue::cleanUp() {
            while(first != divider.load()) {
                eventJob * tmp = first;
                first = first->next;
		//if (DEBUG)
		//std::cout << "Use count of epointer before removal of job" << tmp->eptr.use_count() << "\n";
                tmp->eptr.reset();
                delete tmp; 
                numEvents--;
            }
            return numEvents;
        }

        int EventQueue::getLength() {
	  return numEvents;
        }

        void EventQueue::addEvent(std::shared_ptr<Event> e) {
            eventJob * lastNode = last.load();
            lastNode->next = new eventJob(e);
            
            last.store(lastNode->next);
            numEvents++;
            cleanUp();
	    //if (DEBUG)
	    //std::cout << qName << ":Num events: " << numEvents << "\n";
        }
        
        
        /* The below two functions should only be called by the consumer. */
        bool EventQueue::getEvent(std::shared_ptr<Event>& job) {
            eventJob * dividerNode = divider.load();
            if(dividerNode != last.load()) {
                //job = divider->next->eptr;
                if (dividerNode->next != nullptr) {
                    job = dividerNode->next->eptr;
                    //divider = divider->next;
                    divider.store(dividerNode->next);
		    //if (DEBUG)
		    //std::cout << qName << "JOB REMOVED" << std::endl;
                    return true;
                }
            }
            return false;
        }
        long int EventQueue::nextEventTime() {
            eventJob * dividerNode = divider.load();
            if(dividerNode != last.load()) {
                if(dividerNode->next != nullptr) {
                    long int t = dividerNode->next->eptr->ms_from_start;
                    return t;
                }
            }
            //std::cout << qName << "Time checked. " << std::endl;
            return -1;
        }
//}


EventHeap::EventHeap() {
}
EventHeap::~EventHeap() {
}

void EventHeap::addEvent(Event e) {
    eventHeap.push(e);
}

long int EventHeap::nextEventTime() {
    if(eventHeap.empty() == true) {
        return -1;
    }
    long int t = eventHeap.top().ms_from_start;
    return t;
}

Event EventHeap::nextEvent() {
    Event e = eventHeap.top();
    eventHeap.pop();
    return e;
}

int EventHeap::getLength() {
  return eventHeap.size();
}

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





bool setNonBlocking(int sockfd) {
    int status = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
    if(status == -1) {
        return false;
    }
    return true;
}

int getSocket(DomainType domain, TransportType type, const struct sockaddr *localAddr) {
    int sockfd = -1;
    int d;
    if(domain == IPV4) d = AF_INET;
    else d = AF_INET6;
    if(type == TCP) {
        sockfd = socket(d, SOCK_STREAM, 0);
    }
    else {
        std::cout << "Non-TCP not yet supported.\n";
        sockfd = -1;
    }
    if(sockfd == -1) return -1;
    if(setNonBlocking(sockfd)) return sockfd;
    return -1;
}

bool serverUp(int sockfd) {
    return true;
}

bool connectToServer(int sockfd) {
    return true;
}
