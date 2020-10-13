#include "eventNotifier.h"


int createEventFD() {
  unsigned int val = 0;
  int fd = eventfd(val, O_NONBLOCK);
  if(fd == -1) {
    std::cerr << strerror(errno) << std::endl;
    return -1;
  }
  return fd;
}


EventNotifier::EventNotifier(int myfd, std::string name) {
  fd = myfd;
  myName = name;
}

EventNotifier::~EventNotifier() {
}

int EventNotifier::myFD() {
  return fd;
}

bool EventNotifier::isMe(int readfd) {
  if(readfd == fd) return true;
  return false;
}

bool EventNotifier::sendSignal() {
  uint64_t value = 1;
  int i = write(fd, &value, sizeof(value));  
  if( i != sizeof(value)) {
    return false;
  }
  return true;
}

bool EventNotifier::readSignal() {
  uint64_t value; 
  int i = read(fd, &value, sizeof(value));
  if(i != 1) {
    return false;
  }
  return true;
}


