#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "instrument.h"
#include "misc.h"
#include "midi.h"

#define MIN(a,b) ((a<b)?a:b)

///////////////////////////////////////////////////////////////
// MidiTrack
///////////////////////////////////////////////////////////////

MidiTrack::MidiTrack(uint8_t *ini, uint8_t*fin, MidiState *pMidiState) : MidiStream(ini, fin)
{
    m_pMidiState = pMidiState;
    Reset();
}

void MidiTrack::Reset() 
{ 
    MidiStream::Reset(); 

    m_nextTime = GetVar();
    m_channel = -1;
    m_lastType = 0;
    m_TrackName = "none";
    m_InstrumentName = "none";
    m_status = 0;
    m_elapsed_microseconds = 0;
}

uint32_t MidiTrack::play(uint32_t midi_ticks)
{
    // too early for the next event, bail out
    if (midi_ticks < m_nextTime)
        return m_nextTime;

    // are we done with the stream?
    if (done() == true)
        return NO_EVENTS;

    //process all events for this instant and compute m_nextTime
    for (;;)
    {
        if ((*m_pTrack & 0x80) > 0)
        {
            m_status = GetByte();
        }

        //LOG("%s:  0x%02x ", m_TrackName, type);
        if (m_status >= 0xf0)
        {
            if (m_status == 0xff)
            {
                unsigned char metaType = GetByte();
                unsigned char length = GetByte();

                //LOG("    meta: 0x%02x", metaType);
                //LOG("    length: 0x%02x", length);

                if (metaType >= 0x01 && metaType <= 0x05)
                {
                    const char* ChunkType[] = { "text event", "copyright", "track name", "instrument", "lyrics" };

                    if (metaType == 3)
                    {
                        m_TrackName = (char*)m_pTrack;
                    }
                    else if (metaType == 4)
                    {
                        m_InstrumentName = (char*)m_pTrack;
                    }

                    LOG("    %s: ", ChunkType[metaType - 1]);
                    for (int i = 0; i < length; i++)
                    {
                        char c = GetByte();
                        LOG("%c", c);
                    }
                    LOG("\n");
                }
                else if (metaType == 0x09)
                {
                    LOG("Device_name: ");

                    for (int i = 0; i < length; i++)
                    {
                        char c = GetByte();
                        LOG("%c", c);
                    }
                    LOG("\n");
                }
                else if (metaType == 0x20)
                {
                    m_channel = (m_channel & 0xFF00) | GetByte();
                    LOG("m_channel %i", m_channel);
                }
                else if (metaType == 0x21)
                {
                    m_channel = (m_channel & 0xFF) | GetByte() << 8;
                }
                else if (metaType == 0x51)
                {
                    uint32_t microseconds_per_quarter = GetLength(3);
                    uint32_t beatsPerSecond = 1000000 / microseconds_per_quarter;
                    //LOG("    tempo: %i bpm/QNPM", beatsPerSecond * 60);
                    if (m_pMidiState)
                    {
                        m_pMidiState->m_microSecondsPerMidiTick = (microseconds_per_quarter / m_pMidiState->m_ticksPerQuarterNote);
                    }
                }
                else if (metaType == 0x54)
                {
                    unsigned char hr = GetByte();
                    unsigned char mn = GetByte();
                    unsigned char se = GetByte();
                    unsigned char fr = GetByte();
                    unsigned char ff = GetByte();
                    LOG("SMPTE Offset %i:%i:%i  %i,%i\n", hr, mn, se, fr, ff);
                }
                else if (metaType == 0x58)
                {
                    m_timesignatureNum = GetByte();
                    m_timesignatureDen = 1 << GetByte();
                    unsigned char cc = GetByte();  // midi clocks in a metronome click
                    unsigned char bb = GetByte();  // number of 32nd notes per beat. This byte is usually 8 as there is usually one quarter note per beat and one quarter note contains eight 32nd notes.

                    if (m_pMidiState)
                    {
                        m_pMidiState->m_midi_ticks_per_metronome_tick = cc;
                    }

                    LOG("time sig: %i/%i MIDI clocks per quarter-dotted %i\n", m_timesignatureNum, m_timesignatureDen, cc);
                }
                else if (metaType == 0x59)
                {
                    unsigned char sf = GetByte();
                    unsigned char mi = GetByte();

                    //LOG("    sf: %i mi: %i", sf, mi);
                }
                else if (metaType == 0x2F)
                {
                    //unsigned char nn = GetByte();
                    LOG("************ THE END ************\n");
                }
                else if (metaType == 0x7f)
                {
                    LOG(" sequencer specific");
                    for (int i = 0; i < length; i++)
                    {
                        char c = GetByte();
                        LOG("%02x ", c);
                    }
                    LOG("\n");
                }
                else
                {
                    LOG(" ch %i   unknown metaType: 0x%02x, length %i\n", m_channel, metaType, length);
                    Skip(length);
                }
            }
            else if (m_status == 0xf0)
            {
                LOG("System exclusive: ");
                for (;;)
                {
                    unsigned char c = GetByte();
                    if (c == 0xf7)
                        break;
                    LOG("%02x ", c);
                }
                LOG("\n");
            }
            else if (m_status == 0xf7)
            {
                unsigned char length = GetByte();
                Skip(length);
            }
            else if (m_status == 0xfd)
            {
                GetByte();
                GetByte();
            }
            else
            {

            }
        }
        else
        {
            unsigned char subType = m_status >> 4;
            unsigned char channel = m_status & 0xf;
            if (subType >= 0x8 && subType <= 0xB)
            {
                unsigned char key = GetByte();
                unsigned char speed = GetByte();

                uint8_t data[3] = { m_status , key, speed };
                if (pPiano!=NULL) 
                    pPiano->send_midi(data, 3);
            }
            else if (subType == 0xC) // set program
            {
                unsigned char program = GetByte();

                uint8_t data[2] = { m_status , program };
                //LOG(" %s: program %i: %i\n", m_TrackName, channel, program);

                if (pPiano!=NULL) 
                    pPiano->send_midi(data, 2);
            }
            else if (subType == 0xD) // channel pressure
            {
                unsigned char cc = GetByte();

                uint8_t data[2] = { m_status , cc };
                if (pPiano!=NULL) 
                    pPiano->send_midi(data, 2);
            }
            else if (subType == 0xE)
            {
                unsigned char cc = GetByte();
                unsigned char dd = GetByte();

                uint8_t data[3] = { m_status , cc, dd };
                if (pPiano!=NULL) 
                    pPiano->send_midi(data, 3);
            }
            else
            {
                LOG("    unknown type: 0x%02x", m_status);
                assert(0);
            }
        }

        if (done())
        {
            m_nextTime = NO_EVENTS;
            break;
        }

        uint32_t deltaMidiTicks = GetVar();
        if (deltaMidiTicks > 0)
        {
            m_nextTime += deltaMidiTicks;
            break;
        }
    }
    
    return m_nextTime;
}


///////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////
MidiSong::MidiSong()
{
    m_tracks = 0;
    Reset();
}

uint32_t MidiSong::sequencer_step(uint32_t time)
{
    uint32_t nextEventTime = NO_EVENTS;
    for (uint32_t t = 0; t < m_tracks; t++)
    {
        uint32_t next = m_pTrack[t]->play(time);
        nextEventTime = MIN(next, nextEventTime);
    }
    return nextEventTime;
}

void MidiSong::render_tracks(uint32_t n_samples, float *pOut)
{
    for (uint32_t t = 0; t < m_tracks; t++)
    {
        m_pTrack[t]->pPiano->render_samples(n_samples, pOut);
    }
}

size_t MidiSong::step()
{
    uint32_t nextEventTime = sequencer_step(m_midi_state.m_time);
    if (nextEventTime == NO_EVENTS)
        return nextEventTime;

    uint32_t interval = nextEventTime - m_midi_state.m_time;
    m_midi_state.m_time = nextEventTime;

    //LOG("%6i - %4i [%5i]\n", m_midi_state.m_time, interval, m_midi_state.m_microSecondsPerMidiTick);

    uint64_t microseconds_to_render = ((uint64_t)interval * m_midi_state.m_microSecondsPerMidiTick);

    m_elapsed_microseconds += microseconds_to_render;

    return microseconds_to_render;
}

size_t MidiSong::RenderMidi(const uint32_t sampleRate, size_t size, float *pOut)
{
    size_t size_org = size;

    while (size>0)
    {
        if (m_event_samples_to_render == 0)
        {
            uint64_t microseconds_to_render = step();
            if (microseconds_to_render == NO_EVENTS)
                break;

            m_event_samples_to_render = ( (uint64_t)sampleRate * microseconds_to_render)/ 1000000;
        }

        {
            uint32_t n_samples = MIN(size, m_event_samples_to_render);
            
            if (pOut!=NULL)
            {
                render_tracks(n_samples, pOut);            
                pOut += n_samples * 2;
            }

            size -= n_samples;
            m_event_samples_to_render -= n_samples;
        }
    }

    return size_org - size;
}

uint32_t MidiSong::get_elapsed_milliseconds()
{
    return m_elapsed_microseconds/1000.0f;
}

bool MidiSong::LoadMidi(uint8_t *midi_buffer, size_t midi_buffer_size)
{
    MidiStream ch(midi_buffer, midi_buffer + midi_buffer_size);

    if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'h' && ch.GetByte() == 'd')
    {
        ch.GetLength(4);

        uint32_t format = ch.GetLength(2);
        uint32_t tracks = ch.GetLength(2);
        m_midi_state.m_ticksPerQuarterNote = ch.GetLength(2);

        uint32_t tempo = ((60 * 1000000) / 110);

        m_midi_state.m_microSecondsPerMidiTick = tempo / m_midi_state.m_ticksPerQuarterNote;

        LOG("format %i, tracks %i, ticksPerQuarterNote %i\n", format, tracks, m_midi_state.m_ticksPerQuarterNote);
    }

    m_midi_state.m_time = 0;

    m_tracks = 0;
    while (ch.done() == false)
    {
        if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'r' && ch.GetByte() == 'k')
        {
            int length = ch.GetLength(4);
            m_pTrack[m_tracks] = new MidiTrack(ch.m_pTrack, ch.m_pTrack + length, &m_midi_state);
            m_pTrack[m_tracks]->play(0);
            ch.Skip(length);
            m_tracks++;
        }
    }

    //measure midi length
    while(step() != NO_EVENTS) {}
    float total_midi_time = get_elapsed_milliseconds() / 1000.0f;
    printf("midi length: %f s\n", total_midi_time);
    Reset();

    return true;
}

void MidiSong::Reset() 
{
    m_event_samples_to_render = 0;
    m_elapsed_microseconds = 0;
    m_midi_state.m_time = 0;

    for (int i=0;i<m_tracks;i++)
        m_pTrack[i]->Reset();
}
