#pragma once

#include "EqNetwork.h"
#include <array>
#include <vector>

namespace frostyeq
{

/** One-pole parameter smoother.

    Snaps to the target once it is within epsilon, so a settled parameter
    compares exactly equal and the coefficient recomputation can be skipped.
*/
class Smoother
{
public:
    void prepare (double controlRateHz, double timeMs) noexcept
    {
        const auto tau = std::max (timeMs, 0.01) * 0.001;
        coeff = (float) (1.0 - std::exp (-1.0 / (std::max (controlRateHz, 1.0) * tau)));
    }

    void snap (float v) noexcept        { current = target = v; }
    void setTarget (float t) noexcept   { target = t; }
    float value() const noexcept        { return current; }

    float tick() noexcept
    {
        current += coeff * (target - current);

        if (std::abs (target - current) < 1.0e-5f)
            current = target;

        return current;
    }

private:
    float coeff = 1.0f, current = 0.0f, target = 0.0f;
};

//==============================================================================
/** Everything the plugin does to audio, with no dependency on JUCE's plugin
    layer or on a host. Takes plain values and raw buffers, so the measurement
    harness and the unit tests can drive the real signal path directly.

    Frequency and gain parameters are smoothed and the filter coefficients are
    recomputed at control rate (once per sub-block) rather than per sample. The
    TPT filters keep their state meaningful across a coefficient change, so a
    stepped selector glides between switch positions instead of clicking. If any
    residual artefact ever shows up, the fallback is to crossfade between two
    network instances over ~10 ms.
*/
class DspCore
{
public:
    struct Params
    {
        Model model = Model::m1073;

        int hfFreqIndex  = 1;   // 10k / 12k / 16k  (1084 only; 1073 forces 12k)
        int midFreqIndex = 2;
        int lfFreqIndex  = 1;
        int hpfIndex     = 0;   // 0 == off
        int lpfIndex     = 0;   // 0 == off

        float hfGainDb  = 0.0f;
        float midGainDb = 0.0f;
        float lfGainDb  = 0.0f;
        bool  midHiQ    = false;

        float inputGainDb   = 0.0f;
        float outputLevelDb = 0.0f;
        float mixPercent    = 100.0f;

        bool eqIn        = true;
        bool phaseInvert = false;
        bool autoGain    = false;
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    /** Called once per block, before process(). Cheap: stores targets only. */
    void setParams (const Params&) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** The network whose coefficients the curve display should draw. All
        channels share coefficients, so channel 0 is representative. */
    const EqNetwork& displayNetwork() const noexcept { return networks[0]; }

    static constexpr int kSubBlock = 32;

private:
    void updateCoefficients (int activeChannels) noexcept;

    double sampleRate = 44100.0;

    std::array<EqNetwork, 2> networks;

    Smoother hfFreqSm, midFreqSm, lfFreqSm;     // smoothed in log2(Hz)
    Smoother hfGainSm, midGainSm, lfGainSm;
    Smoother inputGainSm, outputLevelSm, mixSm, autoGainSm;

    Params   params;
    EqSettings currentSettings;
    bool     settingsValid = false;

    std::vector<float> dryScratch;   // preallocated; interleaved by channel
    int maxBlock = 0, maxChannels = 0;
};

} // namespace frostyeq
