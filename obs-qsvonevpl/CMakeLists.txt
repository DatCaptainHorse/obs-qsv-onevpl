option(ENABLE_HEVC "Enable HEVC support" OFF)

add_library(obs-qsvonevpl MODULE)

find_package(VPL CONFIG)
find_package(libobs CONFIG)

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

target_link_libraries(obs-qsvonevpl PRIVATE VPL::dispatcher oneVPL-private OBS::libobs)

# cmake-format: off
set_target_properties(obs-qsvonevpl PROPERTIES FOLDER plugins/obs-qsvonevpl PREFIX "")
# cmake-format: on

# ONEVPL_EXPERIMENTAL
target_compile_definitions(
  obs-qsvonevpl
  PRIVATE
  ONEVPL_EXPERIMENTAL
  MFXVIDEO_CPP_ENABLE_MFXLOAD
  $<$<BOOL:${ENABLE_HEVC}>:ENABLE_HEVC=1>
)

# C++20 for C-like structs
target_compile_features(obs-qsvonevpl PRIVATE cxx_std_20)

set_target_properties(obs-qsvonevpl PROPERTIES FOLDER "plugins/obs-qsvonevpl")

# Windows
if(WIN32)
  add_subdirectory(obs-qsvonevpl-test)
  target_compile_definitions(obs-qsvonevpl PRIVATE DX11_D3D)

  target_sources(
    obs-qsvonevpl
    PRIVATE # cmake-format: sortable
    bits/windows_defs.h
    helpers/common_directx11.cpp
    helpers/common_directx11.hpp
    helpers/common_utils_windows.cpp)

  target_link_libraries(obs-qsvonevpl PRIVATE d3d11 dxgi dxguid)

  # Linux
elseif(UNIX AND NOT APPLE)
  target_sources(
    obs-qsvonevpl
    PRIVATE # cmake-format: sortable
    bits/linux_defs.h
    helpers/common_utils_linux.cpp)

  target_link_libraries(obs-qsvonevpl PRIVATE va va-drm)
endif()
