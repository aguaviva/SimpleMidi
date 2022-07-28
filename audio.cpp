///////////////////////////////////////////////////////////////
// Audio out and synchronization
///////////////////////////////////////////////////////////////
#include <stdint.h>

#include "audio.h"

#define MIN(a,b) (a<b?a:b)

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

void playLoop(float nSeconds, uint32_t  samplesPerSecond, audio_callback func)
{
    uint32_t channels = 2;
    snd_pcm_t *playback_handle;

    const char *device = "default";
    
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
                                  (uint32_t)(nSeconds*1000000.0f)
                                  )) < 0) 
    {   
        printf("Playback open error: %s\n", snd_strerror(err)); 
        exit(EXIT_FAILURE);
    }        
    
    size_t frameSize = snd_pcm_frames_to_bytes(playback_handle, 1);
    printf("framer size %lu\n", frameSize);
        
    snd_pcm_uframes_t buffer_size, period_size;
    snd_pcm_get_params(	playback_handle, &buffer_size, &period_size);
    printf("Output buffer and period size %lu, %lu\n", buffer_size, period_size);

    if ((err = snd_pcm_prepare (playback_handle)) < 0) {
        fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
                snd_strerror (err));
        exit (1);
    }

    uint32_t frames_to_render = buffer_size;

    float *pBuf = (float *)malloc(frames_to_render*sizeof(float)*channels);

    while (1) 
    {
        // wait till the interface is ready for data, or 1 second has elapsed.   
        if ((err = snd_pcm_wait (playback_handle, 1000)) < 0) {
                fprintf (stderr, "poll failed (%s)\n", strerror (errno));
                break;
        }	           

        // find out how much space is available for playback data 
        snd_pcm_sframes_t frames_to_deliver;
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

        frames_to_deliver = MIN(frames_to_render, frames_to_deliver);


        snd_pcm_sframes_t nframes = func(frames_to_deliver, pBuf);
        if (nframes==0)
        {
            printf("done\n");
            break;
        }

        if ((err = snd_pcm_writei (playback_handle, pBuf, nframes)) < 0) {
            fprintf (stderr, "write failed (%s)\n", snd_strerror (err));
            break;
        }
    }
    
    free(pBuf);

    snd_pcm_nonblock(playback_handle, 0); // block
    snd_pcm_drain(playback_handle);
    snd_pcm_close(playback_handle);
}
#endif
