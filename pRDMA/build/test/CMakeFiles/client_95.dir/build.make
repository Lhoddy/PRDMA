# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/cmake-3.18.1/bin/cmake

# The command to remove a file.
RM = /usr/local/cmake-3.18.1/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/hdlu/model_test/nnew_client

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/hdlu/model_test/nnew_client/build

# Include any dependencies generated for this target.
include test/CMakeFiles/client_95.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/client_95.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/client_95.dir/flags.make

test/CMakeFiles/client_95.dir/client_95.c.o: test/CMakeFiles/client_95.dir/flags.make
test/CMakeFiles/client_95.dir/client_95.c.o: ../test/client_95.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/hdlu/model_test/nnew_client/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object test/CMakeFiles/client_95.dir/client_95.c.o"
	cd /home/hdlu/model_test/nnew_client/build/test && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/client_95.dir/client_95.c.o -c /home/hdlu/model_test/nnew_client/test/client_95.c

test/CMakeFiles/client_95.dir/client_95.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/client_95.dir/client_95.c.i"
	cd /home/hdlu/model_test/nnew_client/build/test && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/hdlu/model_test/nnew_client/test/client_95.c > CMakeFiles/client_95.dir/client_95.c.i

test/CMakeFiles/client_95.dir/client_95.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/client_95.dir/client_95.c.s"
	cd /home/hdlu/model_test/nnew_client/build/test && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/hdlu/model_test/nnew_client/test/client_95.c -o CMakeFiles/client_95.dir/client_95.c.s

# Object files for target client_95
client_95_OBJECTS = \
"CMakeFiles/client_95.dir/client_95.c.o"

# External object files for target client_95
client_95_EXTERNAL_OBJECTS =

../bin/client_95: test/CMakeFiles/client_95.dir/client_95.c.o
../bin/client_95: test/CMakeFiles/client_95.dir/build.make
../bin/client_95: ../lib/libdhmp.a
../bin/client_95: test/CMakeFiles/client_95.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/hdlu/model_test/nnew_client/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable ../../bin/client_95"
	cd /home/hdlu/model_test/nnew_client/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/client_95.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/client_95.dir/build: ../bin/client_95

.PHONY : test/CMakeFiles/client_95.dir/build

test/CMakeFiles/client_95.dir/clean:
	cd /home/hdlu/model_test/nnew_client/build/test && $(CMAKE_COMMAND) -P CMakeFiles/client_95.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/client_95.dir/clean

test/CMakeFiles/client_95.dir/depend:
	cd /home/hdlu/model_test/nnew_client/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/hdlu/model_test/nnew_client /home/hdlu/model_test/nnew_client/test /home/hdlu/model_test/nnew_client/build /home/hdlu/model_test/nnew_client/build/test /home/hdlu/model_test/nnew_client/build/test/CMakeFiles/client_95.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/client_95.dir/depend

