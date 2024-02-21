cmake_minimum_required(VERSION 3.1...3.28)
project( DX7 VERSION 1.0 LANGUAGES CXX)

set(msfa ${CMAKE_CURRENT_SOURCE_DIR}/music-synthesizer-for-android/app/src/main/jni/)

if (IS_DIRECTORY ${msfa})

message(STATUS "MSFA found!")

set( source_files
  aligned_buf.h
  controllers.h
  dx7note.cc
  dx7note.h
  env.cc
  env.h
  exp2.cc
  exp2.h
  fir.cc
  fir.h
  fm_core.cc
  fm_core.h
  fm_op_kernel.cc
  fm_op_kernel.h
  freqlut.cc
  freqlut.h
  lfo.cc
  lfo.h
  log2.cc
  log2.h
  module.h
  patch.cc
  patch.h
  pitchenv.cc
  pitchenv.h
  resofilter.cc
  resofilter.h
  ringbuffer.cc
  ringbuffer.h
  sawtooth.cc
  sawtooth.h
  sin.cc
  sin.h
  synth.h
  synth_unit.cc
  synth_unit.h
)

# add path to files
set(source_files_with_path "")
foreach(item ${source_files})
  list(APPEND source_files_with_path ${msfa}/${item})
endforeach()

include_directories(${msfa})
add_library(DX7 dx7.cpp ${source_files_with_path})

else()

message(STATUS "MSFA not found")

endif()

