cmake_minimum_required(VERSION 3.20)
project(SimpleMidiProject CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SIMPLEMIDI_CLI_ONLY "Build SimpleMidi without its graphical interface" OFF)

if(NOT SIMPLEMIDI_CLI_ONLY)
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
endif()


# --- MIDI Core Library (inst-piano, misc, midi) ---
add_library(midi_core STATIC
    inst-piano.cpp
    misc.cpp
    midi.cpp
)

target_include_directories(midi_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Linux (ALSA) dependencies for MIDI handling
if(UNIX AND NOT APPLE)
    find_package(ALSA REQUIRED)
    if(ALSA_FOUND)
        target_include_directories(midi_core PRIVATE ${ALSA_INCLUDE_DIRS})
        target_link_libraries(midi_core PUBLIC ${ALSA_LIBRARY})
    endif()
endif()


# --- The Main Application (includes audio.cpp) ---
add_executable(SimpleMidi
    SimpleMidi.cpp
    audio.cpp
)

target_link_libraries(SimpleMidi PRIVATE midi_core)

if(SIMPLEMIDI_CLI_ONLY)
    target_compile_definitions(SimpleMidi PRIVATE SIMPLEMIDI_CLI_ONLY)
else()
    target_link_libraries(SimpleMidi PRIVATE imgui)
    if(APPLE)
        target_link_libraries(SimpleMidi PRIVATE "-framework OpenGL")
    elseif(WIN32)
        target_link_libraries(SimpleMidi PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(SimpleMidi PRIVATE OpenGL::GL)
    endif()
endif()