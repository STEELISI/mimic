#ifndef POLLHANDLER_H
#define POLLHANDLER_H  
#include <unistd.h>
#include <set>
#include "mimic.h"

#define MAX_EPOLL_EVENTS 1000


enum epollWatchType {
  READ,
  WRITE
};

class PollHandler {
  private:
    std::set<int> fdsToWatchForRead;
    int epollfd;
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int eventIndex = 0;    
    int currentEventCount = 0;
    void watch(int fd, epollWatchType type);
    bool DEBUG = false;
    
  public:
    PollHandler(bool debug);
    ~PollHandler();
    int rssize();
    bool checkForRead(int fd);
    void watchForRead(int fd);
    void watchForWrite(int fd);
    void stopWatching(int fd);
    int waitForEvents(int timeout=-1);
    bool nextEvent(struct epoll_event *e);
};

#endif

