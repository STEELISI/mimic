# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /users/sunshine/mimic/replay

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /users/sunshine/mimic/replay

# Include any dependencies generated for this target.
include CMakeFiles/mimiclib.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/mimiclib.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/mimiclib.dir/flags.make

CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o: CMakeFiles/mimiclib.dir/flags.make
CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o: src/pollHandler.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o -c /users/sunshine/mimic/replay/src/pollHandler.cpp

CMakeFiles/mimiclib.dir/src/pollHandler.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mimiclib.dir/src/pollHandler.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic/replay/src/pollHandler.cpp > CMakeFiles/mimiclib.dir/src/pollHandler.cpp.i

CMakeFiles/mimiclib.dir/src/pollHandler.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mimiclib.dir/src/pollHandler.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic/replay/src/pollHandler.cpp -o CMakeFiles/mimiclib.dir/src/pollHandler.cpp.s

CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o.requires:

.PHONY : CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o.requires

CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o.provides: CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o.requires
	$(MAKE) -f CMakeFiles/mimiclib.dir/build.make CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o.provides.build
.PHONY : CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o.provides

CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o.provides.build: CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o


CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o: CMakeFiles/mimiclib.dir/flags.make
CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o: src/eventNotifier.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o -c /users/sunshine/mimic/replay/src/eventNotifier.cpp

CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic/replay/src/eventNotifier.cpp > CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.i

CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic/replay/src/eventNotifier.cpp -o CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.s

CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o.requires:

.PHONY : CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o.requires

CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o.provides: CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o.requires
	$(MAKE) -f CMakeFiles/mimiclib.dir/build.make CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o.provides.build
.PHONY : CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o.provides

CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o.provides.build: CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o


CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o: CMakeFiles/mimiclib.dir/flags.make
CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o: src/eventQueue.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o -c /users/sunshine/mimic/replay/src/eventQueue.cpp

CMakeFiles/mimiclib.dir/src/eventQueue.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mimiclib.dir/src/eventQueue.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic/replay/src/eventQueue.cpp > CMakeFiles/mimiclib.dir/src/eventQueue.cpp.i

CMakeFiles/mimiclib.dir/src/eventQueue.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mimiclib.dir/src/eventQueue.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic/replay/src/eventQueue.cpp -o CMakeFiles/mimiclib.dir/src/eventQueue.cpp.s

CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o.requires:

.PHONY : CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o.requires

CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o.provides: CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o.requires
	$(MAKE) -f CMakeFiles/mimiclib.dir/build.make CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o.provides.build
.PHONY : CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o.provides

CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o.provides.build: CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o


CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o: CMakeFiles/mimiclib.dir/flags.make
CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o: src/eventHandler.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o -c /users/sunshine/mimic/replay/src/eventHandler.cpp

CMakeFiles/mimiclib.dir/src/eventHandler.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mimiclib.dir/src/eventHandler.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic/replay/src/eventHandler.cpp > CMakeFiles/mimiclib.dir/src/eventHandler.cpp.i

CMakeFiles/mimiclib.dir/src/eventHandler.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mimiclib.dir/src/eventHandler.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic/replay/src/eventHandler.cpp -o CMakeFiles/mimiclib.dir/src/eventHandler.cpp.s

CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o.requires:

.PHONY : CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o.requires

CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o.provides: CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o.requires
	$(MAKE) -f CMakeFiles/mimiclib.dir/build.make CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o.provides.build
.PHONY : CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o.provides

CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o.provides.build: CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o


CMakeFiles/mimiclib.dir/src/mimic.cpp.o: CMakeFiles/mimiclib.dir/flags.make
CMakeFiles/mimiclib.dir/src/mimic.cpp.o: src/mimic.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object CMakeFiles/mimiclib.dir/src/mimic.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mimiclib.dir/src/mimic.cpp.o -c /users/sunshine/mimic/replay/src/mimic.cpp

CMakeFiles/mimiclib.dir/src/mimic.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mimiclib.dir/src/mimic.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic/replay/src/mimic.cpp > CMakeFiles/mimiclib.dir/src/mimic.cpp.i

CMakeFiles/mimiclib.dir/src/mimic.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mimiclib.dir/src/mimic.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic/replay/src/mimic.cpp -o CMakeFiles/mimiclib.dir/src/mimic.cpp.s

CMakeFiles/mimiclib.dir/src/mimic.cpp.o.requires:

.PHONY : CMakeFiles/mimiclib.dir/src/mimic.cpp.o.requires

CMakeFiles/mimiclib.dir/src/mimic.cpp.o.provides: CMakeFiles/mimiclib.dir/src/mimic.cpp.o.requires
	$(MAKE) -f CMakeFiles/mimiclib.dir/build.make CMakeFiles/mimiclib.dir/src/mimic.cpp.o.provides.build
.PHONY : CMakeFiles/mimiclib.dir/src/mimic.cpp.o.provides

CMakeFiles/mimiclib.dir/src/mimic.cpp.o.provides.build: CMakeFiles/mimiclib.dir/src/mimic.cpp.o


CMakeFiles/mimiclib.dir/src/connections.cpp.o: CMakeFiles/mimiclib.dir/flags.make
CMakeFiles/mimiclib.dir/src/connections.cpp.o: src/connections.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object CMakeFiles/mimiclib.dir/src/connections.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mimiclib.dir/src/connections.cpp.o -c /users/sunshine/mimic/replay/src/connections.cpp

CMakeFiles/mimiclib.dir/src/connections.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mimiclib.dir/src/connections.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic/replay/src/connections.cpp > CMakeFiles/mimiclib.dir/src/connections.cpp.i

CMakeFiles/mimiclib.dir/src/connections.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mimiclib.dir/src/connections.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic/replay/src/connections.cpp -o CMakeFiles/mimiclib.dir/src/connections.cpp.s

CMakeFiles/mimiclib.dir/src/connections.cpp.o.requires:

.PHONY : CMakeFiles/mimiclib.dir/src/connections.cpp.o.requires

CMakeFiles/mimiclib.dir/src/connections.cpp.o.provides: CMakeFiles/mimiclib.dir/src/connections.cpp.o.requires
	$(MAKE) -f CMakeFiles/mimiclib.dir/build.make CMakeFiles/mimiclib.dir/src/connections.cpp.o.provides.build
.PHONY : CMakeFiles/mimiclib.dir/src/connections.cpp.o.provides

CMakeFiles/mimiclib.dir/src/connections.cpp.o.provides.build: CMakeFiles/mimiclib.dir/src/connections.cpp.o


CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o: CMakeFiles/mimiclib.dir/flags.make
CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o: src/fileWorker.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o -c /users/sunshine/mimic/replay/src/fileWorker.cpp

CMakeFiles/mimiclib.dir/src/fileWorker.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mimiclib.dir/src/fileWorker.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic/replay/src/fileWorker.cpp > CMakeFiles/mimiclib.dir/src/fileWorker.cpp.i

CMakeFiles/mimiclib.dir/src/fileWorker.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mimiclib.dir/src/fileWorker.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic/replay/src/fileWorker.cpp -o CMakeFiles/mimiclib.dir/src/fileWorker.cpp.s

CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o.requires:

.PHONY : CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o.requires

CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o.provides: CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o.requires
	$(MAKE) -f CMakeFiles/mimiclib.dir/build.make CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o.provides.build
.PHONY : CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o.provides

CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o.provides.build: CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o


# Object files for target mimiclib
mimiclib_OBJECTS = \
"CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o" \
"CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o" \
"CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o" \
"CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o" \
"CMakeFiles/mimiclib.dir/src/mimic.cpp.o" \
"CMakeFiles/mimiclib.dir/src/connections.cpp.o" \
"CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o"

# External object files for target mimiclib
mimiclib_EXTERNAL_OBJECTS =

libmimiclib.a: CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o
libmimiclib.a: CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o
libmimiclib.a: CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o
libmimiclib.a: CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o
libmimiclib.a: CMakeFiles/mimiclib.dir/src/mimic.cpp.o
libmimiclib.a: CMakeFiles/mimiclib.dir/src/connections.cpp.o
libmimiclib.a: CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o
libmimiclib.a: CMakeFiles/mimiclib.dir/build.make
libmimiclib.a: CMakeFiles/mimiclib.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Linking CXX static library libmimiclib.a"
	$(CMAKE_COMMAND) -P CMakeFiles/mimiclib.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/mimiclib.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/mimiclib.dir/build: libmimiclib.a

.PHONY : CMakeFiles/mimiclib.dir/build

CMakeFiles/mimiclib.dir/requires: CMakeFiles/mimiclib.dir/src/pollHandler.cpp.o.requires
CMakeFiles/mimiclib.dir/requires: CMakeFiles/mimiclib.dir/src/eventNotifier.cpp.o.requires
CMakeFiles/mimiclib.dir/requires: CMakeFiles/mimiclib.dir/src/eventQueue.cpp.o.requires
CMakeFiles/mimiclib.dir/requires: CMakeFiles/mimiclib.dir/src/eventHandler.cpp.o.requires
CMakeFiles/mimiclib.dir/requires: CMakeFiles/mimiclib.dir/src/mimic.cpp.o.requires
CMakeFiles/mimiclib.dir/requires: CMakeFiles/mimiclib.dir/src/connections.cpp.o.requires
CMakeFiles/mimiclib.dir/requires: CMakeFiles/mimiclib.dir/src/fileWorker.cpp.o.requires

.PHONY : CMakeFiles/mimiclib.dir/requires

CMakeFiles/mimiclib.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/mimiclib.dir/cmake_clean.cmake
.PHONY : CMakeFiles/mimiclib.dir/clean

CMakeFiles/mimiclib.dir/depend:
	cd /users/sunshine/mimic/replay && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /users/sunshine/mimic/replay /users/sunshine/mimic/replay /users/sunshine/mimic/replay /users/sunshine/mimic/replay /users/sunshine/mimic/replay/CMakeFiles/mimiclib.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/mimiclib.dir/depend
