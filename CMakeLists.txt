cmake_minimum_required (VERSION 3.5)
project(VSCommandPromptHere)

# Set the C++ standard to C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable all compiler warnings
if (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

# Build the executable
add_executable(VSCommandPromptHere VSCommandPromptHere.cpp)
set_target_properties(VSCommandPromptHere PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
