# --- The Main Application (includes audio.cpp) ---
add_executable(unit_tests
    unit_tests.cpp
)

target_link_libraries(unit_tests PRIVATE midilib)