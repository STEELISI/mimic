cmake_minimum_required (VERSION 2.6)
project (mimic)
find_package (Threads)

include_directories("include")
add_library(mimiclib STATIC src/pollHandler.cpp src/eventNotifier.cpp src/eventQueue.cpp src/eventHandler.cpp src/mimic.cpp src/connections.cpp  src/fileWorker.cpp)
add_executable(mimic src/main.cpp)   
target_link_libraries (mimic ${CMAKE_THREAD_LIBS_INIT} mimiclib)   

set (TestProj_VERSION_MAJOR 1)
set (TestProj_VERSION_MINOR 0)

set (CMAKE_CXX_COMPILER /usr/bin/clang++)

set (CMAKE_CXX_FLAGS "-Wall -g -O2 --std=gnu++2a --stdlib=libc++ -DASIO_STANDALONE")

