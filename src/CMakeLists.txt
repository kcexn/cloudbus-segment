file(GLOB
  segmentlib_SOURCES
  *.cpp
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
