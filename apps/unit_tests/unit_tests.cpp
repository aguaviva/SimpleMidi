#include "midi.h"


int main(int argc, char **argv)
{
    size_t midi_size;
    uint8_t* pMidi_buffer;
    MidiSong midi;    
    load_file("test_2.mid", &pMidi_buffer, &midi_size);
    midi.LoadMidi(pMidi_buffer, midi_size, true);

    MidiTrackEditable *pTrack = midi.GetEditableTrack(0);
    pTrack->print();

    std::vector<uint8_t> eventBytes = {0x90, 0xBE, 0xEF};

    printf("---\n");
    pTrack->insertEventAtTick(0, eventBytes);
    pTrack->print();

    printf("---\n");
    pTrack->insertEventAtTick(100, eventBytes);
    pTrack->print();

    printf("---\n");
    pTrack->insertEventAtTick(600, eventBytes);
    pTrack->print();

    printf("---\n");
    pTrack->insertEventAtTick(5000, eventBytes);
    pTrack->print();


    pTrack->seek(0, NULL);
    printf("%i\n", pTrack->m_pTrack - pTrack->m_pTrackIni);

    pTrack->seek(200, NULL);
    printf("%i\n", pTrack->m_pTrack - pTrack->m_pTrackIni);


}