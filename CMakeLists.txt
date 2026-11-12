cmake_minimum_required(VERSION 3.15)
project(MyMidiProject CXX)

# Set global C++ standard (e.g., C++20 as used in your project)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add your shared library subdirectory
add_subdirectory(src)

# Add your applications subdirectory
add_subdirectory(apps)



