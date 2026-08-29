#pragma once

#include <cmath>
#include <complex>

namespace frostyeq
{

/** Topology-preserving (TPT) state-variable filter, after Zavalishin.

    Chosen over a direct-form biquad because its state stays meaningful while
    the coefficients move, so frequency selectors can glide between switch
    positions without the coefficient discontinuity that makes a biquad click.

    The unusual part of this class is analyse(): rather than just producing an
    output, it reports the filter's response as an affine function of its input,

        output = d * input + v

    where d is the instantaneous (direct-feedthrough) gain and v depends only on
    stored state. EqNetwork needs that split to resolve the zero-delay feedback
    loop that couples the EQ bands together. See EqNetwork.h.
*/
struct Svf
{
    enum class Output { lowpass, bandpass, highpass };

    //== Coefficients =========================================================
    float g = 0.0f, k = 1.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;

    //== State ================================================================
    float ic1eq = 0.0f, ic2eq = 0.0f;

    void setCutoff (double frequencyHz, double q, double sampleRate) noexcept
    {
        // Prewarp so the digital corner lands on the analogue one.
        const auto gg = std::tan (juce_pi * frequencyHz / sampleRate);
        const auto kk = 1.0 / q;

        g  = (float) gg;
        k  = (float) kk;
        a1 = (float) (1.0 / (1.0 + gg * (gg + kk)));
        a2 = (float) (gg * (double) a1);
        a3 = (float) (gg * (double) a2);
    }

    void reset() noexcept { ic1eq = ic2eq = 0.0f; }

    /** Decompose the requested output into (instantaneous gain, state term). */
    void analyse (Output type, float& d, float& v) const noexcept
    {
        const auto vBpState = a1 * ic1eq - a2 * ic2eq;
        const auto vLpState = ic2eq * (1.0f - a3) + a2 * ic1eq;

        switch (type)
        {
            case Output::bandpass:
                // Normalised so the branch peaks at 1.0 rather than at Q.
                d = k * a2;
                v = k * vBpState;
                break;

            case Output::lowpass:
                d = a3;
                v = vLpState;
                break;

            case Output::highpass:
                // 1 - k*a2 - a3 simplifies to a1.
                d = a1;
                v = -k * vBpState - vLpState;
                break;
        }
    }

    /** Standalone use, where the input is already known and there is no
        feedback loop to resolve first. */
    float process (Output type, float input) noexcept
    {
        float d = 0.0f, v = 0.0f;
        analyse (type, d, v);
        update (input);
        return d * input + v;
    }

    float processLowpass  (float x) noexcept { return process (Output::lowpass,  x); }
    float processHighpass (float x) noexcept { return process (Output::highpass, x); }

    /** Advance the state. Call once per sample, after the input is resolved. */
    void update (float input) noexcept
    {
        const auto v3 = input - ic2eq;
        const auto v1 = a1 * ic1eq + a2 * v3;
        const auto v2 = ic2eq + a2 * ic1eq + a3 * v3;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
    }

    /** Analogue prototype response, evaluated at the same prewarped frequency
        the discrete filter realises. Used for the curve display and auto-gain,
        so the drawn curve cannot drift from what the audio path does. */
    std::complex<double> responseAt (Output type, double frequencyHz, double sampleRate) const noexcept
    {
        if (g <= 0.0f)
            return { 1.0, 0.0 };

        // Normalised analogue frequency for this filter's corner.
        const std::complex<double> s { 0.0, std::tan (juce_pi * frequencyHz / sampleRate) / (double) g };
        const auto denom = s * s + (double) k * s + 1.0;

        switch (type)
        {
            case Output::bandpass: return ((double) k * s) / denom;   // peak 1.0
            case Output::lowpass:  return 1.0 / denom;
            case Output::highpass: return (s * s) / denom;
        }

        return { 1.0, 0.0 };
    }

private:
    static constexpr double juce_pi = 3.14159265358979323846;
};

//==============================================================================
/** One-pole TPT filter, providing both low-pass and high-pass outputs with the
    same affine decomposition Svf uses.

    The shelving branches are first order on purpose. A second-order branch
    rotates phase far enough that its response swings negative just past the
    corner, which gouges a several-dB hole an octave away from a boosted shelf.
    The original's shelves are gentle 6 dB/octave curves and do no such thing.
    (The subtler dip a real inductor-based shelf does produce is a refinement
    for the measurement phase, not a six-dB artefact.)
*/
struct OnePole
{
    enum class Output { lowpass, highpass };

    float g = 0.0f, bigG = 0.0f, state = 0.0f;

    void setCutoff (double frequencyHz, double sampleRate) noexcept
    {
        const auto gg = std::tan (kPi * frequencyHz / sampleRate);
        g    = (float) gg;
        bigG = (float) (gg / (1.0 + gg));
    }

    void reset() noexcept { state = 0.0f; }

    void analyse (Output type, float& d, float& v) const noexcept
    {
        if (type == Output::lowpass)
        {
            d = bigG;
            v = state * (1.0f - bigG);
        }
        else
        {
            d = 1.0f - bigG;
            v = -state * (1.0f - bigG);
        }
    }

    void update (float input) noexcept
    {
        const auto v = (input - state) * bigG;
        state += 2.0f * v;
    }

    float process (Output type, float input) noexcept
    {
        float d = 0.0f, v = 0.0f;
        analyse (type, d, v);
        update (input);
        return d * input + v;
    }

    float processHighpass (float x) noexcept { return process (Output::highpass, x); }

    std::complex<double> responseAt (Output type, double frequencyHz, double sampleRate) const noexcept
    {
        if (g <= 0.0f)
            return { 1.0, 0.0 };

        const std::complex<double> s { 0.0, std::tan (kPi * frequencyHz / sampleRate) / (double) g };

        return type == Output::lowpass ? 1.0 / (s + 1.0)
                                       : s   / (s + 1.0);
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
};

//==============================================================================
/** Shelving branch: a first-order pole with a second-order one blended
    alongside it.

    A purely first-order shelf is too shallow. Response plots of an assembled
    board show the low shelf falling at almost exactly 7 dB/octave through its
    transition, repeatably across the 35, 60 and 110 Hz positions; a first-order
    branch gives about 4.9 measured the same way. A purely second-order branch
    is far too aggressive -- it carved a 6.5 dB hole an octave from a 12 dB
    boost, which is why these were first order to begin with. Half of each lands
    the slope near 6.7 and, as a side effect, brings the -3 dB corners within
    about 6 % of their marked frequencies, where first order alone ran 13 to
    37 % high.

    Both parts are affine in the input, so the blend is too, and the network's
    zero-delay solve is unaffected.

    Known difference. A boosted shelf on the measured board dips roughly 6 % of
    its boost below flat past the corner -- about 1.2 dB on a 20 dB boost,
    around ten times the corner frequency -- and this reaches 2.5 %. The dip
    needs the branch's real part to go negative, and for this blend that part
    behaves as (1 - 2m)/x^2 well above the corner, so it can only just reach
    zero at m = 0.5 and cannot go usefully negative without the slope running
    away with it. Producing it properly needs a zero in the branch, which the
    original's LC network has and this does not yet. At the boosts anyone
    actually uses the difference is a fraction of a dB.
*/
class ShelfBranch
{
public:
    enum class Type { low, high };

    /** Half and half, solved against the measured slope; `measure shelf`
        reports what this produces. */
    static constexpr float  kSecondOrderMix = 0.50f;
    static constexpr double kSecondOrderQ   = 0.70;

    void setCutoff (double frequencyHz, double sampleRate) noexcept
    {
        first .setCutoff (frequencyHz, sampleRate);
        second.setCutoff (frequencyHz, kSecondOrderQ, sampleRate);
    }

    void reset() noexcept { first.reset(); second.reset(); }

    void analyse (Type type, float& d, float& v) const noexcept
    {
        float d1 = 0.0f, v1 = 0.0f, d2 = 0.0f, v2 = 0.0f;

        first .analyse (type == Type::low ? OnePole::Output::lowpass : OnePole::Output::highpass, d1, v1);
        second.analyse (type == Type::low ? Svf::Output::lowpass     : Svf::Output::highpass,     d2, v2);

        d = (1.0f - kSecondOrderMix) * d1 + kSecondOrderMix * d2;
        v = (1.0f - kSecondOrderMix) * v1 + kSecondOrderMix * v2;
    }

    void update (float input) noexcept
    {
        first .update (input);
        second.update (input);
    }

    std::complex<double> responseAt (Type type, double frequencyHz, double sampleRate) const noexcept
    {
        const auto a = first .responseAt (type == Type::low ? OnePole::Output::lowpass : OnePole::Output::highpass,
                                          frequencyHz, sampleRate);
        const auto b = second.responseAt (type == Type::low ? Svf::Output::lowpass : Svf::Output::highpass,
                                          frequencyHz, sampleRate);

        return (1.0 - (double) kSecondOrderMix) * a + (double) kSecondOrderMix * b;
    }

private:
    OnePole first;
    Svf     second;
};

} // namespace frostyeq
