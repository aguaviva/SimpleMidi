# SimpleMidi
A one file simple midi player:

- Supports Windows and Linux
- Plays audio asynchronously
- Features a square wave synthesizer

## Getting started
First create the slolution/makefile:
```
$ git clone --recursive https://github.com/aguaviva/SimpleMidi.git
$ cd SimpleMidi
$ mkdir build
$ cd build
$ cmake ..
$ make
```

then build it and run it:
- Windows: open and run the solution, the audio will play right away
- Linux: run make, copy the *SimleMidi* into its parent dir so it can find the mid file, then you can run it.

## Known issues
Due to its simple nature it just won't play every single midi file. 

## Future work
Use fixed point so it runs on an arduino/ESP8266.

