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
#include "misc.h"

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

class ADSR
{
    enum Envelope { OFF, ATTACK, DECAY, SUSTAIN, RELEASE};

    Envelope m_envelope = OFF;
    uint32_t m_time = 0;
    uint32_t m_sample_rate = 0;

    // 
    uint32_t attackDuration;
    uint32_t decayDuration;
    uint32_t releaseDuration;

public:

    void set_adr(float attack, float duration, float release)
    {
        attackDuration  = m_sample_rate * attack;
        decayDuration   = m_sample_rate * duration;
        releaseDuration = m_sample_rate * release;
    }

    void set_sample_rate(uint32_t sample_rate)
    {
        m_sample_rate = sample_rate;
    }

    void press()
    {
        m_envelope=ATTACK;
        m_time = 0;
    }

    void release()
    {
        m_envelope=RELEASE;
        m_time = 0;
    }

    float render(uint32_t time2)
    {
        float vol = 0.0;
        m_time++;

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

            case SUSTAIN:
            {
                vol = 0.5f;                    
                break;
            }

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
                vol = 0.0f;
        }        

        //printf("%f \n", vol);
        return vol;
    }
};

class SquareWaveOscillator
{
    uint32_t m_time=0;
    uint32_t m_period = 0;
    uint32_t m_phase = 0;
    bool m_square = false;

public:
    float render(uint32_t time)
    {
        m_time++;

        while (m_phase < time)
        {
            m_phase += m_period;
            m_square = !m_square;
        }

        //return (float)(m_phase - time) / (float)m_period;
        return m_square ? 1.0f : -1.0f;
    }

    void press(uint32_t time, uint32_t period) 
    {
        m_time = 0;
        m_phase = time;
        m_period = period;
    }

    void set_period(uint32_t period)
    {
        m_period = period;
    }
};

struct Channel
{
    SquareWaveOscillator osc;
    uint32_t m_velocity = 0;
    uint8_t m_note = 0;
    uint32_t m_sample_rate = 0;
    
public:
    ADSR m_adsr;

    void set_sample_rate(uint32_t sample_rate)
    {
        m_sample_rate = sample_rate;
        m_adsr.set_sample_rate(sample_rate);
    }

    void press(uint32_t time, unsigned char note, unsigned char velocity)
    {
        if(velocity>0)
        {
            m_adsr.press();
            m_note = note;
            m_velocity = velocity;
            osc.press(time, (uint32_t)(m_sample_rate / NoteToFreq(note))/2);
        }
        else
        {
            release(time, note, velocity);
        }
    }

    void release(uint32_t time, unsigned char note, unsigned char velocity)
    {
        m_adsr.release();
    }

    float render(uint32_t time)
    {
        if (m_note <=0)
            return 0;

        float vol = m_adsr.render(time);
        if (vol==0.0f)
        {
            m_note = 0;
            return 0;
        }

        return osc.render(time) * vol * (m_velocity/127.0f);
    }
};


class Piano : public Instrument
{
    bool m_omni = false;
    uint32_t m_time;

    Channel m_channel[20];
    uint32_t m_sample_rate;
    uint32_t m_noteDuration;

public:
    Piano(uint32_t sample_rate)
    {
        m_sample_rate = sample_rate;

        for (int i = 0; i < _countof(m_channel); i++)
            m_channel[i].set_sample_rate(sample_rate);            

        set_adsr(0.01f, 0.1f, 0.2f);

        m_time = 0;
    }

    void set_adsr(float attack, float duration, float release)
    {
        for (int i = 0; i < _countof(m_channel); i++)
        {
            m_channel[i].m_adsr.set_adr(0.01f, 0.1f, 0.2f);
        }
    }

    int release(char channelId, unsigned char note, unsigned char velocity)
    {
        if (channelId == -1)
        {
            for (uint32_t i = 0; i < _countof(m_channel); i++)
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
            for (uint32_t i = 0; i < _countof(m_channel); i++)
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
            for (uint32_t i = 0; i < _countof(m_channel); i++)
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
        
        for (uint32_t i = 0; i < _countof(m_channel); i++) 
        {
            out += m_channel[i].render(m_time);
        }

        out /= _countof(m_channel);

        m_time++;

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
        else if (subType == 0xE) //pitch wheel
        {
            int32_t pitch_wheel = ((pdata[1] & 0x7f) | ((pdata[2] & 0x7f) << 7)) - 8192;
            float t = (float)(pitch_wheel) / (8192.0);
            int32_t note = m_channel[channel].m_note;
            float freq = NoteToFreqFrac((float)note + 10.0 * t);

            m_channel[channel].osc.set_period( (uint32_t)(m_sample_rate/ freq) / 2);

            //m_channel[channel].m_note += pitch_bend;
            LOG("pitch bend %i: %i\n", channel, pitch_wheel);
            return;

        }
    }

    void render_samples(uint32_t n_samples, float* pOut)
    {
        const uint32_t channels = 2;

        for (uint32_t i = 0; i < n_samples; i++)
        {
            float val = synthesize();

            if (m_mute)
                val = 0.0f;

            for (unsigned short j = 0; j < channels; j++)
            {
                *pOut++ += val;
            }
        }
    }
};

Instrument *GetNewPiano()
{
    return new Piano(48000);
}
