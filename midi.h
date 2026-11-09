#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"

#define MIN(a,b) ((a<b)?a:b)
#define MAX(a,b) ((a>b)?a:b)

#define NO_EVENTS 0xffffffff

typedef uint32_t ticks_t;

///////////////////////////////////////////////////////////////
// MidiStream 
///////////////////////////////////////////////////////////////
class MidiStream
{
public:
    uint8_t *m_pTrack = NULL;
    uint8_t *m_pTrackIni = NULL;
    uint8_t *m_pTrackFin = NULL;

    MidiStream(MidiStream *pM):MidiStream(pM->m_pTrackIni, pM->m_pTrackFin)
    {
    
    }

    MidiStream(uint8_t *ini, uint8_t *fin)
    {
        m_pTrack = ini;
        m_pTrackIni = ini;
        m_pTrackFin = fin;
    }

    void Reset() { m_pTrack = m_pTrackIni; }

    uint32_t GetVar()
    {
        uint32_t out = 0;

        for (;;)
        {
            out <<= 7;
            out += *m_pTrack & 0x7f;

            if ((*m_pTrack & 0x80) == 0x00)
                break;

            m_pTrack++;
        }

        m_pTrack++;

        return out;
    }

    uint8_t PeekByte()
    {
        return *m_pTrack;
    }

    uint8_t  GetByte()
    {
        return *m_pTrack++;
    }

    void Skip(int count)
    {
        m_pTrack += count;
    }

    uint32_t GetLength(int bytes)
    {
        uint32_t val = 0;
        for (int i = 0; i < bytes; i++)
            val = (val << 8) + GetByte();

        return val;
    }

    bool done()
    {
        return m_pTrack >= m_pTrackFin;
    }
};

class MidiSong;

struct TempoChange
{
    uint64_t absolute_tick;
    double mpqn;
    uint64_t cumulative_microseconds;    
};

struct MidiState
{
    uint32_t m_time = 0;
    uint32_t m_ticksPerQuarterNote;  //or ticks per beat
    uint32_t m_midi_ticks_per_metronome_tick = 24;
    uint32_t m_microSecondsPerMidiTick;
    std::vector<TempoChange> m_changes;
};

///////////////////////////////////////////////////////////////
// MidiTrack
///////////////////////////////////////////////////////////////

#include <functional>
#include <vector>

// Change from raw function pointer to std::function
using MidiSendFunc = std::function<void(uint8_t*, size_t)>;

class MidiTrack : public MidiStream
{
    ticks_t m_nextTime = 0;
    unsigned char m_lastType = 0;
    uint64_t m_elapsed_microseconds = 0;
public:
    MidiState *m_pMidiState;
    uint32_t m_channel;
    const char* m_TrackName;
    const char* m_InstrumentName;
    uint8_t m_status;
    uint32_t m_timesignatureNum = 4;
    uint32_t m_timesignatureDen = 4;

    MidiTrack(uint8_t *ini, uint8_t*fin, MidiState *pMidiState = NULL);
    void Reset();
    uint32_t process_next_event(MidiSendFunc pFunc);
    ticks_t play(ticks_t midi_ticks, MidiSendFunc pFunc);
    ticks_t seek(ticks_t midi_ticks, MidiSendFunc pFunc);
    uint32_t get_elapsed_milliseconds() { return m_elapsed_microseconds/1000; };
};

///////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////


class MidiSong
{
    MidiTrack *m_pTrack[50];
    uint32_t m_tracks = 0;
    uint32_t m_output_channel_count;
    uint32_t m_elapsed_microseconds;
    uint32_t m_event_samples_to_render;
public:
    MidiState m_midi_state;

    MidiSong();
    uint32_t GetTrackCount() { return m_tracks; }
    MidiTrack *GetTrack(uint32_t i) { return m_pTrack[i]; }
    uint32_t get_elapsed_microseconds();
    uint32_t get_microseconds_per_quarter_note();
    size_t step(MidiSendFunc *pFunc);
    uint32_t seek(uint64_t microseconds, MidiSendFunc *pFunc=NULL);
    bool LoadMidi(uint8_t *midi_buffer, size_t midi_buffer_size);
    void Reset();
};
