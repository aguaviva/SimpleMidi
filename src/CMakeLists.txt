# Create a library target (STATIC or SHARED)
add_library(midilib 
    midi.cpp
    misc.cpp
)

# Tell CMake where other files can find this library's public headers
target_include_directories(midilib PUBLIC .)