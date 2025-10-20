set(segmentlib_SOURCES
  async_service.cpp
  segment_service.cpp
)

add_library(
  segmentlib
  OBJECT
  ${segmentlib_SOURCES}
)
target_include_directories(
  segmentlib
  PRIVATE
  ${INCLUDE_DIRS}
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(
  segment
  main.cpp
)
target_include_directories(
  segment
  PRIVATE
  ${INCLUDE_DIRS}
)
target_link_libraries(
  segment
  PRIVATE
  $<TARGET_OBJECTS:segmentlib>
)
