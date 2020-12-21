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

/* This is the main program that drives the rest */

#include <atomic>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "eventQueue.h"
#include "eventHandler.h"
#include "fileWorker.h"
#include "connections.h"

// We synchronize on this port
#define SRV_PORT 5205

// Some global flags and constants
std::atomic<bool> isRunning = false;
std::atomic<bool> isInitd = false;
std::atomic<int> numThreads = 5;
std::atomic<int> numconns = 1;
std::atomic<int> numbytes = 100;
std::atomic<int> numevents = 1;
std::atomic<bool> makeup = false;
std::atomic<bool> isServer = false;
std::string servString = "";
std::string serverIP = "";
bool loadMoreFileEvents = true;
auto startTime = 0;
long int global_throughput;
long int global_events;
char logDir[MEDLEN];
std::map<long int, struct stats> connStats;

// Sync semaphores
std::mutex fileHandlerMTX;
std::mutex statsMTX;
std::condition_variable fileHandlerCV;

// Function that prints statistics periodically and at the end
void print_stats(int flag)
{
  std::ofstream myfile;

  if (flag)
    {
      char myName[SHORTLEN], filename[MEDLEN];
      gethostname(myName, SHORTLEN);
      sprintf(filename, "%s/connstats.%s.txt", logDir, myName);
      std::cout<<"Logs are at "<<filename<<std::endl;
      myfile.open(filename);
    }
  int completed = 0, total = 0;
  int delay = 0;
  for (auto it=connStats.begin(); it != connStats.end(); it++)
    {
      total++;
      if (flag)
	myfile<<"Conn "<<it->first<<" state "<<csNames[it->second.state]<<" total events "<<it->second.total_events<<" last event "<<it->second.last_completed<<" delay "<<it->second.delay/1000.0<<" s, started "<<it->second.started/1000.0<<" completed "<<it->second.completed/1000.0<<std::endl;
      if (it->second.state == DONE)
	  completed++;
      
      delay += it->second.delay;
    }
  double avgd;
  if (total > 0)
    avgd = (double)delay/total;
  else
    avgd = 0;
  long int t, e;
  statsMTX.lock();
  t = global_throughput;
  e = global_events;
  global_throughput = 0;
  global_events = 0;
  statsMTX.unlock();
  
  std::cout<<"Successfully completed "<<completed<<"/"<<total<<" avg delay "<<avgd<<" ms, throughput "<<t*8/1000000000.0<<" Gbps, events "<<e<<" flag "<<flag<<std::endl;
  if (flag)
    myfile.close();
}

// Call this handler on CTRL-C
void signal_callback_handler(int signum) {
  isRunning.store(false);
  print_stats(true);
  exit(signum);
}

// Wait for client to contact us so we can sync
void waitForPeer()
{
  struct sockaddr_in servaddr;
  int sockfd;
  
  // socket create and verification 
  sockfd = socket(AF_INET, SOCK_STREAM, 0); 
  if (sockfd == -1) { 
    std::cerr<<"socket creation failed...\n";
    exit(0); 
  } 

  servaddr = getAddressFromString(servString);
  
  // connect the client socket to server socket 
  while(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) { 
    std::cerr<<"connection with the server failed...\n";
    sleep(1);
  }
  char buf[2];
  int n = recv(sockfd, buf, 3, 0);
  if (n <= 0 || strcmp(buf,"OK"))
    exit(0);
  close(sockfd);
}

// Contact the server once we've read the events
void informPeer()
{
  struct sockaddr_in servaddr;

  struct sockaddr cliaddr;
  int in_addr_size = sizeof(cliaddr);
  
  servString="0.0.0.0:"+std::to_string(SRV_PORT);

  servaddr = getAddressFromString(servString);

  // Get an ordinary,blocking socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
      std::cerr<<"socket creation failed...\n";
      exit(0); 
    } 

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    
    // Binding newly created socket to given IP and verification
    int b = bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (b != 0) { 
      perror("socket bind failed...\n"); 
      exit(0); 
    } 


  if(listen(sockfd, MAX_BACKLOG_PER_SRV) == -1) {
    perror("Listen failed");
    return;
  }
  std::cout<<"Server is listening\n";
  

  // Accept one client, send OK and close
  // Later we can add more clients
  int newSockfd = accept(sockfd, &cliaddr, (socklen_t*)&in_addr_size);
  
  if (newSockfd < 0) { 
    printf("server acccept failed...\n"); 
    exit(0); 
  }
  
  send(newSockfd,"OK", 3, 0);
  close(newSockfd);
  close(sockfd);
}

// Print help message about program usage
void printHelp(std::string prog)
{
  //e:sc:t:i:m:dn:b:E:l:h
  std::cout<<"\n\tUsage: "<<prog<<" syncConfig replayConfig IPConfig [options]\n\n"
    "syncConfig: -s | -c serverIP\n"
    "   -s              this is the server, wait for connection to sync\n"
    "   -c serverIP     this is a client, and will connect to serverIP to sync\n\n"
    "replayConfig: -e eventFile | -m peerIPFile [-n numConns][-E numEvents][-b numBytes]\n"
    "   -e eventFile    file with events to be replayed, usually obtained from mimic-extract\n"

    "   -m peerIPFile   make up traffic to generate, using myIPFile and peerIPFile\n"
    "   -n numConns     make up numConns parallel connections\n"
    "   -E numEvents    make up numEvents sends per connection\n"
    "   -b numBytes     each made-up send is numBytes large\n\n"
    "IPConfig: -i IPFile\n"      
    "   -i myIPFile     file that specifies IPs on this machine, one per line\n\n"
    "Options: [-h][-d][-t numThreads][-l logDir]\n"
    "   -h              print this help message\n"
    "   -d              turn on debug messages\n"
    "   -t numThreads   use up to this number of threads\n"
    "   -l logDir       if DEBUG flag is on, save output in this log directory\n";

}
// Main program
int main(int argc, char* argv[]) {

    char myName[SHORTLEN], filename[MEDLEN];

    // Register a signal handler
    signal(SIGINT, signal_callback_handler);
    signal(SIGPIPE, SIG_IGN);

  
    std::string ipFile = "";
    std::string forFile = "";
    std::string eventFile = "";

    bool DEBUG = false;

    int c;
    strcpy(logDir, "/tmp");
    while ((c = getopt (argc, argv, "e:sc:t:i:m:dn:b:E:l:h")) != -1)
    switch (c)
      {
      case 'l':
	strncpy(logDir,optarg,MEDLEN);
	break;
      case 'h':
	printHelp(argv[0]);
	exit(0);
      case 'n':
	numconns.store(atoi(optarg));
	break;
      case 'b':
	numbytes.store(atoi(optarg));
	break;
      case 'E':
	numevents.store(atoi(optarg));
	break;
      case 'm':
	makeup = true;
	forFile = optarg;
	break;
      case 's':
	isServer = true;
	break;
      case 'i':
	ipFile = optarg;
	break;
      case 'd':
	DEBUG = true;
	break;
      case 't':
	numThreads.store(std::stoi(optarg));
	break;
      case 'c':
	serverIP = optarg;
	char ss[MEDLEN];
	sprintf(ss, "%s:%d", serverIP.c_str(), SRV_PORT);
	servString = ss;
	break;
      case 'e':
	eventFile = optarg;
	break;
      case '?':
        if (optopt == 'i' || optopt == 'e' || optopt == 'c' || optopt == 't' || optopt == 'm'
	    || optopt == 'l' || optopt == 'n' || optopt == 'b' || optopt == 'E')
	  std::cerr<<"Option -"<<(char)optopt<<" requires an argument.\n";
        else
	  std::cerr<<"Unknown option -"<<optopt<<std::endl;
        return 1;
      default:
        abort ();
      }
    
    if(ipFile == "") {
        std::cerr << "We need an IPFile argument." << std::endl;
        exit(-1);
    }

    if(forFile == "" && makeup) {
        std::cerr << "We need a foreign file argument to make up traffic." << std::endl;
        exit(-1);
    }

    if (eventFile == "" && !makeup) {
      std::cerr << "We need an eventFile to replay traffic." << std::endl;
      exit(-1);
    }
      

    if (DEBUG)
      std:: cout<<"Debug is ON"<<std::endl;
    
    // Create FileWorker threads
    int notifierFD = createEventFD();
    EventNotifier* loadMoreNotifier = new EventNotifier(notifierFD, "Test file notifier.");
    EventQueue** fileQ = (EventQueue**) malloc(numThreads.load()*sizeof(EventQueue*));
    
    for (int i = 0; i< numThreads.load(); i++)
      fileQ[i] = new EventQueue("File events.");
      
    
    // Store eventfile if any
    std::vector<std::string> eFiles;
    eFiles.push_back(eventFile);
    
    gethostname(myName, SHORTLEN);
    sprintf(filename, "%s/file.%s.txt", logDir, myName);
    FileWorker* fw = new FileWorker(loadMoreNotifier, fileQ, ipFile, forFile, eFiles,
				    &connStats, numThreads.load(), DEBUG, filename, true);
    fw->startup();
    // Start only file worker
    isRunning.store(true);
    std::chrono::high_resolution_clock::time_point fstartPoint = std::chrono::high_resolution_clock::now();
    std::thread fileWorkerThread(&FileWorker::loop, fw, fstartPoint);

    // Wait while initial load is done
    while(isInitd.load() == false)
	sleep(1);

    // Check how many file queues are empty and drop that many threads
    int n = numThreads.load();
    for (int i=0;i<numThreads.load();i++)
      {
	if (fileQ[i]->getLength() == 1)
	  n--;
      }
    numThreads.store(n);
    std::cout<<"Final num threads "<<numThreads.load()<<std::endl;

    // Start event handlers
    EventHandler** eh = (EventHandler**)malloc(numThreads.load()*sizeof(EventHandler*));
      
    for (int i=0;i<numThreads.load();i++)
      {
	sprintf(filename, "%s/thread.%s.%d.txt", logDir, myName,i);
	eh[i] = new EventHandler(loadMoreNotifier, fileQ[i], &connStats, i, DEBUG, filename);
	eh[i]->startup();
      }

    std::thread** eventHandlerThread = (std::thread**)malloc(numThreads.load()*sizeof(std::thread*));

    // We're ready to start, notify the other side
    // and wait until they are ready too
    if (isServer)
      informPeer();
    else
      waitForPeer();

    // Communicate start time to File worker
    std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    fw->setStartTime(now);
    
    for (int i=0; i<numThreads.load(); i++)
      eventHandlerThread[i] = new std::thread(&EventHandler::loop, eh[i], startPoint);

    // Wait forever and keep printing the statistics
    while(true)
      {
	sleep(1);
	print_stats(false);
      }
}
