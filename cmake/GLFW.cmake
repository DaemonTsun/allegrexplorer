
# includes GLFW and defines the following variables:
# GLFW_SOURCES_DIR:  GLFW project path
# GLFW_INCLUDE_DIRS: GLFW include directories
# GLFW_LIBRARIES:    library target name

set(GLFW_SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/glfw")

if (NOT EXISTS "${GLFW_SOURCES_DIR}/CMakeLists.txt")
    execute_process(COMMAND git submodule update --init "${GLFW_SOURCES_DIR}"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

set(GLFW_LIBRARY_TYPE STATIC)
set(GLFW_BUILD_DOCS FALSE)

add_subdirectory("${GLFW_SOURCES_DIR}")

if (NOT DEFINED GLFW_LIBRARIES)
    set(GLFW_LIBRARIES glfw)
endif()

set(GLFW_INCLUDE_DIRS "${GLFW_SOURCES_DIR}/include")
