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
include CMakeFiles/mimic.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/mimic.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/mimic.dir/flags.make

CMakeFiles/mimic.dir/src/main.cpp.o: CMakeFiles/mimic.dir/flags.make
CMakeFiles/mimic.dir/src/main.cpp.o: src/main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/mimic.dir/src/main.cpp.o"
	/usr/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mimic.dir/src/main.cpp.o -c /users/sunshine/mimic/replay/src/main.cpp

CMakeFiles/mimic.dir/src/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mimic.dir/src/main.cpp.i"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /users/sunshine/mimic/replay/src/main.cpp > CMakeFiles/mimic.dir/src/main.cpp.i

CMakeFiles/mimic.dir/src/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mimic.dir/src/main.cpp.s"
	/usr/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /users/sunshine/mimic/replay/src/main.cpp -o CMakeFiles/mimic.dir/src/main.cpp.s

CMakeFiles/mimic.dir/src/main.cpp.o.requires:

.PHONY : CMakeFiles/mimic.dir/src/main.cpp.o.requires

CMakeFiles/mimic.dir/src/main.cpp.o.provides: CMakeFiles/mimic.dir/src/main.cpp.o.requires
	$(MAKE) -f CMakeFiles/mimic.dir/build.make CMakeFiles/mimic.dir/src/main.cpp.o.provides.build
.PHONY : CMakeFiles/mimic.dir/src/main.cpp.o.provides

CMakeFiles/mimic.dir/src/main.cpp.o.provides.build: CMakeFiles/mimic.dir/src/main.cpp.o


# Object files for target mimic
mimic_OBJECTS = \
"CMakeFiles/mimic.dir/src/main.cpp.o"

# External object files for target mimic
mimic_EXTERNAL_OBJECTS =

mimic: CMakeFiles/mimic.dir/src/main.cpp.o
mimic: CMakeFiles/mimic.dir/build.make
mimic: libmimiclib.a
mimic: CMakeFiles/mimic.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/users/sunshine/mimic/replay/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable mimic"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/mimic.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/mimic.dir/build: mimic

.PHONY : CMakeFiles/mimic.dir/build

CMakeFiles/mimic.dir/requires: CMakeFiles/mimic.dir/src/main.cpp.o.requires

.PHONY : CMakeFiles/mimic.dir/requires

CMakeFiles/mimic.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/mimic.dir/cmake_clean.cmake
.PHONY : CMakeFiles/mimic.dir/clean

CMakeFiles/mimic.dir/depend:
	cd /users/sunshine/mimic/replay && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /users/sunshine/mimic/replay /users/sunshine/mimic/replay /users/sunshine/mimic/replay /users/sunshine/mimic/replay /users/sunshine/mimic/replay/CMakeFiles/mimic.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/mimic.dir/depend
