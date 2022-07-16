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

///////////////////////////////////////////////////////////////
// MidiTrack
///////////////////////////////////////////////////////////////

class MidiTrack : public MidiStream
{
    uint64_t m_nextTime;
    unsigned char m_lastType;
    Midi *m_pMidi;
public:
    uint32_t m_channel;
    const char* m_TrackName;
    const char* m_InstrumentName;
    uint8_t m_status;
    uint32_t m_timesignatureNum = 4;
    uint32_t m_timesignatureDen = 4;
    Instrument *pPiano = NULL;

    MidiTrack(uint8_t *ini, uint8_t*fin, Midi *pMidi = NULL);
    uint32_t play(uint32_t midi_ticks);
};

///////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////

class Midi
{
    uint32_t m_time;

    MidiTrack *m_pTrack[50];
    uint32_t m_tracks;
    uint32_t m_nextEventTime;

    uint32_t m_samples_to_render;
public:
    uint32_t m_ticksPerQuarterNote;  //or ticks per beat
    float m_quarterNote = 24;
    uint32_t m_microSecondsPerMidiTick;

    Midi();
    uint32_t GetTrackCount() { return m_tracks; }
    MidiTrack *GetTrack(uint32_t i) { return m_pTrack[i]; }

    size_t RenderMidi(const uint32_t sampleRate, const uint32_t channels, size_t size, float *pOut);
    bool LoadMidi(uint8_t *midi_buffer, size_t midi_buffer_size);
};
