#cmake_minimum_required(VERSION 3.5)
#set(CMAKE_CXX_STANDARD 14)            # Enable c++14 standard
#set(IMGUI_DIR )
#
#project (
#        MDM
#        DESCRIPTION "Motor driver monitoring with ImGui and SOEM"
#        LANGUAGES CXX)
#
### Find dependencies
#find_package(glfw3 REQUIRED)
#include_directories(./libs/imgui)
## Use  cmake -S . -B build/ -DOpenGL_GL_PREFERENCE=GLVND
## to skip legacy OpenGL library
#set(OpenGL_GL_PREFERENCE GLVND)
#find_package(OpenGL REQUIRED)
#set(SOURCE_FILES ./libs/imgui/imgui.cpp ./libs/imgui/imgui_draw.cpp ./libs/imgui/imgui_demo.cpp ./libs/imgui/imgui_tables.cpp ./libs/imgui/imgui_widgets.cpp) # Add all .cpp files of project root directory as source files
#include_directories(./libs/imgui/backends)
#add_library(imgui ${SOURCE_FILES})
#set(BACKEND_FILE ./libs/imgui/backends/imgui_impl_glfw.cpp)
#add_library(imgui_backend_glfw ${BACKEND_FILE})
#set(BACKEND_FILE ./libs/imgui/backends/imgui_impl_opengl3.cpp)
#add_library(imgui_backend_opengl3 ${BACKEND_FILE})
#find_package(PkgConfig REQUIRED)    # For .pc packages
#
#
#set(EXAMPLE_MAIN_FILE ./src/main.cpp)
#add_executable(MDM ${EXAMPLE_MAIN_FILE})
#target_link_libraries(MDM PUBLIC imgui imgui_backend_glfw imgui_backend_opengl3 glfw OpenGL)
#
#
cmake_minimum_required(VERSION 3.5)


## project
project (
        ImGui-GLFW-OpenGL3
        DESCRIPTION "ImGui Sample with GLFW + OpenGL3"
        LANGUAGES CXX)

## Find dependencies
find_package(glfw3 REQUIRED)

# Use  cmake -S . -B build/ -DOpenGL_GL_PREFERENCE=GLVND
# to skip legacy OpenGL library
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

set(ImGUI_DIR ./libs/imgui)

include_directories(./libs/imgui/backends)
include_directories(./libs/imgui)

set(BACKEND_FILE ./libs/imgui/backends/imgui_impl_glfw.cpp)
add_library(imgui_backend_glfw ${BACKEND_FILE})
set(BACKEND_FILE ./libs/imgui/backends/imgui_impl_opengl3.cpp)
add_library(imgui_backend_opengl3 ${BACKEND_FILE})

set(BACKEND_FILE ./libs/imgui/implot/implot.cpp ./libs/imgui/implot/implot_demo.cpp ./libs/imgui/implot/implot_items.cpp)
add_library(implot ${BACKEND_FILE})
set(BACKEND_FILE ./libs/imgui/imgui-knobs/imgui-knobs.cpp)
add_library(imknobs ${BACKEND_FILE})

## Create main executable
add_executable(MDM)

target_include_directories(
        MDM
        PRIVATE
        ${ImGUI_DIR}/
        ${ImGUI_DIR}/implot/
        ${ImGUI_DIR}/imgui-knobs/

)

target_link_libraries(MDM PRIVATE implot imknobs imgui_backend_opengl3 OpenGL::GL glfw imgui_backend_glfw ${CMAKE_DL_LIBS})


target_sources(
        MDM
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src/main.cpp
        ${ImGUI_DIR}/imgui.cpp
        ${ImGUI_DIR}/imgui_demo.cpp
        ${ImGUI_DIR}/imgui_draw.cpp
        ${ImGUI_DIR}/imgui_tables.cpp
        ${ImGUI_DIR}/imgui_widgets.cpp
)

