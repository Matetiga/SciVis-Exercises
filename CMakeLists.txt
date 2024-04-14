 
cmake_minimum_required(VERSION 3.24)
set(COURSE_NAME "SciVis")
project(${COURSE_NAME} C CXX)

# pre-set CGV Framework options
option(CGV_BUILD_EXAMPLES off)
option(CGV_BUILD_WITH_AUDIO off)
option(CGV_STDCPP17 on)

# Include the CGV Framework
if (DEFINED ENV{CGV_DIR})
	# Use system-installed CGV Framework if present
	set(CGV_DIR $ENV{CGV_DIR})
	message("Using CGV Framework source found at:")
	message("> ${CGV_DIR}")
	add_subdirectory(${CGV_DIR} ./cgv)
else()
	# Use CGV Framework bundled in form of the Git submodule
	add_subdirectory(${CMAKE_SOURCE_DIR}/cgv)
endif()

# Add exercises
# - configure for common root
set(SCIVIS_ROOT_DIR ${CMAKE_SOURCE_DIR})
# - add exercises
add_subdirectory("4-volume")
add_subdirectory("3-terrain")
add_subdirectory("2-particle")
add_subdirectory("1-stereo")
add_subdirectory("0-intro")

# Visual Studio fluff
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT task0_framework)
