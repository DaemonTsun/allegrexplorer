cmake_minimum_required(VERSION 3.20)
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/better-cmake/cmake/betterConfig.cmake")
    execute_process(COMMAND git submodule update --init "${CMAKE_CURRENT_SOURCE_DIR}/ext/better-cmake" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}") 
endif()
find_package(better REQUIRED NO_DEFAULT_PATH PATHS ./ext/better-cmake/cmake/)

project(allegrexplorer)

add_exe(allegrexplorer
    VERSION 0.0.1
    SOURCES_DIR "${ROOT}/src"
    GENERATE_TARGET_HEADER "${ROOT}/src/allegrexplorer_info.hpp"
    CPP_VERSION 17
    CPP_WARNINGS ALL SANE FATAL
    INCLUDE_DIRS "${ROOT}/ext/mg/ext/imgui/"
    EXT
        LIB shl 0.7.1 "${ROOT}/ext/shl" INCLUDE LINK
        LIB mg  0.8.0 "${ROOT}/ext/mg"  INCLUDE LINK
        LIB allegrex 1.0.2 "${ROOT}/ext/liballegrex" INCLUDE LINK
    )

# run
add_custom_target("run_${allegrexplorer_TARGET}" COMMAND "${ROOT_BIN}/${allegrexplorer_TARGET}")
