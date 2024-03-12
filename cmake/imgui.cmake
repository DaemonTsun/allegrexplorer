
# includes imgui and defines the following variables:
# imgui_SOURCES_DIR:  imgui project path
# imgui_SOURCES:      list of source files to be compiled into imgui_LIBRARIES
# imgui_INCLUDE_DIRS: imgui include directories
# imgui_LIBRARIES:    library target name

set(imgui_SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/")

if (NOT EXISTS "${imgui_SOURCES_DIR}/imgui.h")
    execute_process(COMMAND git submodule update --init "${imgui_SOURCES_DIR}"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

set(imgui_SOURCES 
    "${imgui_SOURCES_DIR}/imgui.h"
    "${imgui_SOURCES_DIR}/imgui.cpp"

    "${imgui_SOURCES_DIR}/imgui_draw.cpp"
    "${imgui_SOURCES_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCES_DIR}/imgui_tables.cpp"

    "${imgui_SOURCES_DIR}/backends/imgui_impl_opengl3.cpp"
)

if (Debug)
    list(APPEND imgui_SOURCES "${imgui_SOURCES_DIR}/imgui_demo.cpp")
endif()

set(imgui_INCLUDE_DIRS "${imgui_SOURCES_DIR}")

if (DEFINED GLFW_INCLUDE_DIRS)
    list(APPEND imgui_SOURCES "${imgui_SOURCES_DIR}/backends/imgui_impl_glfw.cpp")
    list(APPEND imgui_INCLUDE_DIRS ${GLFW_INCLUDE_DIRS})
endif()

add_library(imgui STATIC)
target_sources(imgui PRIVATE ${imgui_SOURCES})
target_include_directories(imgui PRIVATE ${imgui_INCLUDE_DIRS})

set(imgui_LIBRARIES imgui)
