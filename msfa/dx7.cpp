


#include "synth.h"
#include "synth_unit.h"

struct DX7_object
{
    RingBuffer *ring_buffer;
    SynthUnit * synth_unit;
};

void *dx7_start(int sample_rate)
{
    SynthUnit::Init(sample_rate);

    DX7_object *pO = new DX7_object();
    pO->ring_buffer = new RingBuffer();  
    pO->synth_unit = new SynthUnit(pO->ring_buffer);

    return pO;
}

void dx7_render(void *dx7_object, int n_samples, int16_t* buffer)
{
    ((DX7_object *)dx7_object)->synth_unit->GetSamples(n_samples, buffer);
}

void dx7_send_midi(void *dx7_object, unsigned char* pData, size_t size)
{
    ((DX7_object *)dx7_object)->ring_buffer->Write((const uint8_t *)pData, (int)size);
}