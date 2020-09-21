#include "mimic.h"

bool returnLoadMoreFileEvents() {
  return(loadMoreFileEvents | ! isRunning.load());
}

long int msSinceStart(std::chrono::high_resolution_clock::time_point startTime) {
  /* Get a "now" time for our loop iteration. */
  std::chrono::high_resolution_clock::time_point timePoint = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> sinceStart = timePoint-startTime;
  auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sinceStart);
  long int now = (long int)(int_ms.count());
  
  return(now);

}

Event::Event(std::string ss, std::string cs, int fd, long int mfs, long int mfle, EventType t, long int cid, long int eid, long int v)
{
  serverString = ss;
  connString= cs;
  sockfd = fd;
  ms_from_start = mfs;
  ms_from_last_event = mfle;
  type = t;
  conn_id = cid;
  event_id = eid;
  value = v;
}

Event::Event()
{
  serverString = "";
  connString = "";
  sockfd = -1;
  ms_from_start = 0;
  ms_from_last_event = 0;
  type = NONE;
  conn_id = -1;
  event_id = -1;
  value = 0;
}

