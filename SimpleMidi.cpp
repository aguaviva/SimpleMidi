// xmidi.cpp : Defines the entry point for the console application.
//
#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define _countof(a) (sizeof(a)/sizeof(*(a)))

float NoteToFreq(int note)
{
    return (float)pow(2.0, (note - 49) / 12.0) * 440.0;
}

float playNote(float time, float freq, float vol)
{
    //return (float)(vol * cosf(2 * M_PI * time * freq));
    float v = (float)(vol * cosf(2 * M_PI * time * freq));
    if (v > 0) 
        v = 1;
    else if (v < 0) 
        v = -1;
    return v;
}

void printNote(unsigned char note)
{
    const char *notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    printf("%s%i", notes[note % 12], (note / 12)-1);
}

///////////////////////////////////////////////////////////////
// Simple square wave synthesizer
///////////////////////////////////////////////////////////////
class Piano
{
    struct channel
    {
        unsigned char velocity;
        unsigned char note;
    };

    channel ch[20];
public:
    Piano()
    {
        for (int i = 0; i < _countof(ch); i++)
            memset(&ch[i], 0, sizeof(channel));
    }
    int release(char channel, unsigned char note, unsigned char velocity)
    {
        if (channel==-1)
        {
            for (int i = 0; i < _countof(ch); i++)
            {
                if (ch[i].note == note)
                {
                    ch[i].note = 0;
                    return i;
                }
            }
        }
        else
        {
            ch[channel].note = 0;
        }
        return -1;
    }

    int push(char channel, unsigned char note, unsigned char velocity)
    {        
        if (channel == -1)
        {
            for (int i = 0; i < _countof(ch); i++)
            {
                if (ch[i].note == 0)
                {
                    ch[i].note = note;
                    return i;
                }
            }
        }
        else
        {
            ch[channel].note = note;
        }
        return -1;
    }

    float synthesize(float time)
    {
        float val = 0;
        for (int t = 0; t < _countof(ch); t++)
        {
            if (ch[t].note > 0)
                val += playNote(time, NoteToFreq(ch[t].note), 1);
        }
        val /= _countof(ch);
        return val;
    }

    float print()
    {
        char keyboard[128+1];
        for (int i = 0; i < 128; i++)
        {
            keyboard[i] = '.';
        }
        keyboard[128] = 0;

        for (int t = 0; t < _countof(ch); t++)
            if (ch[t].note > 0)
                keyboard[ch[t].note] = '#';
        printf("%s\n", keyboard);
    }
};

Piano piano;

///////////////////////////////////////////////////////////////
// MidiStream 
///////////////////////////////////////////////////////////////
class MidiStream
{
public:
    unsigned char *pTrack = NULL;
    unsigned char *pTrackFin = NULL;

    MidiStream(unsigned char *ini, unsigned char *fin)
    {
        pTrack = ini;
        pTrackFin = fin;
    }

    uint32_t GetVar()
    {
        uint32_t out = 0;

        for (;;)
        {
            out <<= 7;
            out += *pTrack & 0x7f;

            if ((*pTrack & 0x80) == 0x00)
                break;

            pTrack++;
        }

        pTrack++;

        return out;
    }

    unsigned char GetByte()
    {
        return *pTrack++;
    }

    void Skip(int count)
    {
        pTrack+= count;
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
        int left = pTrackFin - pTrack;
        return left <= 0;
    }
};

///////////////////////////////////////////////////////////////
// MidiTrack
///////////////////////////////////////////////////////////////

uint32_t ticksPerQuarterNote=24;
float quarterNote = 24;
uint32_t bpm = 120;

class MidiTrack : public MidiStream
{
    float nextTime;
    int m_channel;
    unsigned char lastType;
    bool m_omni;
public:

    MidiTrack(int channel, unsigned char *ini, unsigned char *fin) : MidiStream(ini, fin)
    {
        nextTime = GetVar();
        m_channel = channel;
        m_omni = false;
    }

    void play(float seconds)
    {
        while (nextTime <= seconds)
        {
            if (done() == true)
            {
                break;
            }

            unsigned char type = GetByte();
            
            printf("%i ", m_channel);
            printf("%6.3f ", nextTime);
            printf("0x%02x ", type);            

            if (type == 0xff)
            {
                unsigned char metaType = GetByte();
                unsigned char length = GetByte();

                printf("    meta: 0x%02x", metaType);
                printf("    length: 0x%02x", length);

                if (metaType == 0x01)
                {
                    printf("    text event: ");
                    for (int i = 0; i<length; i++)
                        printf("%c", GetByte());
                }
                else if (metaType == 0x03)
                {
                    printf("    track name: ");
                    for (int i = 0; i<length; i++)
                        printf("%c", GetByte());
                }
                else if (metaType == 0x09)
                {
                    printf("    inst: ");
                    for (int i = 0; i<length; i++)
                        printf("%c", GetByte());
                }
                else if (metaType == 0x20)
                {
                    m_channel = GetByte();                    
                }
                else if (metaType == 0x51)
                {
                    uint32_t tempo = GetLength(3);
                    bpm =  60 * 1000000 / tempo;
                    printf("    tempo: %i bpm", bpm);
                }
                else if (metaType == 0x54)
                {
                    unsigned char hr = GetByte();
                    unsigned char mn = GetByte();
                    unsigned char se = GetByte();
                    unsigned char fr = GetByte();
                    unsigned char ff = GetByte();
                }
                else if (metaType == 0x58)
                {
                    unsigned char nn = GetByte();
                    unsigned char dd = pow(2,GetByte());
                    unsigned char cc = GetByte();
                    unsigned char bb = GetByte();
                    
                    quarterNote = 100000.0 / (float)cc;

                    printf("    time sig: %i/%i MIDI clocks per quarter-dotted %i", nn,dd, cc);
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
                    printf("    unknown metaType: 0x%02x", metaType);
                    Skip(length);
                }
            }
            else if (type == 0xf0)
            {
                unsigned char length = GetByte();
                Skip(length);
            }
            else if (type == 0xf7)
            {
                unsigned char length = GetByte();
                Skip(length);
            }
            else
            {
                unsigned char param1;

                if ((type & 0x80) == 0)
                {
                    param1 = type;
                    type = lastType;
                }
                else
                {
                    param1 = GetByte();
                    lastType = type;
                }

                unsigned char subType = type >> 4;
                unsigned char channel = type & 0xf;

                if (subType == 0x8 || subType == 0x9)
                {                    
                    unsigned char key = param1;
                    unsigned char speed = GetByte();

                    int i;

                    if (subType == 8)
                    {
                        i = piano.release(m_omni ? channel : -1, key, speed);
                    }
                    else if (subType == 9)
                    {
                        i = piano.push(m_omni ? channel : -1, key, speed);
                    }

                    //'printf("    ac: %c ch: %i note: ", channel + 0*(subType == 8) ?'^':'v', i);
                    printf("    ac: %c ch: %i note: ", (subType == 8) ? '^' : 'v', channel);
                    printNote(key);
                }
                else if (subType == 0xA)
                {
                    unsigned char cc = param1;
                    unsigned char nn = GetByte();

                    //printf("    ch: %i ch: %i controller: %2i controller: %2i", channel, m_channel, cc, nn);
                }
                else if (subType == 0xB)
                {
                    unsigned char cc = param1;
                    unsigned char nn = GetByte();

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
                    //printf("    ch: %i ch: %i controller: %2i controller: %2i", channel, m_channel, cc, nn);
                }
                else if (subType >= 0xC && subType <= 0xE)
                {
                }
                else if (subType == 0xE)
                {
                    unsigned char lsb = param1;
                    unsigned char msb = GetByte();
                }
                else
                {
                    printf("    unknown type: 0x%02x", type);
                    assert(0);
                }
            }            

            printf("\n");

            if (done())
                break;

            float c = (float)(bpm * ticksPerQuarterNote) / 60.0;

            uint32_t deltaTicks = GetVar();
            if (c >0)
                nextTime += deltaTicks / c;
        }
    }
};

///////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////

class Midi
{
    float time;

    MidiTrack *pTrack[50];
    int tracks;

	unsigned char *midi;

public:
    void RenderMidi(int sampleRate, int channels, size_t size, float *pOut)
    {
        for (size_t i = 0; i < size; i++)
        {
            for (size_t t = 0; t < tracks; t++)
            {
                pTrack[t]->play(time);
            }

            float val = piano.synthesize(time);

            time += 1.0 / sampleRate;

            for (unsigned short j = 0; j < channels; j++)
            {
                *pOut++ = val;
            }
        }
    }

    bool LoadMidi(const char *filename)
    {
        FILE *f;
        //fopen_s(&f, filename, "rb");
        f = fopen(filename, "rb");		
        if (f == NULL)
        {
            printf("Can't open %s\n", filename);
            return false;
        }

        fseek(f, 0L, SEEK_END);
        size_t midiSize = ftell(f);
        fseek(f, 0L, SEEK_SET);

        midi = (unsigned char *)malloc(midiSize);

        fread(midi, midiSize, 1, f);
        fclose(f);

        MidiStream ch(midi, midi + midiSize);

        if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'h' && ch.GetByte() == 'd')
        {
            ch.GetLength(4);

            uint32_t format = (ch.GetByte() << 8) + ch.GetByte();
            uint32_t tracks = (ch.GetByte() << 8) + ch.GetByte();
            ticksPerQuarterNote = (ch.GetByte() << 8) + ch.GetByte();

            printf("format %i, tracks %i, ticksPerQuarterNote %i\n", format, tracks, ticksPerQuarterNote);
        }

        tracks = 0;
        while (ch.done() == false)
        {
            if (ch.GetByte() == 'M' && ch.GetByte() == 'T' && ch.GetByte() == 'r' && ch.GetByte() == 'k')
            {
                int length = ch.GetLength(4);
                pTrack[tracks] = new MidiTrack(tracks, ch.pTrack, ch.pTrack + length);
                ch.Skip(length);
                tracks++;
            }
        }

        time = 0;

		return true;
    }

    void UnloadMidi()
    {
        free(midi);
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
DWORD blocksPlayed = 0;

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{    
    if (uMsg == WOM_OPEN)
    {
        event = CreateEvent(NULL, FALSE, FALSE, NULL);
        blocksPlayed = 0;
    }
    else if (uMsg == WOM_OPEN)
    {
        CloseHandle(event);
    }
    else if (uMsg == WOM_DONE)
    {
        blocksPlayed++;
        SetEvent(event);
    }
}

MMRESULT playLoop(Midi *pMidi, float nSeconds, uint32_t samplesPerSecond = 48000)
{
	uint32_t channels = 2;
	uint32_t bufferCount = 2;
    
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
		uint32_t nBuffer = (nSeconds * samplesPerSecond);
		float *buffer = (float *)malloc(bufferCount * nBuffer * frameSize);
		uint32_t index = 0;

        int blocksRendered = 0;
		for (;;)
		{
			if (GetKeyState(27) & 0x8000)
				break;

			float *buf = &buffer[index * nBuffer  * channels];

			{
				WAVEHDR hdr = { 0 };
				hdr.dwBufferLength = (ULONG)(nBuffer * frameSize);
				hdr.lpData = (LPSTR)buf;
				mmresult = waveOutUnprepareHeader(hWavOut, &hdr, sizeof(hdr));
			}

            // Render audio
            //
            pMidi->RenderMidi(waveFormat.nSamplesPerSec, waveFormat.nChannels, nBuffer, buf);
            blocksRendered++;

            // Play audio
            {
                WAVEHDR hdr = { 0 };
                hdr.dwBufferLength = (ULONG)(nBuffer * frameSize);
                hdr.lpData = (LPSTR)buf;
                mmresult = waveOutPrepareHeader(hWavOut, &hdr, sizeof(hdr));
				assert(mmresult == MMSYSERR_NOERROR);

				mmresult = waveOutWrite(hWavOut, &hdr, sizeof(hdr));
				assert(mmresult == MMSYSERR_NOERROR);

				index = (index + 1) % bufferCount;
            }

			// Pause rendering thread if next buffer is being used by the HW
			//
			if (blocksRendered >= blocksPlayed + bufferCount)
			{
				WaitForSingleObject(event, INFINITE);	
			}			
        }

        timeEndPeriod(1); 
        waveOutClose(hWavOut);

		free(buffer);
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
    Midi midi;
	if (midi.LoadMidi("../Mario-Sheet-Music-Overworld-Main-Theme.mid"))
	{
		playLoop(&midi, .1);
		midi.UnloadMidi();
	}
    return 0;
}


