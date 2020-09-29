#include <atomic>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <signal.h>

#include "mimic.h"
#include "eventQueue.h"
#include "eventHandler.h"
#include "fileWorker.h"
#include "connections.h"

#define SRV_PORT 5205

std::atomic<bool> isRunning = false;
std::atomic<bool> isInitd = false;
std::atomic<int> numThreads = 1;
std::atomic<int> numconns = 1;
std::atomic<int> numbytes = 100;
std::atomic<int> numevents = 1;
std::mutex fileHandlerMTX;
std::mutex statsMTX;

long int global_throughput;
long int global_events;

std::condition_variable fileHandlerCV;
bool loadMoreFileEvents = true;
std::string servString = "";
std::string serverIP = "";
auto startTime = 0;
std::atomic<bool> makeup = false;
std::atomic<bool> isServer = false;

void serverSocketsThread(std::string serverIP, int numConns, EventQueue* eq) {
    int sockets[numConns]; 
    int efd;
    

    if ((efd = epoll_create1(0)) == -1) {
        perror("epoll_create");
        exit(-1);
    }

    for(int i=0; i<numConns; i++) {
        
        /* Get non-blocking socket. */
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        int status = fcntl(sockets[i], F_SETFL, fcntl(sockets[i], F_GETFL, 0) | O_NONBLOCK);
        if(status == -1) {
            std::cerr << "Had trouble getting non-blocking socket." << std::endl;
            exit(-1);
        }
        
        /* Bind. */
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        inet_pton(AF_INET, "0.0.0.0", &(sa.sin_addr));
        sa.sin_port = htons(SRV_PORT + i);
        std::cout << "Binding to port " << SRV_PORT + i << std::endl;
        if(bind(sockets[i], (struct sockaddr *)&sa, sizeof(sa)) <0) {
            perror("bind failed.");
            exit(-1);
        }
        
        /* Listen. */
        if(listen(sockets[i], 3) == -1) {
            perror("listen failed.");
            exit(-1);
        }

        /* Add to epoll. */
        static struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
        ev.data.fd = sockets[i];
        int stat = epoll_ctl(efd, EPOLL_CTL_ADD, sockets[i], &ev);
        if(stat != 0) {
            perror("epoll_ctr, ADD");
            exit(-1);
        }    
    }

    /* Event loop. */
    int MAX_EPOLL_EVENTS_PER_RUN = numConns*2;
    struct epoll_event *events;
    events = (struct epoll_event *)calloc(MAX_EPOLL_EVENTS_PER_RUN, sizeof(struct epoll_event));
    if(!events) {
        perror("Calloc epoll events.");
        exit(-1);
    }
    while(isRunning.load()) {
        int nfds = epoll_wait(efd, events, MAX_EPOLL_EVENTS_PER_RUN, 5000);
        if (nfds < 0){
            perror("Error in epoll_wait!");
            exit(-1);
        }        
        for(int i = 0; i < nfds; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                /* There was an error. */
                std::cerr << "There is an error with a listening socket." << std::endl;
	    }
	    else {
                int fd = events[i].data.fd;
                Event e;
                e.ms_from_start = 0;
                struct sockaddr in_addr;
                int in_addr_size = sizeof(in_addr);
                e.sockfd = accept(fd, &in_addr, (socklen_t*)&in_addr_size);
                std::cout << "Accepted client." << std::endl;
                int status = fcntl(e.sockfd, F_SETFL, fcntl(e.sockfd, F_GETFL, 0) | O_NONBLOCK);
                /* Add this as a socket event to watch. */
                if(e.sockfd != -1 && status != -1) (*eq).addEvent(std::make_shared<Event>(e));
            }
        }
    }
    std::cout << "Server thread quitting." << std::endl;
    for(int i=0; i<numConns; i++) { 
        close(sockets[i]);
    } 
    free(events);
}

int readFromSocket(int sockfd, bool* done) {
    int count = 0;
    *done = false;
    std::cout << std::endl << "In read:" << std::endl;
    while(1) {
        int c;
        char buf[10];
        bzero(buf, sizeof(buf));
        c = read(sockfd, buf, sizeof buf);
        std::cout << "RECVD: " << buf << std::endl;
        if(c == -1) {
            if(errno == EAGAIN) {
                return count;
            }
            *done = true;
            return count;
        }
        else if (c == 0) {
            /* End of file. */
            *done = true;
            return count;
        }
        count = count + c;
        std::cout << "Looping in recv." << std::endl;
    }
}

int writeToSocket(int sockfd, bool* done, int count) {
    *done = false;
    return(send(sockfd, malloc(count * sizeof(char *)), count, 0));
}

void connectionHandlerThread(int numConns, EventQueue* eq) {
    std::shared_ptr<Event> job;
    int efd;

    if ((efd = epoll_create1(0)) == -1) {
        perror("epoll_create");
        exit(-1);
    }

    int MAX_EPOLL_EVENTS_PER_RUN = numConns*2;
    struct epoll_event *events;
    events = (struct epoll_event *)calloc(MAX_EPOLL_EVENTS_PER_RUN, sizeof(struct epoll_event));

    while(isRunning.load()) {
        /* Grab any accepted sockets. */
        while((*eq).getEvent(job)) {
            std::cout << "Second thread received new socket." << std::endl;
            static struct epoll_event ev;
            
            ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
            ev.data.fd = job->sockfd;
            int stat = epoll_ctl(efd, EPOLL_CTL_ADD, job->sockfd, &ev);
            job.reset();
        }
        
        /* Wait for reads. */
        int nfds = epoll_wait(efd, events, MAX_EPOLL_EVENTS_PER_RUN, 0);
        if (nfds < 0){
            perror("Error in epoll_wait!");
            exit(-1);
        }
        for(int i = 0; i < nfds; i++) {
            std::cout << "Got " << nfds << " events." << std::endl;
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                /* There was an error. */
            }
            else if(events[i].events & EPOLLIN) {
                bool done = true;
                int count = 0;
                count = readFromSocket(events[i].data.fd, &done);
            }
            else if(events[i].events & EPOLLOUT) {
                bool done = true;
                int count = 5000;
                count = writeToSocket(events[i].data.fd, &done, count);
            }
        }
    }   
    std::cout << "Read/write thread quitting." << std::endl;     
}

std::map<long int, struct stats> connStats;


void print_stats(int flag)
{
  std::ofstream myfile;

  if (flag)
    {
      char myName[SHORTLEN], filename[MEDLEN];
      gethostname(myName, SHORTLEN);
      sprintf(filename, "connstats.%s.txt", myName);
      myfile.open(filename);
    }
  int completed = 0, total = 0;
  int delay = 0;
  for (auto it=connStats.begin(); it != connStats.end(); it++)
    {
      total++;
      if (flag)
	myfile<<"Conn "<<it->first<<" state "<<it->second.state<<" total events "<<it->second.total_events<<" last event "<<it->second.last_completed<<" delay "<<it->second.delay<<" started "<<it->second.started<<" completed "<<it->second.completed<<std::endl;
      if (it->second.state == DONE)
	{
	  completed++;
	  delay += it->second.delay;
	}
    }
  double avgd = (double)delay/completed;
  long int t, e;
  statsMTX.lock();
  t = global_throughput;
  e = global_events;
  global_throughput = 0;
  global_events = 0;
  statsMTX.unlock();
  
  std::cout<<"Successfully completed "<<completed<<"/"<<total<<" avg delay "<<avgd<<" ms, throughput "<<t<<" events "<<e<<std::endl;
  if (flag)
    myfile.close();
}


void signal_callback_handler(int signum) {
  std::cout << "Caught signal " << signum << std::endl;
   // Terminate program
  isRunning.store(false);
  print_stats(true);
  exit(signum);
}

void waitForPeer()
{
  struct sockaddr_in servaddr, cliaddr;
    
  int sockfd;
  
  // socket create and verification 
  sockfd = socket(AF_INET, SOCK_STREAM, 0); 
  if (sockfd == -1) { 
    std::cerr<<"socket creation failed...\n";
    exit(0); 
  } 

  getAddrFromString(servString, &servaddr);
  
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

void informPeer()
{
  struct sockaddr_in servaddr;

  struct sockaddr cliaddr;
  int in_addr_size = sizeof(cliaddr);
    
  getAddrFromString(servString, &servaddr);

  // Get an ordinary,blocking socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
      std::cerr<<"socket creation failed...\n";
      exit(0); 
    } 

     servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &(servaddr.sin_addr));
    // assign IP, PORT 
    servaddr.sin_port = htons(SRV_PORT); 

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    
    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) { 
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

int main(int argc, char* argv[]) {

    char myName[SHORTLEN], filename[MEDLEN];
    signal(SIGINT, signal_callback_handler);
    signal(SIGPIPE, SIG_IGN);

  
    int numConns = 1000;
    bool roleFlag = false;
    
    std::string ipFile = "";
    std::string forFile = "";
    std::string connFile = "";
    
    std::string eventFile = "";

    bool DEBUG = false;

    int c;
    while ((c = getopt (argc, argv, "e:sc:t:i:m:dn:b:E:")) != -1)
    switch (c)
      {
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
	std::cout<<"Foreign file "<<forFile<<std::endl;
	break;
      case 's':
	std::cout<<"This is server\n";
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
	break;
      case 'e':
	eventFile = optarg;
	break;
      case '?':
        if (optopt == 'i' || optopt == 'e' || optopt == 'c' || optopt == 't' || optopt == 'm')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        return 1;
      default:
        abort ();
      }
    
    char ss[50];
    sprintf(ss, "%s:%d", serverIP.c_str(), SRV_PORT);
    servString = ss;

    
    if(ipFile == "") {
        std::cerr << "We need an IPFile argument." << std::endl;
        exit(-1);
    }

    if(forFile == "" && makeup) {
        std::cerr << "We need an foreign file argument to make up traffic." << std::endl;
        exit(-1);
    }
        
    // Testing File Worker
    // Event notifier & poll for FileWorker.
    int notifierFD = createEventFD();
    EventNotifier* loadMoreNotifier = new EventNotifier(notifierFD, "Test file notifier.");
    EventQueue** fileQ = (EventQueue**) malloc(numThreads.load()*sizeof(EventQueue*));
    
    for (int i = 0; i< numThreads.load(); i++)
      {
	fileQ[i] = new EventQueue("File events.");
      }
    
    notifierFD = createEventFD();
    EventNotifier * acceptNotifier = new EventNotifier(notifierFD, "Test accept notifier.");
    EventQueue * acceptQ = new EventQueue("Accept events");
    
    notifierFD = createEventFD();
    EventNotifier * receivedNotifier = new EventNotifier(notifierFD, "Test received notifier.");
    EventQueue * recvQ = new EventQueue("Received events");
    EventQueue * sentQ = new EventQueue("Sent events");
    EventQueue * serverQ = new EventQueue("Sever start/stop events");
    EventQueue * sendQ = new EventQueue("Send events.");
    
    std::cout<<"Conn file "<<connFile<<" event file "<<eventFile<<std::endl;

    //std::string ipFile = "/users/gbartlet/mimic-generator/testFiles/b-ips.txt";
    //connFile = "testconn.csv";
    //std::string connFile2 = "testconn2.csv";
    std::vector<std::string> eFiles, eFiles2;
    //eventFile = "evconn.csv";
    eFiles.push_back(eventFile);
    //eventFile = "events2.csv";
    //eFiles2.push_back(eventFile);
    std::unordered_map<long int, long int> c2time;
    std::unordered_map<std::string, long int> l2time;

    gethostname(myName, SHORTLEN);
    sprintf(filename, "file.%s.txt", myName);
    FileWorker* fw = new FileWorker(loadMoreNotifier, &c2time, &l2time, fileQ, acceptQ, ipFile, forFile, eFiles, &connStats, numThreads.load(), DEBUG, filename, true);
    fw->startup();
    ConnectionPairMap * ConnIDtoConnectionPairMap = fw->getConnectionPairMap();
    //FileWorker* fw2 = new FileWorker(loadMoreNotifier, fileQ2, acceptQ, &c2eq2, ipFile, connFile2, eFiles2);
    //fw2->startup();
    //ConnectionPairMap * ConnIDtoConnectionPairMap2 = fw2->getConnectionPairMap();
    // Start only file worker
    isRunning.store(true);
    std::chrono::high_resolution_clock::time_point fstartPoint = std::chrono::high_resolution_clock::now();
    /* File worker. */
    std::thread fileWorkerThread(&FileWorker::loop, fw, fstartPoint);

    // Wait while initial load is done
    while(isInitd.load() == false)
      {
	sleep(1);
      }
    // Check how many file queues are empty and drop that many threads
    int n = numThreads.load();
    for (int i=0;i<numThreads.load();i++)
      {
	if (fileQ[i]->getLength() == 1)
	  n--;
      }
    numThreads.store(n);
    std::cout<<"Final num threads "<<numThreads.load()<<std::endl;
    EventHandler** eh = (EventHandler**)malloc(numThreads.load()*sizeof(EventHandler*));
      
    for (int i=0;i<numThreads.load();i++)
      {
	sprintf(filename, "thread.%s.%d.txt", myName,i);
	
	eh[i] = new EventHandler(loadMoreNotifier, &c2time, &l2time, fileQ[i], acceptQ, recvQ, sentQ, serverQ, sendQ, ConnIDtoConnectionPairMap,  &connStats, i, DEBUG, filename);
	
	eh[i]->startup();
      }
    //EventHandler* eh2 = new EventHandler(loadMoreNotifier, fileQ2, acceptQ, recvQ, sentQ, serverQ, sendQ, ConnIDtoConnectionPairMap2, &c2eq2);
    //eh2->startup();
    
    //ServerWorker* sw = new ServerWorker(serverQ, acceptQ);
    //sw->startup(ConnIDtoConnectionPairMap);
    
    //isRunning.store(true);    
    
    // Start rest of our threads.
    /* File worker. */
    //std::thread fileWorkerThread(&FileWorker::loop, fw, startPoint);
    //std::thread fileWorkerThread2(&FileWorker::loop, fw2, startPoint);
    
    /* Server Woker. */
    //std::thread serverWorkerThread(&ServerWorker::loop, sw, startPoint);                     
        
    /* Event Handler. */
    std::thread** eventHandlerThread = (std::thread**)malloc(numThreads.load()*sizeof(std::thread*));

    /* We're ready to start, notify the other side */
    if (isServer)
      informPeer();
    else
      waitForPeer();

    std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    fw->setStartTime(now);
    
    for (int i=0; i<numThreads.load(); i++)
      {
	eventHandlerThread[i] = new std::thread(&EventHandler::loop, eh[i], startPoint);
      }
    //std::thread eventHandlerThread2(&EventHandler::loop, eh2, startPoint);

    /*
    fileWorkerThread.join();

    try
      {
	for (int i=0; i<numThreads.load(); i++)
	  eventHandlerThread[i]->join();
      }
    catch(std::exception& e)
      {
	std::cout<<"Thread died "<<e.what()<<std::endl;
      }
    */
    /*
    EventQueue* eq = new EventQueue();
    std::thread connThread(connectionHandlerThread,numConns, sendQ);
    connThread.join();
    while(true)
      {
	sleep(1);
	print_stats();
      }
    exit(0);
    */

    while(true)
      {
	sleep(1);
	print_stats(false);
      }



    // test connection map.
    /*std::unordered_map<long int, connectionPair*> connIDToConnectionPairMap = {};
    std::unordered_map<std::string, long int> stringToConnID = {};
    connectionPair c = connectionPair("10.1.1.2", 85, "10.1.1.0", 55);
    connectionPair a = connectionPair("10.1.1.2", 85, "10.1.1.0", 55);
    
    if(a==c) {
        std::cout << "Connection pair a and connection pair c are the same." << std::endl;
    }
     
    bool x=true;
    
    connIDToConnectionPairMap[0] = &c;
    stringToConnID[getConnString(&(c.src), &(c.dst), &x)] = 352;

    std::string str = getConnString(&(c.src), &(c.dst), &x);
    try {
        std::cout << "The c conn id is " << stringToConnID.at("bob") << std::endl;
    }
    catch(std::out_of_range) {
        std::cout << "Caught exception." << std::endl;
    }

    exit(1);

    */
}
