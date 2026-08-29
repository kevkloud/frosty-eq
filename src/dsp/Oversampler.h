#pragma once

#include <array>
#include <cmath>

namespace frostyeq
{

//==============================================================================
/** One 2x half-band FIR stage, cascaded for 4x and 8x.

    Written here rather than taken from JUCE so the DSP core stays free of any
    framework, which is what lets the measurement harness and the tests drive
    the real signal path directly.

    Linear phase, so the delay is exactly (kTaps - 1) / 2 samples at the
    oversampled rate. The tap count is chosen so that the delay stays a whole
    number of samples at every supported factor -- a fractional latency could
    not be reported honestly to the host, and would stop the dry path of the
    Mix control from nulling.
*/
class Halfband2x
{
public:
    // 80 is divisible by 8, so the group delay stays a whole number of samples
    // at every factor. 48 was the first choice and was too short: the round
    // trip drooped 0.54 dB at 20 kHz, outside the +/-0.5 dB the module is
    // specified flat to, and worst-case stopband rejection was only 30 dB.
    // At 80 the droop is below 0.01 dB and rejection better than 70 dB.
    static constexpr int kTaps = 81;
    static constexpr int kGroupDelay = (kTaps - 1) / 2;   // at the oversampled rate

    Halfband2x() { design(); }

    void reset() noexcept
    {
        upLine.fill (0.0f);
        downEven.fill (0.0f);
        downOdd.fill (0.0f);
    }

    /** One input sample becomes two.

        Only the odd branch needs a convolution. A half-band filter's even-index
        taps are all exactly zero apart from the centre one -- that is what makes
        it a half-band -- so the even branch reduces to a scaled tap off the
        delay line, and the loop that used to run over it was multiplying two
        dozen zeros every sample. */
    void upsample (float x, float& first, float& second) noexcept
    {
        for (int i = kHalf - 1; i > 0; --i)
            upLine[(size_t) i] = upLine[(size_t) i - 1];

        upLine[0] = x;

        // Zero-stuffing puts the input on the even phase, so the two output
        // samples are the two polyphase branches of the same delay line.
        first  = 2.0f * centreTap * upLine[(size_t) centreIndex];
        second = 2.0f * dot (odd, upLine, oddCount);
    }

    /** Two input samples become one. */
    float downsample (float first, float second) noexcept
    {
        for (int i = kHalf - 1; i > 0; --i)
        {
            downEven[(size_t) i] = downEven[(size_t) i - 1];
            downOdd [(size_t) i] = downOdd [(size_t) i - 1];
        }

        downEven[0] = first;
        downOdd [0] = second;

        // The odd phase of the decimator lags the even phase by one sample.
        return centreTap * downEven[(size_t) centreIndex]
             + dotOffset (odd, downOdd, oddCount);
    }

private:
    static constexpr int kHalf = kTaps / 2 + 2;

    void design()
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr int    m  = kTaps - 1;

        std::array<double, kTaps> h {};
        double sum = 0.0;

        for (int n = 0; n < kTaps; ++n)
        {
            const auto t = (double) n - (double) m * 0.5;

            // Ideal half-band: sinc cutting at a quarter of the oversampled rate.
            const auto sinc = (std::abs (t) < 1.0e-9) ? 0.5 : std::sin (pi * 0.5 * t) / (pi * t);

            // Blackman-Harris: about -92 dB of sidelobe, which puts the folded
            // images far below anything the saturation itself produces.
            const auto p = 2.0 * pi * (double) n / (double) m;
            const auto w = 0.35875 - 0.48829 * std::cos (p)
                         + 0.14128 * std::cos (2.0 * p) - 0.01168 * std::cos (3.0 * p);

            h[(size_t) n] = sinc * w;
            sum += h[(size_t) n];
        }

        for (auto& v : h)
            v /= sum;                      // unity at DC

        oddCount = 0;

        for (int n = 1; n < kTaps; n += 2)
            odd[(size_t) oddCount++] = (float) h[(size_t) n];

        // The even branch is a single tap. Every even-index coefficient of a
        // half-band filter is exactly zero apart from the centre one -- that is
        // what makes it a half-band -- so the loop that used to run over them
        // was multiplying two dozen zeros per sample. If the window or the
        // cutoff ever changes so that this stops holding, the alias-rejection
        // and flat-response tests will catch it.
        centreIndex = m / 4;
        centreTap   = (float) h[(size_t) (m / 2)];
    }

    static float dot (const std::array<float, kHalf>& taps,
                      const std::array<float, kHalf>& line, int count) noexcept
    {
        float acc = 0.0f;

        for (int i = 0; i < count; ++i)
            acc += taps[(size_t) i] * line[(size_t) i];

        return acc;
    }

    static float dotOffset (const std::array<float, kHalf>& taps,
                            const std::array<float, kHalf>& line, int count) noexcept
    {
        float acc = 0.0f;

        for (int i = 0; i < count; ++i)
            acc += taps[(size_t) i] * line[(size_t) i + 1];

        return acc;
    }

    std::array<float, kHalf> odd {};
    int oddCount = 0;

    // The even branch collapses to one coefficient; see design().
    float centreTap = 0.5f;
    int   centreIndex = 0;

    std::array<float, kHalf> upLine {}, downEven {}, downOdd {};
};

//==============================================================================
/** Round-trip delay of a cascade, in samples at the base rate. Whole by
    construction: each stage contributes kGroupDelay at its own rate, twice.

    Free rather than a member so it can size a constexpr member below -- a
    member function is not usable in a constant expression until its class is
    complete. */
constexpr int oversamplerLatency (int factor) noexcept
{
    const auto stages = factor >= 8 ? 3 : factor >= 4 ? 2 : factor >= 2 ? 1 : 0;
    int latency = 0;

    for (int s = 0; s < stages; ++s)
        latency += (2 * Halfband2x::kGroupDelay) / (1 << (s + 1));

    return latency;
}

//==============================================================================
/** Cascade of half-band stages giving 1x, 2x, 4x or 8x. */
class Oversampler
{
public:
    static constexpr int kMaxStages = 3;
    static constexpr int kMaxFactor = 8;

    void setFactor (int newFactor) noexcept
    {
        stages = newFactor >= 8 ? 3 : newFactor >= 4 ? 2 : newFactor >= 2 ? 1 : 0;
        factor = 1 << stages;
        reset();
    }

    int getFactor() const noexcept { return factor; }

    int getLatencySamples() const noexcept { return latencyForFactor (factor); }

    static constexpr int latencyForFactor (int f) noexcept { return oversamplerLatency (f); }

    /** Worst case across every supported factor. Derived rather than written
        out: stating it by hand got it wrong once -- it read kGroupDelay, which
        is the delay of a single stage, not of the three-stage cascade 8x uses.
        DspCore sizes the dry delay buffer from this, so being short here
        overran it and corrupted the heap. */
    static constexpr int kMaxLatency = oversamplerLatency (kMaxFactor);

    void reset() noexcept
    {
        for (auto& s : up)   s.reset();
        for (auto& s : down) s.reset();
    }

    /** Expand one base-rate sample into `factor` samples.

        Each stage must be fed in time order, so this ping-pongs through a
        scratch buffer rather than expanding in place -- expanding in place
        would either clobber a sample before it was read or, going backwards to
        avoid that, present the stage with its input reversed in time. */
    void upsample (float x, float* out) noexcept
    {
        float scratch[kMaxFactor] { x };
        int count = 1;

        for (int s = 0; s < stages; ++s)
        {
            for (int i = 0; i < count; ++i)
                up[(size_t) s].upsample (scratch[i], out[2 * i], out[2 * i + 1]);

            count *= 2;

            for (int i = 0; i < count; ++i)
                scratch[i] = out[i];
        }

        for (int i = 0; i < count; ++i)
            out[i] = scratch[i];
    }

    /** Collapse `factor` samples back to one. */
    float downsample (float* buffer) noexcept
    {
        int count = factor;

        for (int s = stages - 1; s >= 0; --s)
        {
            count /= 2;

            for (int i = 0; i < count; ++i)
                buffer[i] = down[(size_t) s].downsample (buffer[2 * i], buffer[2 * i + 1]);
        }

        return buffer[0];
    }

private:
    std::array<Halfband2x, kMaxStages> up, down;
    int stages = 0, factor = 1;
};

} // namespace frostyeq
