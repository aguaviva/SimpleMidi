cmake_minimum_required(VERSION 3.20)
project(todo-app CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLFW — disable docs/tests/examples, we only want the library
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# ImGui — compiled as a static library
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)


# the app
add_executable(SimpleMidi SimpleMidi.cpp inst-piano.cpp audio.cpp misc.cpp midi.cpp)
target_link_libraries(SimpleMidi PRIVATE imgui)

if(APPLE)
    target_link_libraries(SimpleMidi PRIVATE "-framework OpenGL")
elseif(WIN32)
    target_link_libraries(SimpleMidi PRIVATE opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(SimpleMidi PRIVATE OpenGL::GL)
endif()


if (${UNIX})
    # *nix-specific includes or actions
    message("Linux stuff")
    find_package(ALSA REQUIRED)                                    
    if (ALSA_FOUND)                                                
        include_directories(${ALSA_INCLUDE_DIRS})                  
        target_link_libraries (SimpleMidi PRIVATE  ${ALSA_LIBRARY}  )      
    endif(ALSA_FOUND)  

elseif (${WIN32})
  # Windows-specific includes or actions
  message("Windows stuff")
elseif (${APPLE})
  # ...
endif (${UNIX})

