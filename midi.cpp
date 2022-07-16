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

MidiTrack::MidiTrack(uint8_t *ini, uint8_t*fin, Midi *pMidi) : MidiStream(ini, fin)
{
    m_pMidi = pMidi;
    m_nextTime = GetVar();
    m_channel = -1;
    m_lastType = 0;
    m_TrackName = "none";
    m_InstrumentName = "none";
}

uint32_t MidiTrack::play(uint32_t midi_ticks)
{
    // too early for the next event, bail out
    if (midi_ticks < m_nextTime)
        return m_nextTime;

    // are we done with the stream?
    if (done() == true)
        return 0xffffffff;

    //process all events for this instant and compute m_nextTime
    for (;;)
    {
        if ((*m_pTrack & 0x80) > 0)
        {
            m_status = GetByte();
        }

        //printf("%s:  0x%02x ", m_TrackName, type);
        if (m_status >= 0xf0)
        {
            if (m_status == 0xff)
            {
                unsigned char metaType = GetByte();
                unsigned char length = GetByte();

                //printf("    meta: 0x%02x", metaType);
                //printf("    length: 0x%02x", length);

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

                    printf("    %s: ", ChunkType[metaType - 1]);
                    for (int i = 0; i < length; i++)
                        printf("%c", GetByte());
                    printf("\n");
                }
                else if (metaType == 0x09)
                {
                    printf("Device_name: ");

                    for (int i = 0; i < length; i++)
                        printf("%c", GetByte());
                    printf("\n");
                }
                else if (metaType == 0x20)
                {
                    m_channel = (m_channel & 0xFF00) | GetByte();
                    printf("m_channel %i", m_channel);
                }
                else if (metaType == 0x21)
                {
                    m_channel = (m_channel & 0xFF) | GetByte() << 8;
                }
                else if (metaType == 0x51)
                {
                    uint32_t microseconds_per_quarter = GetLength(3);
                    uint32_t beatsPerSecond = 1000000 / microseconds_per_quarter;
                    //printf("    tempo: %i bpm/QNPM", beatsPerSecond * 60);

                    m_pMidi->m_microSecondsPerMidiTick = (microseconds_per_quarter / m_pMidi->m_ticksPerQuarterNote);
                }
                else if (metaType == 0x54)
                {
                    unsigned char hr = GetByte();
                    unsigned char mn = GetByte();
                    unsigned char se = GetByte();
                    unsigned char fr = GetByte();
                    unsigned char ff = GetByte();
                    printf("SMPTE Offset %i:%i:%i  %i,%i\n", hr, mn, se, fr, ff);
                }
                else if (metaType == 0x58)
                {
                    m_timesignatureNum = GetByte();
                    m_timesignatureDen = 1 << GetByte();
                    unsigned char cc = GetByte();  // midi clocks in a metronome click
                    unsigned char bb = GetByte();  // 

                    m_pMidi->m_quarterNote = 100000.0f / (float)cc;

                    //printf("    time sig: %i/%i MIDI clocks per quarter-dotted %i", m_timesignatureNum, m_timesignatureDen, cc);
                }
                else if (metaType == 0x59)
                {
                    unsigned char sf = GetByte();
                    unsigned char mi = GetByte();

                    //printf("    sf: %i mi: %i", sf, mi);
                }
                else if (metaType == 0x2F)
                {
                    //unsigned char nn = GetByte();
                    printf("************ THE END ************\n");
                }
                else if (metaType == 0x7f)
                {
                    printf(" sequencer specific");
                    for (int i = 0; i < length; i++)
                        printf("%02x ", GetByte());
                    printf("\n");
                }
                else
                {
                    printf(" ch %i   unknown metaType: 0x%02x, length %i\n", m_channel, metaType, length);
                    Skip(length);
                }
            }
            else if (m_status == 0xf0)
            {
                printf("System exclusive: ");
                for (;;)
                {
                    unsigned char c = GetByte();
                    if (c == 0xf7)
                        break;
                    printf("%02x ", c);
                }
                printf("\n");
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
            else if (subType == 0xC || subType == 0xD) // channel pressure
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
                printf("    unknown type: 0x%02x", m_status);
                assert(0);
            }
        }

        if (done())
            break;

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
Midi::Midi()
{
    m_time = 0;
    m_tracks = 0;
    m_nextEventTime = 0;
    m_samples_to_render = 0;
}

size_t Midi::RenderMidi(const uint32_t sampleRate, const uint32_t channels, size_t size, float *pOut)
{
    size_t size_org = size;

    while (size>0)
    {
        if (m_samples_to_render == 0)
        {
            m_time = m_nextEventTime;

            m_nextEventTime = 0xffffffff;
            for (uint32_t t = 0; t < m_tracks; t++)
            {
                uint32_t next = m_pTrack[t]->play(m_time);
                m_nextEventTime = MIN(next, m_nextEventTime);
            }
            if (m_nextEventTime == 0xffffffff)
                break;

            printf("%6i - %4i [%5i]\n", m_time, m_nextEventTime-m_time, m_microSecondsPerMidiTick);
            //pPiano->printKeyboard();

            uint64_t samples_per_midi_tick = ((uint64_t)sampleRate * m_microSecondsPerMidiTick) / 1000000;
            m_samples_to_render = (m_nextEventTime - m_time) * samples_per_midi_tick;
        }

        {
            int n_samples = MIN(size, m_samples_to_render);
            for (uint32_t t = 0; t < m_tracks; t++)
            {
                m_pTrack[t]->pPiano->render_samples((int)n_samples, pOut);
            }
            pOut += n_samples * 2;
            size -= n_samples;

            m_samples_to_render -= n_samples;
        }
    }

    return size_org - size;
}

bool Midi::LoadMidi(uint8_t *midi_buffer, size_t midi_buffer_size)
{
    MidiStream ch(midi_buffer, midi_buffer + midi_buffer_size);

    if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'h' && ch.GetByte() == 'd')
    {
        ch.GetLength(4);

        uint32_t format = ch.GetLength(2);
        uint32_t tracks = ch.GetLength(2);
        m_ticksPerQuarterNote = ch.GetLength(2);

        uint32_t tempo = ((60 * 1000000) / 110);

        m_microSecondsPerMidiTick = tempo / m_ticksPerQuarterNote;

        printf("format %i, tracks %i, ticksPerQuarterNote %i\n", format, tracks, m_ticksPerQuarterNote);
    }

    m_time = 0;

    m_tracks = 0;
    while (ch.done() == false)
    {
        if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'r' && ch.GetByte() == 'k')
        {
            int length = ch.GetLength(4);
            m_pTrack[m_tracks] = new MidiTrack(ch.m_pTrack, ch.m_pTrack + length, this);
            m_pTrack[m_tracks]->play(0);
            ch.Skip(length);
            m_tracks++;
        }
    }

    return true;
}
