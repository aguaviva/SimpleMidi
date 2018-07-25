# SimpleMidi
A one file simple midi player:

- Supports Windows and Linux
- Plays audio asynchronously
- Features a square wave synthesizer

## How to compile it and run it
Create a *build* directory, there run *cmake ..*, then on:
- Windows: open and run the solution, the audio will play right away
- Linux: run make, copy the *SimleMidi* into its parent dir so it can find the mid file, then you can run it.

## Known issues
It doesn't play every single midi file.

## Future work
Use fixed point so it runs on an arduino/ESP8266.
