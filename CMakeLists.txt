cmake_minimum_required (VERSION 3.22)
project (KerrStinn VERSION 2.0.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# FMA contraction moves slice boundaries by a sample; the engine must round
# exactly like the original.
set (KERR_FP_FLAGS -ffp-contract=off -fno-fast-math)

add_library (kerr_engine INTERFACE)
target_include_directories (kerr_engine INTERFACE engine)
target_compile_options (kerr_engine INTERFACE ${KERR_FP_FLAGS})

option (KERR_BUILD_PLUGIN "Build the JUCE plugin" ON)
if (KERR_BUILD_PLUGIN)
    add_subdirectory (plugin)
endif ()

add_subdirectory (tests)
