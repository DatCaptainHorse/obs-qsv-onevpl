cmake_minimum_required(VERSION 3.24...3.25)

project(obs-qsv-onevpl
  VERSION 1.1.0.0
  LANGUAGES CXX
)

# VPL
set(BUILD_DISPATCHER_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory(oneVPL)

# VPL-private (experimental / hidden stuff)
add_subdirectory(oneVPL-private)

# Actual plugin
add_subdirectory(obs-qsvonevpl)
