cmake_minimum_required(VERSION 3.28)
project(cloudbus-segment)

# cloudbus-segment requires at least C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories for build and distribution.
set(INCLUDE_DIRS
  "${CMAKE_SOURCE_DIR}/include/"
)

# Enable testing by default if this is a top-level project or
# in submodules if the project has explicitly set BUILD_TESTING
# by including CTest.
if ((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME) OR BUILD_TESTING)
	set(CB_SEGMENT_BUILD_TESTING_DEFAULT ON)
else()
	set(CB_SEGMENT_BUILD_TESTING_DEFAULT OFF)
endif()
option(CB_SEGMENT_BUILD_TESTING "Enable testing." ${CB_SEGMENT_BUILD_TESTING_DEFAULT})
if (CB_SEGMENT_BUILD_TESTING)
  # Enable tests.
  include(CTest)
endif()
