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
};

MT32EMU piano(48000);
#endif
