cmake_minimum_required (VERSION 2.6)
project (mimic)
find_package (Threads)

include_directories("include")
add_library(mimiclib STATIC src/pollHandler.cpp src/eventNotifier.cpp src/eventQueue.cpp src/eventHandler.cpp src/mimic.cpp src/connections.cpp  src/fileWorker.cpp)
add_executable(mimic-replay src/mimic-replay.cpp)   
target_link_libraries (mimic-replay ${CMAKE_THREAD_LIBS_INIT} mimiclib)   

set (Mimic_VERSION_MAJOR 1)
set (Mimic_VERSION_MINOR 0)

set (CMAKE_CXX_COMPILER /usr/bin/clang++)

set (CMAKE_CXX_FLAGS "-Wall -g -O2 --std=gnu++2a --stdlib=libc++ -DASIO_STANDALONE")

