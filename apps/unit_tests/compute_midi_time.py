import os
from pathlib import Path
import mido

def get_midi_durations(folder_path):
    # Convert to Path object
    folder = Path(folder_path)
    
    if not folder.exists() or not folder.is_dir():
        print(f"Error: '{folder_path}' is not a valid directory.")
        return

    print(f"Scanning '{folder}' for MIDI files...\n")
    print(f"{'Filename':<40} | {'Duration (seconds)':<18}")
    print("-" * 62)

    # Search for standard MIDI extensions (.mid, .midi)
    midi_files = list(folder.glob("*.mid")) + list(folder.glob("*.midi"))

    if not midi_files:
        print("No MIDI files found in this directory.")
        return

    for file_path in sorted(midi_files):
        try:
            # Load the MIDI file
            midi = mido.MidiFile(file_path)
            duration_seconds = midi.length
            
            # Print filename and its size/duration in seconds
            print(f"{file_path.name:<40} | {duration_seconds:<18.2f}")
            
        except Exception as e:
            print(f"{file_path.name:<40} | Error reading file: {e}")

if __name__ == "__main__":
    # Replace with the path to your folder containing MIDI files
    target_folder = "./midis/" 
    get_midi_durations(target_folder)