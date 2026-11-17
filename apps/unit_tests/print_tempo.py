import mido

def print_midi_tempo_timeline(midi_path):
    try:
        mid = mido.MidiFile(midi_path)
    except Exception as e:
        print(f"Error loading MIDI file: {e}")
        return

    ticks_per_beat = mid.ticks_per_beat
    print(f"File: {midi_path}")
    print(f"Ticks per quarter note (TPQ): {ticks_per_beat}")
    print("-" * 75)
    print(f"{'Absolute Tick':<15} | {'Microseconds (μs)':<20} | {'Tempo / Event'}")
    print("-" * 75)

    current_tick = 0
    current_microseconds = 0.0
    current_mpqn = 500000  # Default MIDI tempo: 500,000 µs per quarter note (120 BPM)

    # Print the initial default tempo at tick 0
    default_bpm = mido.tempo2bpm(current_mpqn)
    print(f"{current_tick:<15} | {int(current_microseconds):<20} | {current_mpqn} µs ({default_bpm:.2f} BPM) [Start]")

    # Iterate through all messages across merged tracks in chronological order
    for msg in mido.merge_tracks(mid.tracks):
        delta_ticks = msg.time
        if delta_ticks > 0:
            # Advance microseconds based on current tempo and elapsed ticks
            current_microseconds += (delta_ticks * current_mpqn) / ticks_per_beat
            current_tick += delta_ticks

        # Check if the message is a tempo change event
        if msg.type == 'set_tempo':
            current_mpqn = msg.tempo
            bpm = mido.tempo2bpm(current_mpqn)
            print(f"{current_tick:<15} | {int(current_microseconds):<20} | {current_mpqn} µs ({bpm:.2f} BPM) [Tempo Change]")

    # Print the end of the song
    total_seconds = current_microseconds / 1_000_000
    print("-" * 75)
    print(f"{current_tick:<15} | {int(current_microseconds):<20} | End of Song (~{total_seconds:.2f} seconds)")

if __name__ == "__main__":
    # Replace with the path to your MIDI file
    midi_file_path = "same_notes_tempo_change.mid"
    print_midi_tempo_timeline(midi_file_path)