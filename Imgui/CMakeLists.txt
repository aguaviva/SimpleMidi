cmake_minimum_required(VERSION 3.0.0)

add_library(Imgui imgui_impl_glfw.cpp imgui_impl_opengl3.cpp imgui.cpp imgui_tables.cpp imgui_widgets.cpp imgui_draw.cpp)

target_include_directories (Imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})