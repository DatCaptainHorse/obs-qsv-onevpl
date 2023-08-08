option(ENABLE_HEVC "Enable HEVC support" OFF)

add_library(obs-qsvonevpl MODULE)

find_package(libobs REQUIRED)

target_sources(
  obs-qsvonevpl
  PRIVATE # cmake-format: sortable
  obs-qsv-onevpl-plugin-main.cpp
  obs-qsv-onevpl-plugin-init.cpp
      obs-qsv-onevpl-encoder.cpp
  obs-qsv-onevpl-encoder.hpp
	  obs-qsv-onevpl-encoder-internal.cpp
  obs-qsv-onevpl-encoder-internal.hpp
      helpers/common_utils.cpp
  helpers/common_utils.hpp
      helpers/ext_buf_manager.hpp
      helpers/brc_routines.cpp
      helpers/brc_routines.hpp
)

target_link_libraries(obs-qsvonevpl PRIVATE OBS::libobs)

# cmake-format: off
set_target_properties(obs-qsvonevpl PROPERTIES FOLDER plugins/obs-qsvonevpl PREFIX "")
# cmake-format: on

# ONEVPL_EXPERIMENTAL
target_compile_definitions(
  obs-qsvonevpl
  PRIVATE
  MFXVIDEO_CPP_ENABLE_MFXLOAD
  $<$<BOOL:${UNIX}>:ONEVPL_EXPERIMENTAL>
  $<$<BOOL:${ENABLE_HEVC}>:ENABLE_HEVC=1>
)

set_target_properties(obs-qsvonevpl PROPERTIES FOLDER "plugins/obs-qsvonevpl")

file(GLOB _OBS_QSVONEVPL_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB _OBS_QSVONEVPL_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

source_group("obs-qsvonevpl\\Source Files" FILES ${_OBS_QSVONEVPL_SOURCE_FILES})
source_group("obs-qsvonevpl\\Header Files" FILES ${_OBS_QSVONEVPL_HEADER_FILES})

file(GLOB _OBS_QSVHELPERS_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.cpp)
file(GLOB _OBS_QSVHELPERS_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.hpp)

source_group("obs-qsvonevpl-helpers\\Source Files" FILES ${_OBS_QSVHELPERS_SOURCE_FILES})
source_group("obs-qsvonevpl-helpers\\Header Files" FILES ${_OBS_QSVHELPERS_HEADER_FILES})

# Windows
if(WIN32)
  add_subdirectory(obs-qsvonevpl-test)

get_target_property(mfx_sources libvpl SOURCES)
  list(FILTER mfx_sources INCLUDE REGEX ".*\\.(m|c[cp]?p?|swift)")
  list(FILTER mfx_headers INCLUDE REGEX ".*\\.h(pp)?")
  source_group("libvpl\\Source Files" FILES ${mfx_sources})
  source_group("libvpl\\Header Files" FILES ${mfx_headers})

 target_compile_definitions(obs-qsvonevpl PRIVATE DX11_D3D)

target_sources(
    obs-qsvonevpl
    PRIVATE # cmake-format: sortable
            bits/windows_defs.h
            helpers/common_directx11.cpp
            helpers/common_directx11.hpp
            helpers/common_utils_windows.cpp)

target_link_libraries(obs-qsvonevpl PRIVATE libvpl d3d11 dxgi dxguid OBS::COMutils)

# Linux
elseif(UNIX AND NOT APPLE)
	#find_package(VPL REQUIRED)
  target_sources(
    obs-qsvonevpl
    PRIVATE # cmake-format: sortable
    bits/linux_defs.h
    helpers/common_utils_linux.cpp)

  find_package(VPL REQUIRED)
	target_link_libraries(obs-qsvonevpl PRIVATE VPL::dispatcher oneVPL-private va va-drm)
endif()

get_target_property(target_sources obs-qsvonevpl SOURCES)
list(FILTER target_sources INCLUDE REGEX ".*\\.(m|c[cp]?p?|swift)")
list(FILTER target_headers INCLUDE REGEX ".*\\.h(pp)?")
source_group("obs-qsvonevpl\\Source Files" FILES ${target_sources})
source_group("obs-qsvonevpl\\Header Files" FILES ${target_headers})