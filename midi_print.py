import mido

# Load your MIDI file
mid = mido.MidiFile('all_features_test.mid')

# Print basic file info
print(f"MIDI Type: {mid.type}")
print(f"Ticks per beat (PPQ): {mid.ticks_per_beat}")
print(f"Number of tracks: {len(mid.tracks)}\n")

# Iterate through every track and print its messages
for i, track in enumerate(mid.tracks):
    print(f"--- Track {i}: {track.name} ---")
    for msg in track:
        print(msg)