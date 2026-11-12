#include "midi.h"
#include <random>

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

    std::vector<uint32_t> l = {0, 100, 600, 800, 5000};
    std::random_device rd;
    std::mt19937 gen(rd());
    for(uint32_t k=0;k<100;k++)
    {
        std::shuffle(l.begin(), l.end(), gen);    

        for(uint32_t i=0;i<l.size();i++)
        {
//            printf("- Inserting at %i\n", l[i]);
            pTrack->insertEventAtTick(l[i], eventBytes);
//            pTrack->print();
        }

        for(uint32_t i=0;i<l.size();i++)
        {
//            printf("- Deleting at %i\n", l[i]);
            pTrack->deleteEventAtTick(l[i], eventBytes);
//            pTrack->print();
        }
    }
    printf("--\n");
    pTrack->print();

    pTrack->seek(0, NULL);
    printf("%li\n", pTrack->m_pTrack - pTrack->m_pTrackIni);

    pTrack->seek(200, NULL);
    printf("%li\n", pTrack->m_pTrack - pTrack->m_pTrackIni);


}