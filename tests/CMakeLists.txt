# All tests are EXCLUDE_FROM_ALL: build them with --target, or use the `check` target.

set (ORIGINAL ${CMAKE_SOURCE_DIR}/original)

# Johan Larsby's engine, as published, with the two portability patches.
add_custom_command (
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/engineWorkers.cpp
    COMMAND ${CMAKE_COMMAND} -DSRC=${ORIGINAL}/engineWorkers.cpp
            -DDST=${CMAKE_CURRENT_BINARY_DIR}/engineWorkers.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/reference/patch_original.cmake
    DEPENDS ${ORIGINAL}/engineWorkers.cpp reference/patch_original.cmake)

add_library (kerr_original STATIC EXCLUDE_FROM_ALL
    ${ORIGINAL}/engine.cpp ${CMAKE_CURRENT_BINARY_DIR}/engineWorkers.cpp)
target_include_directories (kerr_original PUBLIC ${ORIGINAL})
target_compile_options (kerr_original PRIVATE ${KERR_FP_FLAGS} -w
    -include ${CMAKE_CURRENT_SOURCE_DIR}/reference/shim.h)

add_executable (reference_check EXCLUDE_FROM_ALL reference/reference_check.cpp)
target_link_libraries (reference_check PRIVATE kerr_original kerr_engine)

add_executable (rand_check EXCLUDE_FROM_ALL rand_check.cpp)
target_link_libraries (rand_check PRIVATE kerr_engine)

add_executable (engine_cli EXCLUDE_FROM_ALL engine_cli.cpp)
target_link_libraries (engine_cli PRIVATE kerr_engine)

# The firmware compiles the engine as C++98; keep it that way.
add_library (cxx98_check OBJECT EXCLUDE_FROM_ALL cxx98_check.cpp)
target_link_libraries (cxx98_check PRIVATE kerr_engine)
set_target_properties (cxx98_check PROPERTIES CXX_STANDARD 98 CXX_EXTENSIONS ON)
target_compile_options (cxx98_check PRIVATE -Wall -Wextra -Werror -pedantic -Wno-long-long)

add_custom_target (check
    COMMAND rand_check
    COMMAND reference_check 300 1 ${ORIGINAL}/fjord.wav
    COMMAND reference_check 300 2
    DEPENDS rand_check reference_check cxx98_check
    USES_TERMINAL)
