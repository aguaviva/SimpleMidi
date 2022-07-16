///////////////////////////////////////////////////////////////
// Simple square wave synthesizer
///////////////////////////////////////////////////////////////

#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "instrument.h"

#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

float NoteToFreq(int note)
{
    return (float)pow(2.0f, (note - 69) / 12.0f) * 440.0f;
}

float NoteToFreqFrac(float note)
{
    return (float)pow(2.0f, (note - 69) / 12.0f) * 440.0f;
}

void printNote(unsigned char note)
{
    const char *notes[] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };
    printf("%s%i", notes[note % 12], (note / 12)-1);
}

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

            //return (float)(m_phase - time) / (float)m_period;
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
                *pOut++ += val;
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

Instrument *GetPiano()
{
    return new Piano(48000);
}