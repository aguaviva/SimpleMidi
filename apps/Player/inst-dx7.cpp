#define _USE_MATH_DEFINES 1
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "instrument.h"
#include "misc.h"

void *dx7_start(int sample_rate);
void dx7_render(void *pInstance, int n_samples, int16_t* buffer);
void dx7_send_midi(void *pInstance, unsigned char* pData, size_t size);

class DX7 : public Instrument
{
    void *m_pInstance;

public:
    DX7(int32_t sample_rate, char *bank)
    {
        m_pInstance = dx7_start(sample_rate);

        //load bank
        uint8_t* pBank_buffer;
        size_t bank_size;
        load_file(bank?bank:"./rom1a.syx", &pBank_buffer, &bank_size);
        dx7_send_midi(m_pInstance, pBank_buffer, bank_size);

        // set instrument o by default
        unsigned char data[2] = { 0xc0 , 0 };
        dx7_send_midi(m_pInstance, data, 2);
    }

    void send_midi(uint8_t* pdata, size_t size)
    {
        dx7_send_midi(m_pInstance, pdata, size);
    }
    
    void render_samples(uint32_t n_samples, float* pOut)
    {
        int channels = 2;

        int16_t o[10240];
        dx7_render(m_pInstance, n_samples, o);
        for (int i = 0; i < n_samples; i++)
        {
            float val = ((float)o[i]) / 65535.0f;
            for (unsigned short j = 0; j < channels; j++)
            {
                *pOut++ += val;
            }
        }
    }
};


Instrument *GetDX7(char *bank)
{
    return new DX7(48000, bank);
}