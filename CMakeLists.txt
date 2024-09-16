cmake_minimum_required(VERSION 3.20)
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/better-cmake/cmake/betterConfig.cmake")
    execute_process(COMMAND git submodule update --init "${CMAKE_CURRENT_SOURCE_DIR}/ext/better-cmake" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}") 
endif()
find_package(better REQUIRED NO_DEFAULT_PATH PATHS ./ext/better-cmake/cmake/)

project(allegrexplorer)

set(fs_COMPILE_DEFINITIONS @Windows)

add_exe(allegrexplorer
    VERSION 0.1.2
    SOURCES_DIR "${ROOT}/src"
    GENERATE_TARGET_HEADER "${ROOT}/src/allegrexplorer_info.hpp"
    CPP_VERSION 20
    CPP_WARNINGS ALL SANE FATAL
                 @MSVC /wd5219 # int -> float conv, needed for imgui
    LIBRARIES @Windows shell32 user32 gdi32
    COMPILE_DEFINITIONS ${fs_COMPILE_DEFINITIONS}

    EXT
        LIB shl      0.10.0 "${ROOT}/ext/shl" INCLUDE LINK GIT_SUBMODULE
        LIB fs       0.9.0  "${ROOT}/ext/fs"  INCLUDE LINK GIT_SUBMODULE
        LIB window-base  1  "${ROOT}/ext/window-base" INCLUDE LINK GIT_SUBMODULE
        LIB allegrex 1.0.4  "${ROOT}/ext/liballegrex" INCLUDE LINK GIT_SUBMODULE
    )

target_include_directories(${allegrexplorer_TARGET} PRIVATE ${window-base-1_INCLUDE_DIRS})

# run
add_custom_target("run" COMMAND "${ROOT_BIN}/${allegrexplorer_TARGET}")
