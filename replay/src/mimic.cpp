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



