#ifndef EVENTNOTIFIER_H
#define EVENTNOTIFIER_H 
#include <unistd.h>
#include <sys/eventfd.h>
#include "mimic.h"

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