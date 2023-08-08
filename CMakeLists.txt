cmake_minimum_required(VERSION 3.24...3.25)

project(obs-qsv-onevpl
  VERSION 1.1.0.0
  LANGUAGES CXX
)

# VPL
# If Linux, disable tools, for some reason Windows requires them?
if(UNIX)
  set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(oneVPL)

# VPL-private (experimental / hidden stuff)
add_subdirectory(oneVPL-private)

# Actual plugin
add_subdirectory(obs-qsvonevpl)
