import mido
from mido import Message, MetaMessage, MidiFile, MidiTrack

def test1():
    # Create a multi-track MIDI file (Format 1)
    mid = MidiFile(type=1)

    # Track 1: Setup & Note Testing (Polyphony + Sustain Pedal + Volume)
    track1 = MidiTrack()
    mid.tracks.append(track1)

    track1.append(Message('control_change', channel=0, control=7, value=100, time=0))   # Volume
    track1.append(Message('control_change', channel=0, control=64, value=127, time=0))  # Sustain ON
    track1.append(Message('note_on', channel=0, note=60, velocity=64, time=0))        # C4
    track1.append(Message('note_on', channel=0, note=64, velocity=80, time=100))      # E4 (Chord)
    track1.append(Message('note_on', channel=0, note=67, velocity=90, time=100))      # G4 (Chord)
    track1.append(Message('note_off', channel=0, note=60, velocity=0, time=480))      # Release C4 (held by sustain)
    track1.append(Message('note_off', channel=0, note=64, velocity=0, time=100))      # Release E4
    track1.append(Message('note_off', channel=0, note=67, velocity=0, time=100))      # Release G4
    track1.append(Message('control_change', channel=0, control=64, value=0, time=10)) # Sustain OFF (triggers actual voice off)

    # Track 2: Dedicated Pitch Bend / Glissando Sweep Test (Channel 0)
    """
    track2 = MidiTrack()
    mid.tracks.append(track2)

    track2.append(Message('note_on', channel=0, note=60, velocity=100, time=0)) # Hold C4

    # Sweep pitch bend smoothly from center (8192) up to max (+2 semitones)
    for bend_val in range(0, 8192, 512):
        # mido pitchwheel takes values from -8192 to 8191
        track2.append(Message('pitchwheel', channel=0, pitch=bend_val, time=30))

    track2.append(Message('note_off', channel=0, note=60, velocity=0, time=480))
    """

    track3 = MidiTrack()
    mid.tracks.append(track3)
    track3.append(Message('note_on', channel=0, note=60, velocity=64, time=1000))        # C4
    track3.append(Message('note_off', channel=0, note=60, velocity=64, time=100))        # C4

    track3.append(Message('note_on', channel=0, note=64, velocity=64, time=1200))        # C4
    track3.append(Message('note_off', channel=0, note=64, velocity=64, time=100))        # C4

    # Save the test file
    mid.save('all_features_test.mid')
    print("Generated 'all_features_test.mid' successfully!")


def test2():
    # Create a single-track MIDI file (Format 0)
    mid = MidiFile(type=0)

    # Track 1: Setup & Note Testing (Polyphony + Sustain Pedal + Volume)
    track1 = MidiTrack()
    mid.tracks.append(track1)

    tempo_value = mido.bpm2tempo(6)
    track1.append(MetaMessage('set_tempo', tempo=tempo_value))

    track1.append(Message('control_change', channel=0, control=7, value=100, time=0))   # Volume
    track1.append(Message('control_change', channel=0, control=64, value=127, time=0))  # Sustain ON
    track1.append(Message('note_on', channel=0, note=60, velocity=64, time=0))        # C4
    track1.append(Message('note_on', channel=0, note=64, velocity=80, time=100))      # E4 (Chord)
    track1.append(Message('note_on', channel=0, note=67, velocity=90, time=100))      # G4 (Chord)
    track1.append(Message('note_off', channel=0, note=60, velocity=0, time=480))      # Release C4 (held by sustain)
    track1.append(Message('note_off', channel=0, note=64, velocity=0, time=100))      # Release E4
    track1.append(Message('note_off', channel=0, note=67, velocity=0, time=100))      # Release G4
    track1.append(Message('control_change', channel=0, control=64, value=0, time=10)) # Sustain OFF (triggers actual voice off)

    # Save the test file
    mid.save('test_2.mid')
    print("Generated 'test_2.mid' successfully!")

def test3():
    # Create a single-track MIDI file (Format 0)
    mid = MidiFile(type=0)

    # Track 1: Setup & Note Testing (Polyphony + Sustain Pedal + Volume)
    track1 = MidiTrack()
    mid.tracks.append(track1)

    tempo_value = mido.bpm2tempo(6)
    track1.append(MetaMessage('set_tempo', tempo=tempo_value))

    track1.append(Message('control_change', channel=0, control=7, value=100, time=0))   # Volume
    track1.append(Message('control_change', channel=0, control=64, value=127, time=0))  # Sustain ON
    track1.append(Message('note_on', channel=0, note=60, velocity=64, time=500))        # C4
    track1.append(Message('note_off', channel=0, note=60, velocity=0, time=5000))      # Release C4 (held by sustain)
    track1.append(Message('control_change', channel=0, control=64, value=0, time=500)) # Sustain OFF (triggers actual voice off)

    # Save the test file
    mid.save('test_3.mid')
    print("Generated 'test_3.mid' successfully!")

def test4():
    # Create a single-track MIDI file (Format 0)
    mid = MidiFile(type=0)
    
    track2 = MidiTrack()
    mid.tracks.append(track2)

    track2.append(Message('note_on', channel=0, note=60, velocity=100, time=0)) # Hold C4

    # Sweep pitch bend smoothly from center (8192) up to max (+2 semitones)
    for bend_val in range(0, 8192, 512):
        # mido pitchwheel takes values from -8192 to 8191
        track2.append(Message('pitchwheel', channel=0, pitch=bend_val, time=30))

    track2.append(Message('note_off', channel=0, note=60, velocity=0, time=480))

    # Save the test file
    mid.save('4_pitchwheel.mid')
    print("Generated '4_pitchwheel' successfully!")



test1()
test2()
test3()