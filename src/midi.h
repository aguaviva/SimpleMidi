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

    uint32_t GetVarPeek()
    {
        uint32_t out = 0;

        for (uint32_t i=0;;i++)
        {
            out <<= 7;
            out += m_pTrack[i] & 0x7f;

            if ((m_pTrack[i] & 0x80) == 0x00)
                break;
        }

        return out;
    }

    uint32_t GetVarSizePeek()
    {
        uint32_t i=0;
        for (;;) 
        {
            if ((m_pTrack[i] & 0x80) == 0x00)
                return i+1;           
            i++;
        }
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

struct TempoChange {
    uint32_t absolute_tick; // When the tempo change happens
    uint32_t usPerQuarter;  // Microseconds per quarter note (from FF 51 03)
    uint32_t absolute_microseconds;
};

struct MidiState
{
    uint32_t m_time = 0;
    uint32_t m_ticksPerQuarterNote;  //or ticks per beat
    uint32_t m_midi_ticks_per_metronome_tick = 24;
    uint32_t m_microSecondsPerMidiTick;
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
protected:
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
    virtual ~MidiTrack() = default;
    void Reset();
    ticks_t get_tempo_changes(std::vector<TempoChange> &tempo_changes);
    uint32_t process_next_event(MidiSendFunc pFunc);
    ticks_t play(ticks_t midi_ticks, MidiSendFunc pFunc);
    ticks_t seek(ticks_t midi_ticks, MidiSendFunc pFunc);
    uint32_t get_elapsed_milliseconds() { return m_elapsed_microseconds/1000; };
    void print();    
};

class MidiTrackEditable : public MidiTrack
{
    std::vector<uint8_t> m_trackData;
    void GetTimes(ticks_t ticks, ticks_t *pPrev_time, ticks_t *pNext_time);

public:
    MidiTrackEditable(uint8_t *ini, uint8_t *fin, MidiState *pMidiState = NULL);
    // Inserts a note at an absolute track tick; changes this track's in-memory MIDI data.
    bool insertEventAtTick(uint32_t tick, const std::vector<uint8_t>& eventBytes);
    bool deleteEventAtTick(uint32_t tick, const std::vector<uint8_t>& eventBytes);
    bool insertNote(uint32_t tick, uint8_t key, uint8_t velocity, uint32_t durationTicks, uint8_t channel = 0);
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
    uint32_t m_tempo_index;
    std::vector<TempoChange> m_tempo_changes;

public:
    MidiState m_midi_state;

    MidiSong();
    uint32_t GetTrackCount() { return m_tracks; }
    MidiTrack *GetTrack(uint32_t i) { return m_pTrack[i]; }
    MidiTrackEditable *GetEditableTrack(uint32_t i) { return dynamic_cast<MidiTrackEditable *>(m_pTrack[i]); }
    uint32_t get_elapsed_microseconds();
    uint32_t get_microseconds_per_quarter_note();
    size_t step(MidiSendFunc *pFunc);
    uint32_t seek(uint64_t microseconds, MidiSendFunc *pFunc=NULL);
    bool LoadMidi(uint8_t *midi_buffer, size_t midi_buffer_size, bool editable = false);
    void Reset();
    void compute_tempo_changes();
    const std::vector<TempoChange> &getTempoChanges() { return m_tempo_changes; };
    ticks_t microseconds_to_ticks(uint32_t microseconds);
    uint32_t ticks_to_microseconds(ticks_t ticks);
    uint64_t get_total_ms();
};
