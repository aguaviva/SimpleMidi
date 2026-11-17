#include "midi.h"
#include "misc.h"
#include <random>

class Midi
{
    public:
    size_t midi_size;
    uint8_t* pMidi_buffer;
    MidiSong midi;    
    bool load(const char *filename)
    {
        load_file(filename, &pMidi_buffer, &midi_size);
        midi.LoadMidi(pMidi_buffer, midi_size, true);
    }
    ~Midi()
    {
        free(pMidi_buffer);
    }
};

void test_insertion()
{
    Midi m;
    m.load("test_2.mid");
    MidiTrackEditable *pTrack = m.midi.GetEditableTrack(0);
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

bool test_times()
{
    std::vector<std::string> files;
    get_midi_list("../../midis", &files);
    for(int i=0;i<files.size();i++)
    {
        printf("%30s  ",files[i].c_str());
        {
            Midi m;
            m.load(("../../midis/" + files[i]).c_str());
            printf(" %3.2f\n", m.midi.get_total_ms()/1e6f);    
        }
        
    }
}

int main(int argc, char **argv)
{
    test_insertion();
    test_times();
}