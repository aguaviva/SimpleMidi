cmake_minimum_required(VERSION 3.0.0)
project(SimpleMidi VERSION 0.1.0)

include(CTest)
enable_testing()

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

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
