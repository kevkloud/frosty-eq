#include "DspCore.h"
#include <algorithm>
#include <cstring>

namespace frostyeq
{

namespace
{
    float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

    float toLog2Hz (float hz) noexcept { return std::log2 (std::max (hz, 1.0f)); }
    float fromLog2Hz (float l) noexcept { return std::exp2 (l); }

    /** Bit-exact float comparison, deliberately. Smoother::tick snaps to its
        target once inside epsilon, so a settled parameter compares identical
        and the coefficient recomputation can be skipped entirely. Written with
        < rather than == to say that this is intended, not an oversight. */
    constexpr bool exactly (float a, float b) noexcept
    {
        return ! (a < b) && ! (b < a);
    }

    bool sameSettings (const EqSettings& a, const EqSettings& b) noexcept
    {
        return a.model == b.model
            && a.midHiQ == b.midHiQ
            && exactly (a.hfFreqHz,  b.hfFreqHz)  && exactly (a.hfGainDb,  b.hfGainDb)
            && exactly (a.midFreqHz, b.midFreqHz) && exactly (a.midGainDb, b.midGainDb)
            && exactly (a.lfFreqHz,  b.lfFreqHz)  && exactly (a.lfGainDb,  b.lfGainDb)
            && exactly (a.hpfFreqHz, b.hpfFreqHz) && exactly (a.lpfFreqHz, b.lpfFreqHz);
    }
}

//==============================================================================
void DspCore::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate  = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maxBlock    = std::max (maxBlockSize, 1);
    maxChannels = std::clamp (numChannels, 1, (int) networks.size());

    for (auto& n : networks)
        n.prepare (sampleRate);

    // Coefficients update once per sub-block, so the smoothers run at that rate.
    const auto controlRate = sampleRate / (double) kSubBlock;

    for (auto* s : { &hfFreqSm, &midFreqSm, &lfFreqSm })
        s->prepare (controlRate, 25.0);            // frequency glides

    for (auto* s : { &hfGainSm, &midGainSm, &lfGainSm,
                     &inputGainSm, &outputLevelSm, &mixSm, &autoGainSm })
        s->prepare (controlRate, 20.0);

    // All allocation happens here. process() must never allocate.
    dryScratch.assign ((size_t) (maxBlock * maxChannels), 0.0f);

    settingsValid = false;
    reset();
}

void DspCore::reset() noexcept
{
    for (auto& n : networks)
        n.reset();

    std::fill (dryScratch.begin(), dryScratch.end(), 0.0f);
}

//==============================================================================
void DspCore::setParams (const Params& p) noexcept
{
    params = p;

    const auto hfHz  = highShelfFreq (p.model, p.hfFreqIndex);
    const auto midHz = midFreq (p.midFreqIndex);
    const auto lfHz  = lowShelfFreq (p.lfFreqIndex);

    hfFreqSm .setTarget (toLog2Hz (hfHz));
    midFreqSm.setTarget (toLog2Hz (midHz));
    lfFreqSm .setTarget (toLog2Hz (lfHz));

    hfGainSm .setTarget (p.hfGainDb);
    midGainSm.setTarget (p.midGainDb);
    lfGainSm .setTarget (p.lfGainDb);

    inputGainSm  .setTarget (dbToGain (p.inputGainDb));
    outputLevelSm.setTarget (dbToGain (p.outputLevelDb));
    mixSm        .setTarget (std::clamp (p.mixPercent, 0.0f, 100.0f) * 0.01f);

    if (! settingsValid)
    {
        // First block after prepare: start settled rather than gliding up from
        // silence-adjacent defaults.
        hfFreqSm .snap (toLog2Hz (hfHz));
        midFreqSm.snap (toLog2Hz (midHz));
        lfFreqSm .snap (toLog2Hz (lfHz));
        hfGainSm .snap (p.hfGainDb);
        midGainSm.snap (p.midGainDb);
        lfGainSm .snap (p.lfGainDb);
        inputGainSm  .snap (dbToGain (p.inputGainDb));
        outputLevelSm.snap (dbToGain (p.outputLevelDb));
        mixSm        .snap (std::clamp (p.mixPercent, 0.0f, 100.0f) * 0.01f);
        autoGainSm   .snap (1.0f);
    }
}

//==============================================================================
void DspCore::updateCoefficients (int activeChannels) noexcept
{
    EqSettings s;
    s.model     = params.model;
    s.hfFreqHz  = fromLog2Hz (hfFreqSm.tick());
    s.midFreqHz = fromLog2Hz (midFreqSm.tick());
    s.lfFreqHz  = fromLog2Hz (lfFreqSm.tick());
    s.hfGainDb  = hfGainSm.tick();
    s.midGainDb = midGainSm.tick();
    s.lfGainDb  = lfGainSm.tick();
    s.midHiQ    = params.midHiQ;
    s.hpfFreqHz = hpfFreq (params.model, params.hpfIndex);
    s.lpfFreqHz = lpfFreq (params.model, params.lpfIndex);

    // Once the smoothers settle the settings compare exactly equal, so a static
    // EQ costs no trigonometry at all.
    if (settingsValid && sameSettings (s, currentSettings))
        return;

    for (int ch = 0; ch < activeChannels; ++ch)
        networks[(size_t) ch].setSettings (s);

    currentSettings = s;
    settingsValid   = true;

    if (params.autoGain)
        autoGainSm.setTarget ((float) (1.0 / networks[0].broadbandGain()));
    else
        autoGainSm.setTarget (1.0f);
}

//==============================================================================
void DspCore::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const auto activeChannels = std::clamp (numChannels, 0, maxChannels);

    if (activeChannels == 0 || numSamples <= 0)
        return;

    const auto polarity = params.phaseInvert ? -1.0f : 1.0f;

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const auto n = std::min (kSubBlock, numSamples - start);

        updateCoefficients (activeChannels);

        const auto inGain   = inputGainSm.tick();
        const auto outGain  = outputLevelSm.tick() * autoGainSm.tick();
        const auto wet      = mixSm.tick();
        const auto dryLevel = 1.0f - wet;

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            auto* data = channels[ch] + start;
            auto* dry  = dryScratch.data() + (size_t) ch * (size_t) maxBlock;
            auto& net  = networks[(size_t) ch];

            std::memcpy (dry, data, (size_t) n * sizeof (float));

            if (params.eqIn)
            {
                for (int i = 0; i < n; ++i)
                    data[i] = net.processSample (data[i] * inGain * polarity) * outGain;
            }
            else
            {
                // EQ In only bypasses the equaliser section; the gain stages
                // stay in circuit, as on the hardware.
                for (int i = 0; i < n; ++i)
                    data[i] = data[i] * inGain * polarity * outGain;
            }

            for (int i = 0; i < n; ++i)
                data[i] = data[i] * wet + dry[i] * dryLevel;
        }
    }
}

} // namespace frostyeq
