#ifdef HAS_MT32EMU
#include "instrument.h"
#include <mt32emu/mt32emu.h>
#include <iostream>
#include <memory>
#include <vector>

class MT32EMU : public Instrument
{
    MT32Emu::FileStream m_controlROMFile;
    MT32Emu::FileStream m_pcmROMFile;
    const MT32Emu::ROMImage* pControlROMImage;
    const MT32Emu::ROMImage* pPCMROMImage;
    std::unique_ptr<MT32Emu::Synth> m_pSynth;
    std::unique_ptr<MT32Emu::SampleRateConverter> m_sampleRateConverter;
    std::vector<float> m_renderBuffer;
    uint32_t m_sampleRate;
    bool m_opened;

public:
    explicit MT32EMU(int32_t sample_rate)
        : pControlROMImage(nullptr),
          pPCMROMImage(nullptr),
          m_sampleRate(sample_rate > 0 ? static_cast<uint32_t>(sample_rate) : 48000),
          m_opened(false)
    {
        if (m_controlROMFile.open("./instruments/munt/ctrl_mt32_2_04.rom")) {
            pControlROMImage = MT32Emu::ROMImage::makeROMImage(&m_controlROMFile);
        } else {
            std::cerr << "Failed to open control ROM file!\n";
        }

        if (m_pcmROMFile.open("./instruments/munt/pcm_mt32.rom")) {
            pPCMROMImage = MT32Emu::ROMImage::makeROMImage(&m_pcmROMFile);
        } else {
            std::cerr << "Failed to open PCM ROM file!\n";
        }

        if (pControlROMImage && pPCMROMImage)
        {
            m_pSynth = std::make_unique<MT32Emu::Synth>(nullptr);
            m_pSynth->selectRendererType(MT32Emu::RendererType_FLOAT);
            const auto outputMode = MT32Emu::SampleRateConverter::getBestAnalogOutputMode(m_sampleRate);
            if (m_pSynth->open(*pControlROMImage, *pPCMROMImage, outputMode))
            {
                m_pSynth->setOutputGain(1.0f);
                m_pSynth->setReverbOutputGain(0.0f);
                m_pSynth->setMIDIDelayMode(MT32Emu::MIDIDelayMode_IMMEDIATE);
                m_sampleRateConverter = std::make_unique<MT32Emu::SampleRateConverter>(
                    *m_pSynth, m_sampleRate, MT32Emu::SamplerateConversionQuality_GOOD);
                m_opened = true;

                const MT32Emu::Bit8u standardMIDIChannelsSysEx[] = {
                    0x10, 0x00, 0x0D, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
                };
                m_pSynth->writeSysex(0x10, standardMIDIChannelsSysEx, sizeof(standardMIDIChannelsSysEx));
            }
            else
            {
                std::cerr << "m_pSynth->open() failed with provided ROMs.\n";
            }
        } else {
            std::cerr << "Failed to create MT-32 ROM images.\n";
        }
    }

    ~MT32EMU()
    {
        m_sampleRateConverter.reset();
        if (m_pSynth) {
            m_pSynth->close();
            m_pSynth.reset();
        }
        if (pControlROMImage) {
            MT32Emu::ROMImage::freeROMImage(pControlROMImage);
        }
        if (pPCMROMImage) {
            MT32Emu::ROMImage::freeROMImage(pPCMROMImage);
        }
    }

    void send_midi(uint8_t* pdata, size_t size)
    {
        if (!m_opened || !pdata || size == 0) return;
        if (pdata[0] < 0x80) {
            m_pSynth->writeSysex(0x10, pdata, static_cast<uint32_t>(size));
            return;
        }
        if (pdata[0] == 0xf0) {
            if (size >= 2 && pdata[size - 1] == 0xf7) {
                m_pSynth->playSysex(pdata, static_cast<uint32_t>(size));
            }
            return;
        }
        if ((size != 2 && size != 3) || pdata[0] < 0x80 || pdata[0] >= 0xf0) return;
        if (MT32Emu::Synth::getShortMessageLength(pdata[0]) != size) return;
        for (size_t i = 1; i < size; ++i) {
            if (pdata[i] > 0x7f) return;
        }

        uint32_t msg = 0;
        for (size_t i = 0; i < size; ++i) {
            msg |= (static_cast<uint32_t>(pdata[i]) << (i * 8));
        }

        m_pSynth->playMsg(msg);
    }

    void reset() override
    {
        if (!m_opened) return;

        m_pSynth->flushMIDIQueue();
        const MT32Emu::Bit8u systemResetSysex[] = {0x7f, 0x00, 0x00};
        m_pSynth->writeSysex(0x10, systemResetSysex, sizeof(systemResetSysex));
        const MT32Emu::Bit8u standardMIDIChannelsSysex[] = {
            0x10, 0x00, 0x0D, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
        };
        m_pSynth->writeSysex(0x10, standardMIDIChannelsSysex, sizeof(standardMIDIChannelsSysex));
    }

    void render_samples(uint32_t n_samples, float* pOut)
    {
        if (!m_opened || !pOut || n_samples == 0) return;

        const size_t sampleCount = static_cast<size_t>(n_samples) * 2;
        m_renderBuffer.resize(sampleCount);
        m_sampleRateConverter->getOutputSamples(m_renderBuffer.data(), n_samples);
        if (m_mute) return;

        for (size_t i = 0; i < sampleCount; ++i) {
            pOut[i] += m_renderBuffer[i];
        }
    }
};

Instrument *GetMT32()
{
    return new MT32EMU(48000);
}

#endif