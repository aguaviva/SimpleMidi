// xmidi.cpp : Defines the entry point for the console application.
//
#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SYNTH_BASICSYNTH
//#define SYNTH_DX7
//#define SYNTH_MT32EMU

#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

#define MIN(a,b) ((a<b)?a:b)

bool load_file(const char* filename, uint8_t** pOut, size_t* pSize)
{
    FILE* f = fopen(filename, "rb");
    if (f == NULL)
    {
        printf("Can't open %s\n", filename);
        return false;
    }

    fseek(f, 0L, SEEK_END);
    *pSize = ftell(f);
    fseek(f, 0L, SEEK_SET);

    *pOut = (uint8_t*)malloc(*pSize);

    fread(*pOut, *pSize, 1, f);
    fclose(f);

    return true;
}

float NoteToFreq(int note)
{
    return (float)pow(2.0f, (note - 69.0f) / 12.0f) * 440.0f;
}

float NoteToFreqFrac(float note)
{
    return (float)pow(2.0f, (note - 69.0f) / 12.0f) * 440.0f;
}

void printNote(unsigned char note)
{
    const char *notes[] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };
    printf("%s%i", notes[note % 12], (note / 12)-1);
}

///////////////////////////////////////////////////////////////
// Simple square wave synthesizer
///////////////////////////////////////////////////////////////
class Instrument
{
public:
    virtual void send_midi(uint8_t* pdata, size_t size) = 0;
    virtual void render_samples(int n_samples, float* pOut) = 0;
    virtual void printKeyboard() = 0;
};

#ifdef SYNTH_BASICSYNTH
class Piano : public Instrument
{
    bool m_omni = false;
    uint32_t m_time;
    struct Channel
    {
        enum Envelope { OFF, ATTACK, DECAY, SUSTAIN, RELEASE};
        Envelope m_envelope = OFF;
        uint32_t m_time=0;
        uint32_t m_velocity = 0;
        uint32_t m_period = 0;
        uint8_t m_note = 0;
        uint32_t m_sample_rate = 0;

        // sqaure stuff
        uint32_t m_phase = 0;
        bool m_square = false;
        float playSquareWave(uint32_t time)
        {
            while (m_phase < time)
            {
                m_phase += m_period;
                m_square = !m_square;
            }
            return m_square ? 1.0f : -1.0f;
        }

        void press(uint32_t time, unsigned char note, unsigned char velocity)
        {
            if(velocity>0)
            {
                m_envelope=ATTACK;
                m_time = 0;
                m_note = note;
                m_velocity = velocity;
                m_phase = time;
                m_period = (uint32_t)(m_sample_rate / NoteToFreq(note))/2;
            }
            else
            {
                release(time, note, velocity);
            }
        }

        void release(uint32_t time, unsigned char note, unsigned char velocity)
        {
            switch(m_envelope)
            {
                case ATTACK:
                case DECAY:
                    m_envelope=RELEASE;
                    break;
                case SUSTAIN:
                    m_envelope=RELEASE;
                    break;
            }
            m_time = 0;
        }

        float render(uint32_t time)
        {
            if (m_note <=0)
                return 0;
            
            uint32_t attackDuration  = m_sample_rate * 0.01f;
            uint32_t decayDuration   = m_sample_rate * 0.1f;
            uint32_t sustainDuration = m_sample_rate * 0.5f;
            uint32_t releaseDuration = m_sample_rate * 0.2f;

            float vol = 0.0;

            switch(m_envelope)
            {
                case ATTACK:
                    if (m_time<=attackDuration)
                    {
                        vol = (float)m_time / (float)attackDuration;
                        break;
                    }
                    m_envelope = DECAY;
                    m_time = 0;

                case DECAY:
                    if (m_time <= decayDuration)
                    {
                        float t = (float)m_time / (float)decayDuration;
                        vol =  0.5f + 0.5f*(1-t);                    
                        break;
                    }
                    m_envelope = SUSTAIN;
                    m_time = 0;

                case SUSTAIN:
                    if (m_time <= sustainDuration)
                    {
                        vol = 0.5f;                    
                        break;
                    }
                    m_envelope = RELEASE;
                    m_time = 0;

                case RELEASE:
                    if (m_time <= releaseDuration)
                    {
                        float t = ((float)m_time / (float)releaseDuration);                        
                        vol = 0.5f*(1-t);
                        break;
                    }
                    m_envelope = OFF;
                    m_time = 0;

                case OFF:
                    m_note = 0;
                    return 0.0;
            }

            m_time++;
            return playSquareWave(time) * vol * (m_velocity/127.0f);
        }
    };

    Channel m_channel[20];
    uint32_t m_sample_rate;
    uint32_t m_noteDuration;

public:
    Piano(uint32_t sample_rate)
    {
        m_sample_rate = sample_rate;

        for (int i = 0; i < _countof(m_channel); i++)
        m_channel[i].m_sample_rate = sample_rate;

        m_time = 0;
    }

    int release(char channelId, unsigned char note, unsigned char velocity)
    {
        if (channelId == -1)
        {
            for (int i = 0; i < _countof(m_channel); i++)
            {
                if (m_channel[i].m_note == note)
                {
                    channelId = i;
                    break;
                }
            }
        }

        //assert(channelId >= 0);

        if (channelId >=0)
        {
            m_channel[channelId].release(m_time, note, velocity);
        }

        return channelId;
    }

    int push(char channelId, unsigned char note, unsigned char velocity)
    {        
        if (channelId == -1)
        {
            for (int i = 0; i < _countof(m_channel); i++)
            {
                if (m_channel[i].m_note == note)
                {
                    channelId = i;
                    break;
                }
            }
        }

        if (channelId == -1)
        {
            for (int i = 0; i < _countof(m_channel); i++)
            {
                if (m_channel[i].m_note == 0)
                {
                    channelId = i;
                    break;
                }
            }
        }
        
        assert(channelId >= 0);

        if (channelId >= 0)
        {
            m_channel[channelId].press(m_time, note, velocity);
        }

        return channelId;
    }

    float synthesize()
    {
        float out = 0;
        
        for (int i = 0; i < _countof(m_channel); i++) 
        {
            out += m_channel[i].render(m_time);
        }

        out /= _countof(m_channel);

        m_time ++;

        return out;
    }

    void send_midi(uint8_t* pdata, size_t size)
    {
        unsigned char subType = pdata[0] >> 4;
        unsigned char channel = pdata[0] & 0xf;        

        if (subType == 0x8 || subType == 0x9)
        {
            uint8_t key = pdata[1];
            uint8_t speed = pdata[2];

            //if (channel==9)
            //    return;
            //printf("%i %i, %i %i\n", subType, channel, key, speed);

            if (subType == 8)
            {
                release(m_omni ? channel : -1, key, speed);
            }
            else if (subType == 9)
            {
                push(m_omni ? channel : -1, key, speed);
            }
        }
        else if (subType == 0xB)
        {
            unsigned char cc = pdata[1];
            unsigned char nn = pdata[2];
            if (cc == 0x78)
            {
                m_omni = false;
            }
            else  if (cc == 0x7c)
            {
                m_omni = false;
            }
            else if (cc == 0x7d)
            {
                m_omni = true;
            }
        }
        else if (subType == 0xE) //pitch wheel
        {
            int32_t pitch_wheel = ((pdata[1] & 0x7f) | ((pdata[2] & 0x7f) << 7)) - 8192;
            float t = (float)(pitch_wheel) / (8192.0);
            int32_t note = m_channel[channel].m_note;
            float freq = NoteToFreqFrac((float)note + 10.0 * t);

            m_channel[channel].m_period = (uint32_t)(m_sample_rate/ freq) / 2;

            //m_channel[channel].m_note += pitch_bend;
            printf("pitch bend %i: %i\n", channel, pitch_wheel);
            return;

        }
    }

    void render_samples(int n_samples, float* pOut)
    {
        int channels = 2;

        for (int i = 0; i < n_samples; i++)
        {
            float val = synthesize();
            for (unsigned short j = 0; j < channels; j++)
            {
                *pOut++ = val;
            }
        }
    }

    void printKeyboard()
    {
        char keyboard[128+1];
        for (int i = 0; i < 128; i++)
            keyboard[i] = '.';
        keyboard[128] = 0;

        for (int t = 0; t < _countof(m_channel); t++)
            if (m_channel[t].m_envelope != Channel::OFF )
                keyboard[m_channel[t].m_note] = '#';

        printf("%s\n", keyboard);
    }
};
Piano piano(48000);
#endif

#ifdef SYNTH_MT32EMU
//#define MT32EMU_API_TYPE 3
#include "mt32emu.h"


class MT32EMU : public Instrument, public MT32Emu::ReportHandler
{
    //mt32emu_service_i service;
    MT32Emu::Synth* m_pSynth;
public:
    MT32EMU(int32_t sample_rate)
    {       
        MT32Emu::FileStream romFile1;
        romFile1.open("../roms/mt32/ctrl_mt32_2_04.rom");
        const MT32Emu::ROMImage* pControlROMImage = MT32Emu::ROMImage::makeROMImage(&romFile1);

        MT32Emu::FileStream romFile2;
        romFile2.open("../roms/mt32/pcm_mt32.rom");
        const MT32Emu::ROMImage* pPCMROMImage = MT32Emu::ROMImage::makeROMImage(&romFile2);
        
        m_pSynth = new MT32Emu::Synth(this);
        if (m_pSynth->open(*pControlROMImage, *pPCMROMImage))
        {
            int sampleRate = m_pSynth->getStereoOutputSampleRate();
            m_pSynth->setOutputGain(1);
            m_pSynth->setReverbOutputGain(0);
            m_pSynth->setMIDIDelayMode(MT32Emu::MIDIDelayMode_IMMEDIATE);
            m_pSynth->selectRendererType(MT32Emu::RendererType_FLOAT);
            const MT32Emu::Bit8u StandardMIDIChannelsSysEx[] = { 0x10, 0x00, 0x0D, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };
            m_pSynth->writeSysex(0x10, StandardMIDIChannelsSysEx, sizeof(StandardMIDIChannelsSysEx));
            
        }

        //mt32emu_set_actual_stereo_output_samplerate(context);
    }

    void send_midi(uint8_t* pdata, size_t size)
    {
        uint8_t data[4] = { pdata[0]|1, pdata[1], pdata[2],0 };
        MT32Emu::Bit32u d = *((MT32Emu::Bit32u*)data);
        m_pSynth->playMsg(d);
    }

    void render_samples(int n_samples, float* pOut)
    {
        m_pSynth->render(pOut, n_samples);
    }

    void printKeyboard()
    {
        printf("\n");
    }
};

MT32EMU piano(48000);
#endif

#ifdef SYNTH_DX7
void dx7_start(int sample_rate, size_t buf_size);
void dx7_render(int n_samples, int16_t* buffer);
void dx7_send_midi(unsigned char* pData, size_t size);

class DX7 : public Instrument
{
public:
    DX7(int32_t sample_rate)
    {
        dx7_start(sample_rate, 1024);

        uint8_t* pBank_buffer;
        size_t bank_size;
        load_file("../rom1a.syx", &pBank_buffer, &bank_size);
        dx7_send_midi(pBank_buffer, bank_size);

        unsigned char data[2] = { 0xc0 , 0 };
        dx7_send_midi(data, 2);
    }

    void send_midi(uint8_t* pdata, size_t size)
    {
        dx7_send_midi(pdata, size);
    }
    
    void render_samples(int n_samples, float* pOut)
    {
        int channels = 2;

        int16_t o[10240];
        dx7_render(n_samples, o);
        for (int i = 0; i < n_samples; i++)
        {
            float val = ((float)o[i]) / 65535.0f;
            for (unsigned short j = 0; j < channels; j++)
            {
                *pOut++ = val;
            }
        }
    }

    void printKeyboard()
    {
        printf("\n");
    }

};
DX7 piano(48000);
#endif

///////////////////////////////////////////////////////////////
// MidiStream 
///////////////////////////////////////////////////////////////
class MidiStream
{
public:
    uint8_t*m_pTrack = NULL;
    uint8_t*m_pTrackFin = NULL;

    MidiStream(uint8_t *ini, uint8_t *fin)
    {
        m_pTrack = ini;
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

    void Back()
    {
        m_pTrack--;
    }

    unsigned char GetByte()
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

///////////////////////////////////////////////////////////////
// MidiTrack
///////////////////////////////////////////////////////////////
uint32_t ticksPerQuarterNote;  //or ticks per beat
float quarterNote = 24;
uint32_t microSecondsPerMidiTick;

class MidiTrack : public MidiStream
{
    uint64_t m_nextTime;
    uint32_t m_channel;
    unsigned char m_lastType;
    const char* m_TrackName;
    const char* m_InstrumentName;
    uint8_t m_status;
    uint32_t m_timesignatureNum = 4;
    uint32_t m_timesignatureDen = 4;

public:

    MidiTrack(uint8_t *ini, uint8_t*fin) : MidiStream(ini, fin)
    {
        m_nextTime = GetVar();
        m_channel = -1;
        m_lastType = 0;
        m_TrackName = "none";
    }

    uint32_t play(uint32_t microSeconds)
    {
        // too early for the next event, bail out
        if (microSeconds < m_nextTime)
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

                        microSecondsPerMidiTick = (microseconds_per_quarter / ticksPerQuarterNote);
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

                        quarterNote = 100000.0f / (float)cc;

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
                    piano.send_midi(data, 3);
                }
                else if (subType == 0xC || subType == 0xD) // channel pressure
                {
                    unsigned char cc = GetByte();

                    uint8_t data[2] = { m_status , cc };
                    piano.send_midi(data, 2);
                }
                else if (subType == 0xE)
                {
                    unsigned char cc = GetByte();
                    unsigned char dd = GetByte();

                    uint8_t data[3] = { m_status , cc, dd };
                    piano.send_midi(data, 3);
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
};

///////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////

class Midi
{
    uint32_t m_time;

    MidiTrack *pTrack[50];
    uint32_t m_tracks;
    uint32_t m_nextEventTime;

    uint32_t m_samples_to_render;
public:
    Midi()
    {
        m_time = 0;
        m_tracks = 0;
        m_nextEventTime = 0;
        m_samples_to_render = 0;
    }

    size_t RenderMidi(const uint32_t sampleRate, const uint32_t channels, size_t size, float *pOut)
    {
        size_t size_org = size;

        while (size>0)
        {
            if (m_samples_to_render == 0)
            {
                m_nextEventTime = 0xffffffff;
                for (uint32_t t = 0; t < m_tracks; t++)
                {
                    uint32_t next = pTrack[t]->play(m_time);
                    if (next > 0)
                    {
                        m_nextEventTime = MIN(next, m_nextEventTime);
                    }
                }

                if (m_nextEventTime == 0xffffffff)
                    break;

                printf("%6i - %4i [%5i]", m_time, m_nextEventTime-m_time, microSecondsPerMidiTick);
                piano.printKeyboard();

                uint64_t event_time = (uint64_t)(m_nextEventTime - m_time) * microSecondsPerMidiTick;
                m_time = m_nextEventTime;

                m_samples_to_render = ((uint64_t)sampleRate * event_time) / 1000000;
            }

            if (true)
            {
                int n_samples = MIN(size, m_samples_to_render);
                piano.render_samples((int)n_samples, pOut);
                pOut += n_samples * 2;
                size -= n_samples;

                m_samples_to_render -= n_samples;
            }
            else
            {
                m_samples_to_render = 0;
            }
        }

        return size_org - size;
    }

    bool LoadMidi(uint8_t *midi_buffer, size_t midi_buffer_size)
    {
        MidiStream ch(midi_buffer, midi_buffer + midi_buffer_size);

        if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'h' && ch.GetByte() == 'd')
        {
            ch.GetLength(4);

            uint32_t format = ch.GetLength(2);
            uint32_t tracks = ch.GetLength(2);
            ticksPerQuarterNote = ch.GetLength(2);

            uint32_t tempo = ((60 * 1000000) / 110);

            microSecondsPerMidiTick = tempo / ticksPerQuarterNote;

            printf("format %i, tracks %i, ticksPerQuarterNote %i\n", format, tracks, ticksPerQuarterNote);
        }

        m_tracks = 0;
        while (ch.done() == false)
        {
            if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'r' && ch.GetByte() == 'k')
            {
                int length = ch.GetLength(4);
                pTrack[m_tracks] = new MidiTrack(ch.m_pTrack, ch.m_pTrack + length);
                ch.Skip(length);
                m_tracks++;
            }
        }

        m_time = 0;

        return true;
    }
};

///////////////////////////////////////////////////////////////
// Audio out and synchronization
///////////////////////////////////////////////////////////////

#if _WIN32

#include <windows.h>
#include <mmreg.h>
#pragma comment(lib, "Winmm.lib")

HANDLE event;
uint32_t buffers_queued = 0;

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{    
    if (uMsg == WOM_OPEN)
    {
        event = CreateEvent(NULL, FALSE, FALSE, NULL);
        buffers_queued = 0;
    }
    else if (uMsg == WOM_CLOSE)
    {
        CloseHandle(event);
    }
    else if (uMsg == WOM_DONE)
    {
        buffers_queued--;
        SetEvent(event);
    }
}

MMRESULT playLoop(Midi *pMidi, float nSeconds, uint32_t samplesPerSecond = 48000)
{
    const uint32_t channels = 2;
    const uint32_t bufferCount = 2;

    WAVEFORMATEX waveFormat = { 0 };
    waveFormat.cbSize = 0;
    waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    waveFormat.nChannels = channels;
    waveFormat.nSamplesPerSec  = samplesPerSecond;
    waveFormat.wBitsPerSample  = CHAR_BIT * sizeof(float);
    waveFormat.nBlockAlign     = waveFormat.nChannels * waveFormat.wBitsPerSample / CHAR_BIT;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

    HWAVEOUT hWavOut = NULL;
    MMRESULT mmresult = waveOutOpen(&hWavOut, WAVE_MAPPER, &waveFormat, (DWORD_PTR)waveOutProc, NULL, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
    if (mmresult == MMSYSERR_NOERROR)
    {
        timeBeginPeriod(1);

        // allocate buffer
        //
        uint32_t frameSize = (uint32_t)(channels * sizeof(float));
        uint32_t nBuffer = (uint32_t)(nSeconds * samplesPerSecond);        
        uint32_t index = 0;

        WAVEHDR buf_hdr[bufferCount];
        for (int i = 0; i < bufferCount; i++)
        {
            buf_hdr[i] = { 0 };
            buf_hdr[i].dwBufferLength = (ULONG)(nBuffer * frameSize);
            buf_hdr[i].lpData = (LPSTR)malloc(nBuffer * frameSize);
            mmresult = waveOutPrepareHeader(hWavOut, &buf_hdr[i], sizeof(buf_hdr[0]));
            assert(mmresult == MMSYSERR_NOERROR);
        }

        uint32_t blocksRendered = 0;
        for (;;)
        {
            if (GetKeyState(27) & 0x8000)
                break;

            // Render audio
            //
            while (buffers_queued < bufferCount)
            {
                if (pMidi->RenderMidi(waveFormat.nSamplesPerSec, waveFormat.nChannels, buf_hdr[index].dwBufferLength / frameSize, (float*)buf_hdr[index].lpData) == false)
                    break;
                mmresult = waveOutWrite(hWavOut, &buf_hdr[index], sizeof(buf_hdr[0]));
                assert(mmresult == MMSYSERR_NOERROR);

                index = (index + 1) % bufferCount;
                buffers_queued++;
            }

            WaitForSingleObject(event, INFINITE);

        }

        timeEndPeriod(1); 
        waveOutClose(hWavOut);

        while (buffers_queued >0)
            WaitForSingleObject(event, INFINITE);

        for (int i = 0; i < bufferCount; i++) 
        {
            mmresult = waveOutUnprepareHeader(hWavOut, &buf_hdr[i], sizeof(buf_hdr[0]));
            assert(mmresult == MMSYSERR_NOERROR);
            free(buf_hdr[i].lpData);
        }
    }


    return mmresult;
}

#elif __linux__

#include <../alsa/asoundlib.h>
#include <pthread.h>

pthread_cond_t condition;
pthread_mutex_t signalMutex;
uint32_t blocksPlayed = 0;
size_t nBuffer;

static void callback(snd_async_handler_t *pcm_callback)
{
    snd_pcm_t *handle = snd_async_handler_get_pcm(pcm_callback);
    snd_pcm_sframes_t pcm_avail = snd_pcm_avail(handle);
    
    blocksPlayed++;
    pthread_cond_signal(&condition);    
}

snd_pcm_t *playback_handle;
float buf[2*4096];
Midi *pMidi2;
int playback_callback (snd_pcm_sframes_t nframes)
{
    nframes = pMidi2->RenderMidi(48000, 2, nframes, buf);
    if (nframes==0)
        return 0;

    int err;
    if ((err = snd_pcm_writei (playback_handle, buf, nframes)) < 0) {
        fprintf (stderr, "write failed (%s)\n", snd_strerror (err));
    }

    return err;
}

void playLoop(Midi *pMidi, float nSeconds, uint32_t  samplesPerSecond = 48000)
{
    uint32_t channels = 2;
    pMidi2=pMidi;
    pthread_cond_init(&condition, NULL);
    pthread_mutex_init(&signalMutex, NULL);

    const char *device = "default";
    snd_output_t *output = NULL;
    
    int err;       
    if ((err = snd_pcm_open(&playback_handle, device, SND_PCM_STREAM_PLAYBACK,  SND_PCM_ASYNC)) < 0) 
    {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    
    if ((err = snd_pcm_set_params(playback_handle,
                                  SND_PCM_FORMAT_FLOAT_LE,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  channels,
                                  samplesPerSecond,
                                  0,
                                  200000
                                  )) < 0) 
    {   
        printf("Playback open error: %s\n", snd_strerror(err)); 
        exit(EXIT_FAILURE);
    }        
    
    size_t frameSize = snd_pcm_frames_to_bytes(playback_handle, 1);
    printf("framer size %lu\n", frameSize);
        
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    snd_pcm_get_params(	playback_handle, &buffer_size, &period_size);
    printf("Output buffer and period size %lu, %lu\n", buffer_size, period_size);

    snd_pcm_sframes_t frames_to_deliver;

    /* tell ALSA to wake us up whenever 4096 or more frames
        of playback data can be delivered. Also, tell
        ALSA that we'll start the device ourselves.
    */
	snd_pcm_sw_params_t *sw_params;
    if ((err = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
        fprintf (stderr, "cannot allocate software parameters structure (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    if ((err = snd_pcm_sw_params_current (playback_handle, sw_params)) < 0) {
        fprintf (stderr, "cannot initialize software parameters structure (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    if ((err = snd_pcm_sw_params_set_avail_min (playback_handle, sw_params, 4096)) < 0) {
        fprintf (stderr, "cannot set minimum available count (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    if ((err = snd_pcm_sw_params_set_start_threshold (playback_handle, sw_params, 0U)) < 0) {
        fprintf (stderr, "cannot set start mode (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    if ((err = snd_pcm_sw_params (playback_handle, sw_params)) < 0) {
        fprintf (stderr, "cannot set software parameters (%s)\n",
                snd_strerror (err));
        exit (1);
    }

    if ((err = snd_pcm_prepare (playback_handle)) < 0) {
        fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
                snd_strerror (err));
        exit (1);
    }

    while (1) {

        /* wait till the interface is ready for data, or 1 second
            has elapsed.
        */

        if ((err = snd_pcm_wait (playback_handle, 1000)) < 0) {
                fprintf (stderr, "poll failed (%s)\n", strerror (errno));
                break;
        }	           

        /* find out how much space is available for playback data */

        if ((frames_to_deliver = snd_pcm_avail_update (playback_handle)) < 0) {
            if (frames_to_deliver == -EPIPE) {
                fprintf (stderr, "an xrun occured\n");
                break;
            } else {
                fprintf (stderr, "unknown ALSA avail update return value (%ld)\n", 
                        frames_to_deliver);
                break;
            }
        }

        frames_to_deliver = frames_to_deliver > 4096 ? 4096 : frames_to_deliver;

        /* deliver the data */
        int res = playback_callback (frames_to_deliver);
        if (res==0)
        {
            printf("done\n");
            break;
        }
        else if ( res<0) 
        {
            fprintf (stderr, "playback callback failed\n");
            break;
        }
    }
    
    snd_pcm_nonblock(playback_handle, 0); // block
    snd_pcm_drain(playback_handle);

    snd_pcm_close(playback_handle);
}
#endif

///////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    //const char *filename = "../Mario-Sheet-Music-Overworld-Main-Theme.mid";
    //const char* filename = "../32175_Yie-Ar-KungFu.mid";
    //const char* filename = "../slowman.mid";
    //const char *filename = "../AroundTheWorld.mid";
    //const char *filename = "../Tetris - Tetris Main Theme.mid";
    //const char* filename = "../darude-sandstorm.mid";
    //const char* filename = "../Gigi_Dagostino__Lamour_Toujours.mid";
    //const char* filename = "../Never-Gonna-Give-You-Up-3.mid";
    //const char *filename = "../stage-1.mid";
    //const char *filename = "../doom.mid";
    //const char *filename = "../TakeOnMe.mid";
    //const char *filename = "../Guns n Roses - November Rain.mid";
    //const char *filename = "../mozart-piano-concerto-21-2-elvira-madigan-piano-solo.mid";
    //const char *filename = "../John Lennon - Imagine.mid";
    //const char *filename = "../BadRomance.mid";
    const char* filename = "../The Legend of Zelda Ocarina of Time - Song of Storms.mid";
    //const char* filename = "../The Legend of Zelda Ocarina of Time - New Ocarina Melody.mid";
    //const char *filename = "../Guns n Roses - Sweet Child O Mine.mid";
    //const char *filename = "../goonies.mid";
    //const char *filename = "../kungfu.mid";
    //const char *filename = "../metalgr1.mid";
    //const char *filename = "../MetalGearMSX_OperationIntrudeN313.mid";
    //const char *filename = "../Theme_of_tara_jannee2.mid";

    if (argc > 1)
    {
        filename = argv[1];
    }
    
    uint8_t* pMidi_buffer;
    size_t midi_size;
    load_file(filename, &pMidi_buffer, &midi_size);

    Midi midi;
    
    if (midi.LoadMidi(pMidi_buffer, midi_size))
    {
        playLoop(&midi, 0.1f);
        free(pMidi_buffer);
    }

    return 0;
}
