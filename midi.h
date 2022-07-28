#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "instrument.h"
#include "misc.h"

#define MIN(a,b) ((a<b)?a:b)
#define MAX(a,b) ((a>b)?a:b)

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

class Midi;

struct MidiState
{
    uint32_t m_time = 0;
    uint32_t m_nextEventTime = 0;
    uint32_t m_ticksPerQuarterNote;  //or ticks per beat
    uint32_t m_midi_ticks_per_metronome_tick = 24;
    uint32_t m_microSecondsPerMidiTick;
};

///////////////////////////////////////////////////////////////
// MidiTrack
///////////////////////////////////////////////////////////////

class MidiTrack : public MidiStream
{
    uint64_t m_nextTime = 0;
    unsigned char m_lastType = 0;
public:
    MidiState *m_pMidiState;
    uint32_t m_channel;
    const char* m_TrackName;
    const char* m_InstrumentName;
    uint8_t m_status;
    uint32_t m_timesignatureNum = 4;
    uint32_t m_timesignatureDen = 4;
    Instrument *pPiano = NULL;

    MidiTrack(uint8_t *ini, uint8_t*fin, MidiState *pMidiState = NULL);
    void Reset();
    uint32_t play(uint32_t midi_ticks);
};

///////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////


class Midi
{
    MidiTrack *m_pTrack[50];
    uint32_t m_tracks = 0;
    uint32_t m_sample_rate;
    uint32_t m_output_channel_count;

    uint32_t m_samples_to_render;
public:
    MidiState m_midi_state;

    Midi();
    uint32_t GetTrackCount() { return m_tracks; }
    MidiTrack *GetTrack(uint32_t i) { return m_pTrack[i]; }
    float GetTime();
    size_t RenderMidi(const uint32_t sampleRate, const uint32_t channels, size_t size, float *pOut);
    bool LoadMidi(uint8_t *midi_buffer, size_t midi_buffer_size);
    void Reset();
};
