#include "connections.h"
#include "eventQueue.h"
#include "fileWorker.h"
#include "mimic.h"


FileWorker::FileWorker(EventNotifier* loadMoreNotifier, std::unordered_map<long int, long int>* c2time, std::unordered_map<std::string, long int>* l2time, EventQueue** outQ, EventQueue* accept, std::string& ipFile, std::string& forFile,  std::vector<std::string>& eFiles, std::map<long int, struct stats>* cs, int nt, bool debug, std::string myname, bool useMMapFlag) {
  
    fileEventsAddedCount = 0;
    useMMap = useMMapFlag;
    my_conn_id = 0;
    my_time = 10000;
    my_sport = 10000;
    my_cport = 20000;
    my_runningtime = 0;
    my_events = 0;
    startTime = 0;
    
    connTime = c2time;
    listenerTime = l2time;
    connStats = cs;
    DEBUG = debug;
    out = new std::ofstream(myname);

    threadToEventCount = {};
    threadToConnCount = {};
    servStringToThread = {};
    
    /* Deal with our notifier where the EventHandler can prompt us to load more events. */
    loadEventsNotifier = loadMoreNotifier;
    loadEventsPollHandler = new PollHandler(DEBUG);
    loadEventsPollHandler->watchForRead(loadMoreNotifier->myFD());
    
    /* Get a shortterm heap so we can internally reorder connection start/stop events 
       with events from event files. */
    shortTermHeap = new EventHeap();
    
    /* Queue of events for the EventHandler. */
    outEvents = outQ;
    acceptEvents = accept;
    IPListFile = ipFile;
    foreignIPFile = forFile;
    eventsFiles = eFiles;


    /* Open our events files. */    
    for(currentEventFile = eventsFiles.begin(); currentEventFile != eventsFiles.end(); ++currentEventFile) {
        try {
	  if (*currentEventFile != "-")
	    {
	      std::ifstream* f = new std::ifstream(*currentEventFile, std::ios::in);
	      eventsIFStreams.push_back(f);
	    }
        }
        catch(std::ios_base::failure& e) {
            std::cerr << e.what() << std::endl;
        }
    }
    
    /* If we should use mmap, mmap our files now. */
    if (makeup.load())
      return;
    if(useMMap) {
        for(currentEventFile = eventsFiles.begin(); currentEventFile != eventsFiles.end(); ++currentEventFile) {
           /* Get the file size. */
           struct stat st;
           stat((*currentEventFile).c_str(), &st);
           size_t filesize = st.st_size;
           
           int filefd = open((*currentEventFile).c_str(), O_RDONLY, 0);
           assert(filefd != -1);
           
           void* mmappedData = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, filefd, 0);
           assert(mmappedData != MAP_FAILED);
           
           mmapedFiles.push_back(mmappedData);
           mmapToSize[mmappedData] = filesize;
           
           madvise(mmappedData, filesize, POSIX_MADV_SEQUENTIAL);
        }    
    }

    eventIFStreamsItr = eventsIFStreams.begin();
    mmappedFilesItr = mmapedFiles.begin();
    currentEventFile = eventsFiles.begin();
}    

FileWorker::~FileWorker() {
    for(eventIFStreamsItr = eventsIFStreams.begin(); eventIFStreamsItr != eventsIFStreams.end(); ++eventIFStreamsItr) {
        (*eventIFStreamsItr)->close();
    }
}

std::string FileWorker::trim(const std::string& str, const std::string& whitespace) {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return "";
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

ConnectionPairMap * FileWorker::getConnectionPairMap() {
    return &connIDToConnectionPairMap;
}

/* Potentially may want to move to an mmapping strategy. */
std::vector <std::vector <std::string>> FileWorker::loadFile(std::istream* infile, int numFields, int numRecords) {

    std::vector <std::vector <std::string>> data;
    
    int i = 0;
    bool noLimit = false;
    if(numRecords <= 0) noLimit = true;

    while(infile->good()) {	
        std::string s;
        if(!std::getline(*infile, s)) break;
        std::istringstream ss(s);
        std::vector <std::string> record;
        
        while(ss) {
            
            std::string s;
            if(!std::getline(ss, s, ',')) break;
            record.push_back(trim(s));

        }
        if(record.size() == numFields) {
            data.push_back(record);
        }
        i = i+1;
        if(i >= numRecords && !noLimit) break;
    }
    return(data);
}

void FileWorker::getFields(std::string bufPart, std::vector <std::string>* record, int numFields)
{
  std::istringstream ss(bufPart);
  
  while(ss) {
    std::string s;
    if(!std::getline(ss, s, ',')) break;
    record->push_back(trim(s));
  }
}

std::vector <std::vector <std::string>> FileWorker::loadMMapFile(void * mmapData, int numFields, int numRecords) {
    std::vector <std::vector <std::string>> data;
    
    int i = 0;
    bool noLimit = false;
    if(numRecords <= 0) noLimit = true;
    
    char *buff_end = (char *) mmapData + mmapToSize[mmapData];
    char *begin = (char *)mmapData, *end = NULL;
    
    while((end = static_cast<char*>(memchr(begin,'\n',static_cast<size_t>(buff_end-begin)))) != NULL) {
        std::vector <std::string> record;
        std::string bufPart;
        bufPart.assign(begin,end);

	getFields(bufPart, &record, numFields);
	if(record.size() == numFields) {
	  data.push_back(record);
	}
	else {
	  std::cerr << "ERROR: Not enough fields in line to process: " <<bufPart<<" size "<<record.size()<<" numfields "<<numFields<< std::endl;
	}

        i = i + 1;

        if(end != buff_end) {
            begin = end+1;
        }
        else break;
    }
    if (DEBUG)
      (*out)<<"Loaded "<<i<<" records\n";

    return data;
}


bool FileWorker::isMyIP(std::string IP) {
    std::unordered_set<std::string>::const_iterator got = myIPs.find(IP);
    
    if(got == myIPs.end()) return false;
    return true;
}

bool FileWorker::isMyConnID(long int connID) {
    std::unordered_set<long int>::const_iterator got = myConnIDs.find(connID);
    
    if(got == myConnIDs.end()) return false;
    return true;
}

void FileWorker::loadEvents(int eventsToGet, int rounds) {
    
  if (DEBUG)
    (*out) <<rounds<<" Loading events, last line " <<lastLine<<" max "<<eventData.size()<<std::endl;

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  
  for(int i=0; i<numThreads.load();i++)
    {
      threadToConnCount[i] = 0;
      threadToEventCount[i] = 0;
    }
  
    int currentThread = 0;

    int eventsProduced = 0;

    // If we're done get out
    if (isProcessed)
      {
	return;
      }
    // Should we make up traffic or read from file?
    if (lastLine == 0)
      {
	if (makeup.load())
	  {
	    bool done = false;
	    
	    if (DEBUG)
	      (*out)<<"Making up events\n";
	    
	    if (startTime > 0)
	      if (now - startTime + 10000 > my_time)
		my_time = now - startTime + 10000;
	    
	    if (DEBUG)
	      (*out)<<"Start time is "<<startTime<<" now "<<now<<" my time "<<my_time<<std::endl;
	    
	    auto it = myIPs.begin();
	    std::string myIP = *it;
	    it = foreignIPs.begin();
	    std::string peerIP = *it;
	    if (DEBUG)
	      (*out)<<"Makeup traffic between "<<myIP<<" and "<<peerIP<<std::endl;
	    
	    std::string server, client;
	    int sport, cport;
	    std::string inputString="";
	    double time;
	    int numFields = 8;
	    
	    std::vector <std::string> record;
	    int totalevents = 0;
	    if (my_events == 0)
	      {
		my_eid = 0;
		for(int i=0; i<numconns.load(); i++)
		  {
		    if (i < numconns.load()/2)
		      {
			if (isServer)
			  {
			    server = peerIP;
			    client = myIP;
			  }
			else
			  {
			    client = peerIP;
			    server = myIP;
			  }
		      }
		    else
		      {
			if (!isServer)
			  {
			    server = peerIP;
			    client = myIP;
			  }
			else
			  {
			    client = peerIP;
			    server = myIP;
			  }
		      }
		    // Create initial connection
		    
		    time = my_time - SRV_UPSTART;
		    
		    if (time > lastEventTime)
		      lastEventTime = time;
		    
		    
		    time /= 1000;
		    inputString = "CONN,"+std::to_string(my_conn_id+i)+","+client+","+std::to_string((my_cport++) % 65536 + 1)+",->,"+server+","+std::to_string(my_sport+i)+","+std::to_string(time);
		    totalevents++;
		    if (DEBUG)
		      (*out)<<inputString<<std::endl;
		    record.clear();
		    getFields(inputString, &record, numFields);
		    eventData.push_back(record);
		  }
	      }
	    time = my_time;
	    double delta = 1000/numevents.load()/4.0;
	    my_runningtime = time;
	    if (DEBUG)
	      (*out)<<"Total events "<<totalevents<<std::endl;
	    
	    for (int j=my_events; j<numevents.load();j++)
	      {
		double dtime;
		long int eid;
		for(int i=0; i<numconns.load(); i++)
		  {
		    if (i < numconns.load()/2)
		      {
			if (isServer)
			  {
			    server = peerIP;
			    client = myIP;
			  }
			else
			  {
			    client = peerIP;
			    server = myIP;
			  }
		      }
		    else
		      {
			if (!isServer)
			  {
			    server = peerIP;
			    client = myIP;
			  }
			else
			  {
			    client = peerIP;
			    server = myIP;
			  }
		      }
		    
		    eid = my_eid;
		    dtime = my_runningtime;
		    dtime += delta;
		    time = dtime/1000;
		    inputString = "EVENT,"+std::to_string(my_conn_id+i)+","+std::to_string(eid++)+","+client+",SEND,"+std::to_string(numbytes.load())+",0,"+std::to_string(time);
		    if (DEBUG)
		      (*out)<<inputString<<std::endl;
		    record.clear();
		    getFields(inputString, &record, numFields);
		    eventData.push_back(record);
		    dtime += delta;
		    time = dtime/1000;
		    inputString = "EVENT,"+std::to_string(my_conn_id+i)+","+std::to_string(eid++)+","+server+",WAIT,"+std::to_string(numbytes.load())+",0,"+std::to_string(time);
		    if (DEBUG)
		      (*out)<<inputString<<std::endl;
		    record.clear();
		    getFields(inputString, &record, numFields);
		    eventData.push_back(record);
		    dtime += delta;
		    time = dtime/1000;
		    inputString = "EVENT,"+std::to_string(my_conn_id+i)+","+std::to_string(eid++)+","+server+",SEND,"+std::to_string(numbytes.load())+",0,"+std::to_string(time);
		    record.clear();
		    getFields(inputString, &record, numFields);
		    if (DEBUG)
		      (*out)<<inputString<<std::endl;
		    eventData.push_back(record);
		    dtime += delta;
		    time = dtime/1000;
		    inputString = "EVENT,"+std::to_string(my_conn_id+i)+","+std::to_string(eid++)+","+client+",WAIT,"+std::to_string(numbytes.load())+",0,"+std::to_string(time);
		    if (DEBUG)
		      (*out)<<inputString<<std::endl;
		    record.clear();
		    getFields(inputString, &record, numFields);
		    eventData.push_back(record);
		  }
		my_eid = eid;
		totalevents += 4*numconns.load();
		my_runningtime = dtime;
		if (DEBUG)
		  (*out)<<"Total events "<<totalevents<<" my eid "<<my_eid<<std::endl;
		if (totalevents >= eventsToGet*0.9)
		  {
		    my_events = j;    
		    done = true;
		    break;
		  }
	      }
	    if (!done)
	      {
		double dtime;
		long int eid;
		for(int i=0; i<numconns.load(); i++)
		  {
		    eid = my_eid;
		    dtime = my_runningtime;
		    inputString = "EVENT,"+std::to_string(my_conn_id+i)+","+std::to_string(eid++)+","+client+",CLOSE,"+std::to_string(numbytes.load())+",0,"+std::to_string(time);
		    if (DEBUG)
		      (*out)<<inputString<<std::endl;
		    record.clear();
		    getFields(inputString, &record, numFields);
		    eventData.push_back(record);
		    dtime += delta;
		    time = dtime/1000;
		    inputString = "EVENT,"+std::to_string(my_conn_id+i)+","+std::to_string(eid++)+","+server+",CLOSE,"+std::to_string(numbytes.load())+",0,"+std::to_string(time);
		    if (DEBUG)
		      (*out)<<inputString<<std::endl;
		    record.clear();
		    getFields(inputString, &record, numFields);
		    eventData.push_back(record);
		  }		
		my_conn_id += numconns.load();
		my_time += 1000;
		my_events = 0;
	      }
	  }
	else
	  {
	    if(useMMap) {
	      if (DEBUG)
		(*out) << "Using MMAP" << std::endl;
	      eventData = loadMMapFile(*mmappedFilesItr, 8, eventsToGet);
	    }
	    else {
	      eventData = loadFile(*eventIFStreamsItr, 8, eventsToGet);
	    }
	    /* We've probably reached the end of the file. */
	    eventIFStreamsItr++;
	    mmappedFilesItr++;
	    
	    if(eventIFStreamsItr >= eventsIFStreams.end() || mmappedFilesItr >= mmapedFiles.end()) {
	      isDone = true;
	      if (false) // Jelena, relooping
		{
		  for(eventIFStreamsItr = eventsIFStreams.begin(); eventIFStreamsItr != eventsIFStreams.end(); ++eventIFStreamsItr) {
		    (*eventIFStreamsItr)->clear();
		    (*eventIFStreamsItr)->seekg(0, std::ios::beg);
		  }
		  eventIFStreamsItr = eventsIFStreams.begin();
		  mmappedFilesItr = mmapedFiles.begin();
		  if(loopedCount == 0) {
		    /* This is the first time we've looped, record the duration. */
		    loopDuration = lastEventTime;
		  }
		  loopedCount = loopedCount + 1;
		}
	    }
	  }
      }
    std::vector<int>::size_type i = lastLine;
      
    for(;i < eventData.size(); i++) {
      eventsProduced = eventsProduced + 1;

      // Check if this is CONN record or event record
      if (eventData[i][0] == "CONN")
	{
	  long int connID;
	  try {
	      connID = std::stol(eventData[i][1].c_str());
	    }
	    catch(...){
	      perror("Problem with connData line, continuing.");
	      continue;
	    }
	    std::string src = trim(eventData[i][2]);
	    int sport = std::stoi(eventData[i][3].c_str());
	    std::string dst = trim(eventData[i][5]);
	    int dport = std::stoi(eventData[i][6].c_str());
	    char ports[10], portd[10];
	    sprintf(portd, "%d", dport);
	    sprintf(ports, "%d", sport);
	    std::string servString = dst + ":" + portd;
	    std::string connString = src + ":" + ports+","+dst+":"+portd;
	    
	    if(isMyIP(src) || isMyIP(dst)) {
	      /* Add this connid to our ids.*/
	      if (DEBUG)
		(*out) << "Adding " << connID << " to my connection ids." << std::endl;
	      
	      myConnIDs.insert(connID);
	      
	      /* Fill out connIDToConnectionPairMap */
	      connectionPair cp = connectionPair(src, sport, dst, dport);
	      connIDToConnectionPairMap[connID] = std::make_shared<connectionPair>(cp);
	      /* Add an event to start this connection. */
	      Event e;
	      e.serverString = servString;
	      e.connString = connString;
	      e.conn_id = connID;
	      e.event_id = -1;
	      e.value = -1;
	      e.ms_from_start = 0;
	      e.ms_from_last_event = 0;
	      (*connStats)[e.conn_id].total_events = 0;
	      (*connStats)[e.conn_id].state = INIT;
	      (*connStats)[e.conn_id].delay = 0;
	      (*connStats)[e.conn_id].last_completed = -1;
	      //if (DEBUG)
	      //	(*out) << "4\n";
	      if(isMyIP(src)) {
                e.ms_from_start = stod(eventData[i][7])*1000 + SRV_UPSTART;
                e.type = CONNECT;
		if (DEBUG)
		  (*out)<<"Adding connect event for conn "<<e.conn_id<<"\n";

		shortTermHeap->addEvent(e);
	      }
	      else {
                /* XXX Have we started a server for this IP:port yet? If not, add event. */
		e.ms_from_start =  std::max((long int)(std::stod(eventData[i][7].c_str()) * 1000), (long int) 0);
		e.event_id = -2;
                e.type = SRV_START;
		if (DEBUG)
		  (*out)<<"Server string "<<servString<<std::endl;

		connIDToServString[e.conn_id] = servString;
		shortTermHeap->addEvent(e);

		if (DEBUG)
		  (*out)<<"Adding server event START for server "<<e.serverString<<" for conn "<<e.conn_id<<"\n";
	      }
	      (*connStats)[e.conn_id].total_events++;
	      if (e.ms_from_start + loopedCount*loopDuration > lastEventTime)
		lastEventTime = e.ms_from_start+ loopedCount * loopDuration;
	      if (DEBUG)
		(*out)<<"Lastevent "<<lastEventTime<<std::endl;
	    }
	    src.clear();
	    dst.clear();
	}
	else if(eventData[i][0] == "EVENT")
	  {
	    //(*out)<<"Check conn id "<<eventData[i][1].c_str()<<std::endl;
	    if(isMyConnID(std::stol(eventData[i][1].c_str()))) {
	      //(*out)<<"My conn\n";
	      Event e;
	      e.conn_id = std::stol(eventData[i][1].c_str());
	      e.event_id = std::stol(eventData[i][2].c_str());      
	      e.value = std::stoi(eventData[i][5].c_str()); 
	      e.ms_from_last_event = (long int)(std::stod(eventData[i][6].c_str()) * 1000);
	      e.ms_from_start = (long int)(std::stod(eventData[i][7].c_str()) * 1000) + loopedCount * loopDuration;
	      /* Type of event - send and receive. */
	      if(isMyIP(eventData[i][3])) {
		//(*out)<<"My ip\n";
		(*connStats)[e.conn_id].total_events++;

		if(eventData[i][4].compare("SEND")==0) e.type = SEND;
		else if (eventData[i][4].compare("WAIT")==0) e.type = RECV;
		else if (eventData[i][4].compare("CLOSE")==0) e.type = CLOSE;
		//(*out)<<"Data type "<<e.type<<std::endl;
		//else {
		//    if(eventData[i][3].compare("SEND")==0) e.type = RECV;
		//    else e.type = WAIT;
		//}
		//(*out) << "Have event with time of " << e.ms_from_start << std::endl;
		
		if (DEBUG)
		  (*out)<<"Event for conn "<<e.conn_id<<" event id "<<e.event_id<<" type "<<EventNames[e.type]<<" value "<<e.value<<" time "<<e.ms_from_start<<std::endl;

		(*connTime)[e.conn_id] = e.ms_from_start;
		shortTermHeap->addEvent(e);
	      }
	      double evtime =  std::stod(eventData[i][7].c_str()) * 1000 + loopedCount * loopDuration;
	      if (evtime > lastEventTime)
		lastEventTime = evtime;
	      if (eventsProduced >= eventsToGet)
		{
		  lastLine = i+1;
		  break;
		}
	      //if (t > lastEventTime)
	      //lastEventTime = t;
	    }
	  }
    }
    if (DEBUG)
      (*out)<<rounds<<" lastevent "<<lastEventTime<<" short term heap "<<shortTermHeap->getLength()<<" events produced "<<eventsProduced<<" events to get "<<eventsToGet<<std::endl;

    if  (i == eventData.size())
      {
	if (!isDone)
	  {
	    lastLine = 0;
	    if (makeup.load())
	      eventData.clear();
	  }
	else
	  {
	    isProcessed = true;
	    if (DEBUG)
	      (*out)<<"Processed set to true\n";
	  }
      }
    
    // Now go through times when server should end and add those
    /*
    for(auto it = listenerTime->begin(); it != listenerTime->end();)
      {
	if (it->second < lastEventTime - SRV_GAP)
	  {
	    Event e;
	    e.serverString = it->first;
	    e.conn_id = -1;
	    e.event_id = -1;
	    e.value = -1;
	    e.ms_from_start = it->second + 2*SRV_UPSTART;
	    e.ms_from_last_event = 0;
	    e.type = SRV_END;
	    shortTermHeap->addEvent(e);
	    auto dit = it;
	    it++;
	    listenerTime->erase(dit);
	    //if (e.ms_from_start > lastEventTime)
	    //lastEventTime = e.ms_from_start;
	    if (DEBUG)
	      (*out)<<"Created srv end job for "<<e.serverString<<" at time "<<e.ms_from_start<<std::endl;
	  }
	else
	  it++;
	  }*/
    if (DEBUG)
      (*out) << "Loaded " << eventsProduced << " events from file"<<std::endl;
    //shortTermHeap->print();
}

bool FileWorker::startup() {
    if (DEBUG)
      (*out)<<"File worker starting\n";
    /* Check that we can read our connection, IP and events files. */

    /* Load our IPs. */
    myIPs = {};
    foreignIPs = {};
    for (int f = 0; f < 2; f++)
      {
	std::ifstream infile;
	try {
	  if (f == 0)
	    {
	      (*out)<<"Opened my file "<<IPListFile<<"\n";
	      infile.open(IPListFile.c_str(), std::ios::in);
	    }
	  else
	    {
	      if (foreignIPFile != "")
		{
		  (*out)<<"Opened foreign file "<<foreignIPFile<<"\n";
		  infile.open(foreignIPFile.c_str(), std::ios::in);
		}
	      else
		break;
	    }
	}
	catch (std::ios_base::failure& e) {
	  std::cerr << e.what() << std::endl;
	}
	std::vector <std::vector <std::string>> ipData = loadFile(&infile, 1, -1);
	for(std::vector<int>::size_type i = 0; i != ipData.size(); i++) {
	  if (DEBUG)
	    (*out) << "IP in file: '" << ipData[i][0] <<"'"<< std::endl;
	  std::string ip = trim(ipData[i][0]);
	  if (f == 0)
	    myIPs.insert(ip);
	  else
	    foreignIPs.insert(ip);
	}    
	infile.close();
      }
    
    /* Set ourselves up for the first event file.*/
    /* XXX Should check if our event files are time ordered. */
    loadEvents(maxQueuedFileEvents, rounds++);
    return true;
}

int FileWorker::findMin()
{
  int min = -1;
  int mt = -1;
  for(int i=0; i<numThreads.load();i++)
    {
      if (min == -1 || threadToEventCount[i] < min)
	{
	  min = threadToEventCount[i];
	  mt = i;
	}
    }
  return mt;
}

void FileWorker::setStartTime(unsigned long int sTime)
{
  startTime = sTime;
  if (DEBUG)
    (*out)<<"Start time set to "<<startTime<<std::endl;
}

void FileWorker::loop(std::chrono::high_resolution_clock::time_point startTime) {
    long int nextET = -1;
    int currentThread = 0;
    
    if (DEBUG)
      (*out)<<"FW looping heap has "<<shortTermHeap->getLength()<<" events\n";
    //shortTermHeap->print();

    while(isRunning.load()) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	if (DEBUG)
	  (*out)<<"MS "<<ms<<" looping\n";
        nextET = shortTermHeap->nextEventTime();
	if (DEBUG)
	  (*out) << "Pulling from our heap, next event time in heap is: " << nextET << " Last event time: " << lastEventTime << std::endl;
        while(nextET <= lastEventTime && nextET > -1) {
	  Event e = shortTermHeap->nextEvent();
	  //if (DEBUG)
	  //(*out) << "Have event to add of type " << EventNames[e.type] <<" time "<<nextET<<std::endl;
	  if (DEBUG)
	    (*out) << "Adding event with time: " << shortTermHeap->nextEventTime() << " time of last event added " << lastEventTime <<  std::endl;
	  std::shared_ptr<Event> e_shr = std::make_shared<Event>(e);
	  int t;
	  if (connIDToThread.find(e.conn_id) != connIDToThread.end())
	    {
	      t = connIDToThread[e.conn_id];
	    }
	  else
	    {
	      t = findMin();
	      if (DEBUG)
		(*out)<<"Found new thread "<<t<<" for conn "<<e.conn_id<<std::endl;	      

	      if (e.type == SRV_START)
		{
		  // SRV_START gets added to all threads since we
		  // don't know which thread will get it
		  for(int i=0; i<numThreads.load();i++)
		    {
		      servStringToThread[e.serverString].insert(i);
		      if (DEBUG)
			(*out)<<"Pushed thread "<<i<<" for servser strign "<<e.serverString<<std::endl;
		      threadToConnCount[i]++;
		    }
		}
	      else
		threadToConnCount[t]++;
	      connIDToThread[e.conn_id] = t;
	    }
	  threadToEventCount[t]++;
	  // Figure out if we'll add SRV_START or not
	  if (e.type == SRV_START)
	    {
	      // SRV_START gets added to all threads since we don't know who will get
	      // the event from the OS
	      for(int i=0; i<numThreads.load();i++)
		{
		  (*listenerTime)[e.serverString] = e.ms_from_start;
		  outEvents[i]->addEvent(e_shr);
		}
	    }
	  else
	    {	      
	      outEvents[t]->addEvent(e_shr);
	      if (connIDToServString.find(e.conn_id) != connIDToServString.end())
		{
		  std::string servString = connIDToServString[e.conn_id];
		  for(auto it=servStringToThread[servString].begin(); it != servStringToThread[servString].end(); it++)
		    {
		      if ((*it) != t)
			{
			  outEvents[*it]->addEvent(e_shr);
			}
		    }
		}
	    }
	  e_shr.reset();
	  fileEventsAddedCount++;
	  if(fileEventsAddedCount > maxQueuedFileEvents) {
	    fileEventsAddedCount = 0;
	    break;
	  }
	  nextET = shortTermHeap->nextEventTime();
        }
	for(int i=0; i<numThreads.load();i++)
	  {
	    if (DEBUG)
	      (*out)<<"Thread "<<i<<" conns "<<threadToConnCount[i]<<" events "<<threadToEventCount[i]<<std::endl;
	  }

	if (isInitd.load() == false)
	  isInitd.store(true);
	
        /* Maybe we should give it a rest for a bit. */
        loadEventsPollHandler->waitForEvents(1);
        
        struct epoll_event e;
        if(isRunning.load() && loadEventsPollHandler->nextEvent(&e)) {
	  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	  if (DEBUG)
            (*out) <<"MS "<<ms<<" Got notification to load more events." << std::endl;
            while(loadEventsPollHandler->nextEvent(&e)) {
	      if (DEBUG)
                (*out) << "Got load notification from loadEventsNotifier." << std::endl;
                loadEventsNotifier->readSignal();
            }
            loadEvents(maxQueuedFileEvents, rounds++);
        }
    }    
}

