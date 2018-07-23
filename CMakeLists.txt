cmake_minimum_required(VERSION 2.8.9)
set(CMAKE_SUPPRESS_REGENERATION true)
project (SimpleMidi)
add_executable(SimpleMidi SimpleMidi.cpp)

if (${UNIX})
    # *nix-specific includes or actions
    message("Linux stuff")
    find_package(ALSA REQUIRED)                                    
    if (ALSA_FOUND)                                                
        include_directories(${ALSA_INCLUDE_DIRS})                  
        target_link_libraries (SimpleMidi ${ALSA_LIBRARY})      
    endif(ALSA_FOUND)  

elseif (${WIN32})
  # Windows-specific includes or actions
  message("Windows stuff")
elseif (${APPLE})
  # ...
endif (${UNIX})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SimpleMidi)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/)
set(EXECUTABLE_OUTPUT_PATH "bin")
