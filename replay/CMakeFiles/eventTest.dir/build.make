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
CMAKE_SOURCE_DIR = /users/sunshine/mimic-generator

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /users/sunshine/mimic-generator

# Include any dependencies generated for this target.
include CMakeFiles/eventTest.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/eventTest.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/eventTest.dir/flags.make

CMakeFiles/eventTest.dir/src/eventTest.cpp.o: CMakeFiles/eventTest.dir/flags.make
CMakeFiles/eventTest.dir/src/eventTest.cpp.o: src/eventTest.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic-generator/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/eventTest.dir/src/eventTest.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/eventTest.dir/src/eventTest.cpp.o -c /users/sunshine/mimic-generator/src/eventTest.cpp

CMakeFiles/eventTest.dir/src/eventTest.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/eventTest.dir/src/eventTest.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic-generator/src/eventTest.cpp > CMakeFiles/eventTest.dir/src/eventTest.cpp.i

CMakeFiles/eventTest.dir/src/eventTest.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/eventTest.dir/src/eventTest.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic-generator/src/eventTest.cpp -o CMakeFiles/eventTest.dir/src/eventTest.cpp.s

CMakeFiles/eventTest.dir/src/eventTest.cpp.o.requires:

.PHONY : CMakeFiles/eventTest.dir/src/eventTest.cpp.o.requires

CMakeFiles/eventTest.dir/src/eventTest.cpp.o.provides: CMakeFiles/eventTest.dir/src/eventTest.cpp.o.requires
	$(MAKE) -f CMakeFiles/eventTest.dir/build.make CMakeFiles/eventTest.dir/src/eventTest.cpp.o.provides.build
.PHONY : CMakeFiles/eventTest.dir/src/eventTest.cpp.o.provides

CMakeFiles/eventTest.dir/src/eventTest.cpp.o.provides.build: CMakeFiles/eventTest.dir/src/eventTest.cpp.o


# Object files for target eventTest
eventTest_OBJECTS = \
"CMakeFiles/eventTest.dir/src/eventTest.cpp.o"

# External object files for target eventTest
eventTest_EXTERNAL_OBJECTS =

eventTest: CMakeFiles/eventTest.dir/src/eventTest.cpp.o
eventTest: CMakeFiles/eventTest.dir/build.make
eventTest: libmimiclib.a
eventTest: CMakeFiles/eventTest.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/users/sunshine/mimic-generator/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable eventTest"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/eventTest.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/eventTest.dir/build: eventTest

.PHONY : CMakeFiles/eventTest.dir/build

CMakeFiles/eventTest.dir/requires: CMakeFiles/eventTest.dir/src/eventTest.cpp.o.requires

.PHONY : CMakeFiles/eventTest.dir/requires

CMakeFiles/eventTest.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/eventTest.dir/cmake_clean.cmake
.PHONY : CMakeFiles/eventTest.dir/clean

CMakeFiles/eventTest.dir/depend:
	cd /users/sunshine/mimic-generator && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /users/sunshine/mimic-generator /users/sunshine/mimic-generator /users/sunshine/mimic-generator /users/sunshine/mimic-generator /users/sunshine/mimic-generator/CMakeFiles/eventTest.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/eventTest.dir/depend

