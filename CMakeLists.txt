cmake_minimum_required(VERSION 3.24...3.25)

project(obs-qsv-onevpl
  VERSION 1.1.0.0
  LANGUAGES C CXX
)

set(BUILD_DEV_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory(oneVPL)
add_subdirectory(obs-qsvonevpl)
