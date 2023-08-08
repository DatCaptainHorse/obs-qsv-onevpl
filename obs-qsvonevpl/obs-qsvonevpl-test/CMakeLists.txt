cmake_minimum_required(VERSION 3.24...3.25)

add_executable(obs-qsvonevpl-test)
target_sources(obs-qsvonevpl-test PRIVATE obs-qsv-test.cpp)
target_compile_definitions(obs-qsvonevpl-test PRIVATE "$<$<BOOL:${ENABLE_HEVC}>:ENABLE_HEVC>")
target_link_libraries(obs-qsvonevpl-test PRIVATE VPL oneVPL-private d3d11 dxgi dxguid)

# cmake-format: off
set_target_properties(obs-qsvonevpl-test PROPERTIES FOLDER plugins/obs-qsvonevpl PREFIX "")
# cmake-format: on

# C++17 for structured bindings
target_compile_features(obs-qsvonevpl-test PRIVATE cxx_std_17)
