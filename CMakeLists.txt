cmake_minimum_required (VERSION 2.6)
project (mimic)
find_package (Threads)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY lib)
include_directories(replay/include)
add_library(mimiclib STATIC replay/src/pollHandler.cpp replay/src/eventNotifier.cpp replay/src/eventQueue.cpp replay/src/eventHandler.cpp replay/src/utils.cpp replay/src/connections.cpp  replay/src/fileWorker.cpp)
add_executable(mimic-replay replay/src/mimic-replay.cpp)
add_executable(mimic-extract extract/mimic-extract.cpp)
target_link_libraries (mimic-replay ${CMAKE_THREAD_LIBS_INIT} mimiclib)
target_link_libraries (mimic-extract trace)

set (Mimic_VERSION_MAJOR 1)
set (Mimic_VERSION_MINOR 0)

set (CMAKE_CXX_COMPILER /usr/bin/clang++)

set (CMAKE_CXX_FLAGS "-Wall -g -O2 --std=gnu++2a --stdlib=libc++ -DASIO_STANDALONE")

install(TARGETS mimic-extract mimic-replay
        RUNTIME DESTINATION bin)
