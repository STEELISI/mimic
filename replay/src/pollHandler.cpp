#include "pollHandler.h"

PollHandler::PollHandler(bool debug) {  
  /* Get our epoll fd. */
  DEBUG = debug;
  epollfd = epoll_create1(0);
  if(epollfd == -1) { 	 
    throw std::runtime_error("Failure to create epoll.");
  }
}

PollHandler::~PollHandler() {
}

void PollHandler::watchForRead(int fd) {
  if (DEBUG)
    std::cout<<"PH: watching for read on "<<fd<<std::endl;
  watch(fd, READ);
  fdsToWatchForRead.insert(fd);
}

int PollHandler::rssize()
{
  return fdsToWatchForRead.size();
}


bool PollHandler::checkForRead(int fd)
{
  return (fdsToWatchForRead.find(fd) != fdsToWatchForRead.end()) ;
}


void PollHandler::watchForWrite(int fd) {
  /* Should ONLY be called if we have attempted to write the socket, 	*/
  /* and got EAGAIN! Should always remove write watch after. 		*/
  watch(fd, WRITE);
}


void PollHandler::watch(int fd, epollWatchType type) {
  /* Set up epoll event structure. */
  struct epoll_event event = {0};
  event.data.fd = fd;
  
  /* Are we looking for incoming data? Or the ability to write to an EAGAIN fd? */
  if(type == READ)
    event.events = EPOLLIN |  EPOLLET;
  else if(type == WRITE)
    /* If we're watching for a write, don't rearm the fd after an event. */
    event.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;
  //event.events = EPOLLOUT | EPOLLET;  
  if (DEBUG)
    std::cout<<"Added watch for "<<fd<<" for event type "<<type<<std::endl;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == -1) {
  
    /* We might have failed because we're already watching this fd. */
    if(errno == EEXIST) {
      if(epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) != -1)
        return;
    }
    
    /* We've failed to watch this fd, fail silently*/
    //std::cerr << "Unable to add fd to epoll to watch for read." <<fd<<" error "<<errno<<" no mem "<<ENOMEM<<std::endl;
  }
  
  return;
}

void PollHandler::stopWatching(int fd) {
  if(epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) != -1) return;
  perror("Removing fd from epoll.");
  fdsToWatchForRead.erase(fd);
}


int PollHandler::waitForEvents(int timeout) {
  /* Wait for events, or timeout. 				*/
  /* Timeout is by default -1, so we'll wait for an event. 	*/
  eventIndex = 0;
  currentEventCount = epoll_wait(epollfd, events, MAX_EPOLL_EVENTS, timeout);
  return currentEventCount;
}       

bool PollHandler::nextEvent(struct epoll_event *e) {
  /* If we have no more events from this last return from epoll_wait, return false. */
  if(currentEventCount == 0) return false;

  /* If we've still got event, copy the next event from the events array to the passed in pointer. */
  memcpy(e, events + eventIndex, sizeof(struct epoll_event));

  /* We have EPOLLONESHOT with writes, so if we're also watching for reads on this fd, we need to rearm. */
  if(e->events & EPOLLOUT) {
    if(fdsToWatchForRead.find(e->data.fd) != fdsToWatchForRead.end()) {
      watchForRead(e->data.fd);
    }
  }

  /* Update our array index and our current event count. */
  eventIndex++;
  currentEventCount--;
  return true;
}
