cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_TOOLCHAIN_FILE "/home/raul/repos/vcpkg/scripts/buildsystems/vcpkg.cmake")
message(STATUS "vcpkg: ${CMAKE_TOOLCHAIN_FILE}")

project(SimpleMidi VERSION 0.1.0)

find_package(imgui CONFIG REQUIRED)

find_package(glfw3 CONFIG REQUIRED)

include(CTest)
enable_testing()

add_executable(SimpleMidi SimpleMidi.cpp inst-piano.cpp audio.cpp misc.cpp midi.cpp)
target_link_libraries(SimpleMidi PRIVATE OpenGL glfw imgui::imgui)

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

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack) 
