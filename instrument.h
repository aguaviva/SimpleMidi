#pragma once

class Instrument
{
public:
    bool m_mute = false;

    virtual void send_midi(uint8_t* pdata, size_t size) = 0;
    virtual void render_samples(int n_samples, float* pOut) = 0;
};

