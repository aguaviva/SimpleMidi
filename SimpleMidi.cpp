// xmidi.cpp : Defines the entry point for the console application.
//
#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    return (float)pow(2.0f, (note - 69) / 12.0f) * 440.0f;
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

class Piano : public Instrument
{
    bool m_omni = false;
    uint32_t m_time;
    struct Channel
    {
        uint32_t m_time=0;
        uint32_t m_velocity = 0;
        uint32_t m_period = 0;
        uint8_t m_note = 0;

        // sqaure stuff
        uint32_t m_phase = 0;
        bool m_square = false;
        float playSquareWave(uint32_t time, float vol)
        {
            while (m_phase < time)
            {
                m_phase += m_period;
                m_square = !m_square;
            }

            return m_square ? vol : -vol;
        }
    };

    Channel m_channel[20];
    uint32_t m_noteDuration;
    uint32_t m_sample_rate;

public:
    Piano(uint32_t sample_rate)
    {
        m_sample_rate = sample_rate;

        m_noteDuration = (uint32_t)(0.3f * 1000000); // in microseconds

        for (int i = 0; i < _countof(m_channel); i++)
            memset(&m_channel[i], 0, sizeof(Channel));

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

        if (channelId >=0)
        {
            m_channel[channelId].m_note = 0;
            m_channel[channelId].m_velocity = velocity;
            m_channel[channelId].m_time = 0;
        }

        return channelId;
    }

    int push(char channelId, unsigned char note, unsigned char velocity)
    {        
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

        if (channelId >= 0)
        {
            m_channel[channelId].m_note = note;
            m_channel[channelId].m_velocity = velocity;
            m_channel[channelId].m_time = m_time;
            m_channel[channelId].m_phase = m_time;

            m_channel[channelId].m_period = (uint32_t)(1000000.0f / NoteToFreq(note))/2;
        }

        return channelId;
    }

    float synthesize()
    {
        float out = 0;
        
        for (int i = 0; i < _countof(m_channel); i++) 
        {
            if (m_channel[i].m_note > 0)
            { 
                // volume decay
                uint32_t timeSincePressed = m_time - m_channel[i].m_time;

                if (timeSincePressed < m_noteDuration)
                {
                    float t = (float)timeSincePressed / (float)m_noteDuration;
                    float vol = 1.0f-t;

                    out += m_channel[i].playSquareWave(m_time, vol);
                }
                else
                {
                    m_channel[i].m_note = 0;
                }
            }
        }

        out /= _countof(m_channel);

        m_time += 1000000 / m_sample_rate;

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
            if (m_channel[t].m_note > 0)
                keyboard[m_channel[t].m_note] = '#';

        printf("%s\n", keyboard);
    }
};

#if 0

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

        unsigned char data[2] = { 0xc0 , 10 };
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
#else
Piano piano(48000);
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
        uint64_t left = m_pTrackFin - m_pTrack;
        return left <= 0;
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
    uint32_t m_nextTime;
    uint32_t m_channel;
    unsigned char m_lastType;

    uint32_t m_timesignatureNum = 4;
    uint32_t m_timesignatureDen = 4;

public:

    MidiTrack(uint8_t *ini, uint8_t*fin) : MidiStream(ini, fin)
    {
        m_nextTime = GetVar();
        m_channel = -1;
        m_lastType = 0;
    }

    uint32_t play(uint32_t microSeconds)
    {
        // too early for the next event, bail out
        if (microSeconds < m_nextTime)
            return m_nextTime;

        // are we done with the stream?
        if (done() == true)
            return m_nextTime;

        //process all events for this instant and compute m_nextTime
        for (;;)
        {
            unsigned char type = GetByte();            
            if ((type & 0x80) == 0)
            {
                Back();
                type = m_lastType;
            }
            else
            {                    
                m_lastType = type;
            }

            //printf("%i:  0x%02x ", m_channel, type);

            if (type == 0xff)
            {
                unsigned char metaType = GetByte();
                unsigned char length = GetByte();

                //printf("    meta: 0x%02x", metaType);
                //printf("    length: 0x%02x", length);
                
                if (metaType >= 0x01 && metaType <= 0x05)
                {
                    char *ChunkType[] = { "text event", "copyright", "track name", "instrument", "lyrics" };
                    printf("    %s: ", ChunkType[metaType-1]);
                    for (int i = 0; i<length; i++)
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
                    uint32_t beatsPerSecond =  1000000 / microseconds_per_quarter;
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
                    m_timesignatureDen = 1<< GetByte();
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
                    unsigned char nn = GetByte();
                    printf("************ THE END ************\n");
                }
                else
                {
                    printf(" ch %i   unknown metaType: 0x%02x, length %i\n", m_channel, metaType, length);
                    Skip(length);
                }
            }
            else if (type == 0xf0)
            {
                printf("System exclusive: ");
                for(;;)
                {
                    unsigned char c = GetByte();
                    if (c == 0xf7)
                        break;
                    printf("%02x ", c);
                }
                printf("\n");
            }
            else if (type == 0xf7)
            {
                unsigned char length = GetByte();
                Skip(length);
            }
            else if (type == 0xfd)
            {
                GetByte();
                GetByte();
            }
            else
            {
                unsigned char subType = type >> 4;
                unsigned char channel = type & 0xf;
                if (subType >= 0x8 && subType <= 0xB)
                {                    
                    unsigned char key = GetByte();
                    unsigned char speed = GetByte();

                    uint8_t data[3] = { type , key, speed };
                    piano.send_midi(data, 3);
                }
                else if (subType == 0xC || subType == 0xD) // channel pressure
                {
                    unsigned char cc = GetByte();

                    uint8_t data[2] = { type , cc};
                    piano.send_midi(data, 2);
                }
                else if (subType == 0xE ) 
                {
                    unsigned char cc = GetByte();

                    uint8_t data[2] = { type , cc };
                    piano.send_midi(data, 2);
                }
                else
                {
                    printf("    unknown type: 0x%02x", type);
                    assert(0);
                }
            }            

            if (done())
                break;

            uint32_t deltaMidiTicks = GetVar();                       
            if (deltaMidiTicks > 0)
            {
                m_nextTime += (deltaMidiTicks * microSecondsPerMidiTick);
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
    uint32_t time;

    MidiTrack *pTrack[50];
    uint32_t tracks;

    uint32_t m_nextEventTime = 0;
public:
    bool RenderMidi(const uint32_t sampleRate, const uint32_t channels, size_t size, float *pOut)
    {
        while (size>0)
        {
            // process event and compute when the next event is
            if (m_nextEventTime <= time)
            {
                m_nextEventTime = 1000000000;
                for (uint32_t t = 0; t < tracks; t++)
                {
                    uint32_t next = pTrack[t]->play(time);
                    if (next > 0)
                        m_nextEventTime = MIN(next, m_nextEventTime);
                }

                printf("%9i - %9i [%9i]", time, m_nextEventTime, microSecondsPerMidiTick);
                piano.printKeyboard();
            }

            // render chunk!
            if (time < m_nextEventTime)
            {
                uint64_t event_time = (m_nextEventTime - time);
                uint64_t n_samples = ((uint64_t)sampleRate * event_time) / 1000000;

                if (n_samples > size)
                {
                    n_samples = size;
                    event_time = (uint64_t)(((uint64_t)1000000 * (uint64_t)n_samples) / sampleRate);
                }

                piano.render_samples(n_samples, pOut);
                pOut += n_samples* 2;

                time += event_time;
                size -= n_samples;
            }

        }

        return true;
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

        tracks = 0;
        while (ch.done() == false)
        {
            if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'r' && ch.GetByte() == 'k')
            {
                int length = ch.GetLength(4);
                pTrack[tracks] = new MidiTrack(ch.m_pTrack, ch.m_pTrack + length);
                ch.Skip(length);
                tracks++;
            }
        }

        time = 0;

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
        uint32_t frameSize = channels * sizeof(float);
        uint32_t nBuffer = (uint32_t)(nSeconds * samplesPerSecond);        
        uint32_t index = 0;

        WAVEHDR buf_hdr[bufferCount];
        for (int i = 0; i < bufferCount; i++)
        {
            WAVEHDR hdr = { 0 };
            hdr.dwBufferLength = (ULONG)(nBuffer * frameSize);
            hdr.lpData = (LPSTR)malloc(nBuffer * frameSize);
            mmresult = waveOutPrepareHeader(hWavOut, &hdr, sizeof(hdr));
            assert(mmresult == MMSYSERR_NOERROR);

            buf_hdr[i] = hdr;
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
                pMidi->RenderMidi(waveFormat.nSamplesPerSec, waveFormat.nChannels, buf_hdr[index].dwBufferLength / frameSize, (float*)buf_hdr[index].lpData);
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

void playLoop(Midi *pMidi, float nSeconds, uint32_t  samplesPerSecond = 48000)
{
    uint32_t channels = 2;

    pthread_cond_init(&condition, NULL);
    pthread_mutex_init(&signalMutex, NULL);

    const char *device = "plughw:0,0";
    snd_output_t *output = NULL;
    
    int err;       
    snd_pcm_t *pcm_handle;
    if ((err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) 
    {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    
    if ((err = snd_pcm_set_params(pcm_handle,
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
    
    size_t frameSize = snd_pcm_frames_to_bytes(pcm_handle, 1);
    printf("framer size %lu\n", frameSize);
        
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    snd_pcm_get_params(	pcm_handle, &buffer_size, &period_size);
    printf("Output buffer and period size %lu, %lu\n", buffer_size, period_size);
    
    // set async mode    
    snd_async_handler_t *pcm_callback;
    err = snd_async_add_pcm_handler(&pcm_callback,pcm_handle,callback,NULL);
    if (err < 0) 
    {
        fprintf(stderr, "unable to add async handler %i (%s)\n", err, snd_strerror(err));
        exit(EXIT_FAILURE);
    } 
           
    // allocate buffer
    float *buffer = (float *)calloc(buffer_size, frameSize);
    nBuffer = (size_t)(period_size);  
    uint32_t bufferCount = buffer_size/period_size;

    uint32_t index = 0;
    uint32_t blocksRendered = 0;
    for (;;)
    {        
        // Render audio
        //
        float *buf = &buffer[index * nBuffer];
        pMidi->RenderMidi(samplesPerSecond, channels, nBuffer, buf);
        blocksRendered++;

        // Play audio
        {
            snd_pcm_sframes_t frames = snd_pcm_writei(pcm_handle, buf, nBuffer);
            if (frames < 0)
                    frames = snd_pcm_recover(pcm_handle, frames, 0);
            if (frames < 0) {
                    printf("snd_pcm_writei failed: %s\n", snd_strerror(err));
                    break;
            }
            //if (frames > 0 && frames < (long)sizeof(buffer))
            //    printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);
            
            index = (index + 1) % bufferCount;
        }

        // Pause rendering thread if we are getting too ahead
        //
        if (blocksRendered  >= blocksPlayed + bufferCount)
        {
            pthread_cond_wait(&condition, &signalMutex);
        }            
    }
    
    snd_pcm_close(pcm_handle);
}
#endif

///////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    const char *filename = "../Mario-Sheet-Music-Overworld-Main-Theme.mid";
    //const char* filename = "../32175_Yie-Ar-KungFu.mid";
    //const char* filename = "../slowman.mid";
    //const char *filename = "../AroundTheWorld.mid";
    //const char *filename = "../test1.mid";
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
