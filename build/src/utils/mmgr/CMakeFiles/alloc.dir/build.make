# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.27

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
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.27.4/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.27.4/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/zlh/Documents/Git/rdbms

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/zlh/Documents/Git/rdbms/build

# Include any dependencies generated for this target.
include src/utils/mmgr/CMakeFiles/alloc.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include src/utils/mmgr/CMakeFiles/alloc.dir/compiler_depend.make

# Include the progress variables for this target.
include src/utils/mmgr/CMakeFiles/alloc.dir/progress.make

# Include the compile flags for this target's objects.
include src/utils/mmgr/CMakeFiles/alloc.dir/flags.make

src/utils/mmgr/CMakeFiles/alloc.dir/alloc.cc.o: src/utils/mmgr/CMakeFiles/alloc.dir/flags.make
src/utils/mmgr/CMakeFiles/alloc.dir/alloc.cc.o: /Users/zlh/Documents/Git/rdbms/src/utils/mmgr/alloc.cc
src/utils/mmgr/CMakeFiles/alloc.dir/alloc.cc.o: src/utils/mmgr/CMakeFiles/alloc.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/zlh/Documents/Git/rdbms/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/utils/mmgr/CMakeFiles/alloc.dir/alloc.cc.o"
	cd /Users/zlh/Documents/Git/rdbms/build/src/utils/mmgr && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/utils/mmgr/CMakeFiles/alloc.dir/alloc.cc.o -MF CMakeFiles/alloc.dir/alloc.cc.o.d -o CMakeFiles/alloc.dir/alloc.cc.o -c /Users/zlh/Documents/Git/rdbms/src/utils/mmgr/alloc.cc

src/utils/mmgr/CMakeFiles/alloc.dir/alloc.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/alloc.dir/alloc.cc.i"
	cd /Users/zlh/Documents/Git/rdbms/build/src/utils/mmgr && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/zlh/Documents/Git/rdbms/src/utils/mmgr/alloc.cc > CMakeFiles/alloc.dir/alloc.cc.i

src/utils/mmgr/CMakeFiles/alloc.dir/alloc.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/alloc.dir/alloc.cc.s"
	cd /Users/zlh/Documents/Git/rdbms/build/src/utils/mmgr && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/zlh/Documents/Git/rdbms/src/utils/mmgr/alloc.cc -o CMakeFiles/alloc.dir/alloc.cc.s

# Object files for target alloc
alloc_OBJECTS = \
"CMakeFiles/alloc.dir/alloc.cc.o"

# External object files for target alloc
alloc_EXTERNAL_OBJECTS =

src/utils/mmgr/liballoc.a: src/utils/mmgr/CMakeFiles/alloc.dir/alloc.cc.o
src/utils/mmgr/liballoc.a: src/utils/mmgr/CMakeFiles/alloc.dir/build.make
src/utils/mmgr/liballoc.a: src/utils/mmgr/CMakeFiles/alloc.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/Users/zlh/Documents/Git/rdbms/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library liballoc.a"
	cd /Users/zlh/Documents/Git/rdbms/build/src/utils/mmgr && $(CMAKE_COMMAND) -P CMakeFiles/alloc.dir/cmake_clean_target.cmake
	cd /Users/zlh/Documents/Git/rdbms/build/src/utils/mmgr && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/alloc.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/utils/mmgr/CMakeFiles/alloc.dir/build: src/utils/mmgr/liballoc.a
.PHONY : src/utils/mmgr/CMakeFiles/alloc.dir/build

src/utils/mmgr/CMakeFiles/alloc.dir/clean:
	cd /Users/zlh/Documents/Git/rdbms/build/src/utils/mmgr && $(CMAKE_COMMAND) -P CMakeFiles/alloc.dir/cmake_clean.cmake
.PHONY : src/utils/mmgr/CMakeFiles/alloc.dir/clean

src/utils/mmgr/CMakeFiles/alloc.dir/depend:
	cd /Users/zlh/Documents/Git/rdbms/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/zlh/Documents/Git/rdbms /Users/zlh/Documents/Git/rdbms/src/utils/mmgr /Users/zlh/Documents/Git/rdbms/build /Users/zlh/Documents/Git/rdbms/build/src/utils/mmgr /Users/zlh/Documents/Git/rdbms/build/src/utils/mmgr/CMakeFiles/alloc.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : src/utils/mmgr/CMakeFiles/alloc.dir/depend

