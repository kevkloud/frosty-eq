#include "dsp/EqNetwork.h"
#include "dsp/DspCore.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using namespace frostyeq;

namespace
{
    int failures = 0;
    constexpr double kPi = 3.14159265358979323846;   // kPi is not portable to MSVC
    constexpr double kSampleRate = 48000.0;

    void check (bool ok, const std::string& what)
    {
        if (! ok) { std::cerr << "FAIL: " << what << '\n'; ++failures; }
    }

    void checkClose (double actual, double expected, double tol, const std::string& what)
    {
        if (! (std::abs (actual - expected) <= tol))
        {
            std::cerr << "FAIL: " << what << " -- expected " << expected
                      << " +/- " << tol << ", got " << actual << '\n';
            ++failures;
        }
    }

    EqNetwork makeNetwork (const EqSettings& s)
    {
        EqNetwork n;
        n.prepare (kSampleRate);
        n.setSettings (s);
        return n;
    }

    /** Impulse response of the actual sample-by-sample audio path. */
    std::vector<float> impulseResponse (EqNetwork& net, int length)
    {
        net.reset();
        std::vector<float> h ((size_t) length);
        h[0] = net.processSample (1.0f);

        for (int i = 1; i < length; ++i)
            h[(size_t) i] = net.processSample (0.0f);

        return h;
    }

    /** Single-frequency DFT by incremental rotation. */
    std::complex<double> dftAt (const std::vector<float>& h, double hz)
    {
        const auto w = -2.0 * kPi * hz / kSampleRate;
        const std::complex<double> step { std::cos (w), std::sin (w) };
        std::complex<double> rot { 1.0, 0.0 }, acc { 0.0, 0.0 };

        for (float v : h) { acc += (double) v * rot; rot *= step; }

        return acc;
    }

    double toDb (double magnitude)
    {
        return 20.0 * std::log10 (std::max (magnitude, 1.0e-12));
    }


    //==========================================================================
    // Phase 4 helpers: drive the whole chain, saturation included.
    //==========================================================================

    constexpr int kChainSize = 1 << 15;

    double snapToBin (double hz)
    {
        const auto bin = std::max (1.0, std::round (hz * (double) kChainSize / kSampleRate));
        return bin * kSampleRate / (double) kChainSize;
    }

    std::vector<float> renderChain (double binHz, double levelDb, float inputGainDb,
                                    int oversampling, DspCore::Params extra = {})
    {
        DspCore core;
        core.prepare (kSampleRate, 512, 1, oversampling);

        auto p = extra;
        p.inputGainDb  = inputGainDb;
        p.oversampling = oversampling;
        core.setParams (p);

        const auto amplitude = std::pow (10.0, levelDb / 20.0);
        std::vector<float> buffer ((size_t) kChainSize);
        float* channels[1] { buffer.data() };

        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < kChainSize; ++i)
                buffer[(size_t) i] = (float) (amplitude
                    * std::sin (2.0 * kPi * binHz * (double) i / kSampleRate));

            core.process (channels, 1, kChainSize);
        }

        return buffer;
    }

    double magnitudeAt (const std::vector<float>& x, double hz)
    {
        const auto w = -2.0 * kPi * hz / kSampleRate;
        const std::complex<double> step { std::cos (w), std::sin (w) };
        std::complex<double> rot { 1.0, 0.0 }, acc { 0.0, 0.0 };

        for (float v : x) { acc += (double) v * rot; rot *= step; }

        return 2.0 * std::abs (acc) / (double) x.size();
    }

    double thdPercent (double hz, double levelDb, float inputGainDb, int oversampling)
    {
        const auto f0 = snapToBin (hz);
        const auto x  = renderChain (f0, levelDb, inputGainDb, oversampling);

        const auto fundamental = magnitudeAt (x, f0);
        double power = 0.0;

        for (int n = 2; n <= 8 && f0 * n < kSampleRate * 0.5; ++n)
        {
            const auto m = magnitudeAt (x, f0 * n);
            power += m * m;
        }

        return fundamental > 0.0 ? 100.0 * std::sqrt (power) / fundamental : 0.0;
    }

    /** Width in octaves at a fixed number of dB below the peak -- the classical
        bandwidth definition. Measuring at a *fraction* of the peak instead is
        misleading: that metric widens with gain even for a constant-Q bell. */
    double bellWidthOctaves (const EqNetwork& net, double centreHz, double dbBelowPeak)
    {
        const auto threshold = net.magnitudeDbAt (centreHz) - dbBelowPeak;

        const auto edge = [&] (double direction)
        {
            double hz = centreHz;

            for (int i = 0; i < 4000; ++i)
            {
                hz *= std::pow (2.0, direction * 0.002);   // 1/500 octave steps

                if (hz < 10.0 || hz > 22000.0)
                    break;

                if (net.magnitudeDbAt (hz) < threshold)
                    return hz;
            }

            return hz;
        };

        return std::log2 (edge (1.0) / edge (-1.0));
    }
}

//==============================================================================
int main()
{
    //== 1. Unity when flat ===================================================
    // The most important invariant of the shared-feedback structure: with every
    // band at 0 dB the numerator and denominator sums cancel exactly.
    {
        EqSettings s;
        auto net = makeNetwork (s);

        for (double hz : { 20.0, 100.0, 1000.0, 5000.0, 15000.0, 20000.0 })
            checkClose (net.magnitudeDbAt (hz), 0.0, 1.0e-6,
                        "flat EQ must be unity at " + std::to_string ((int) hz) + " Hz");

        auto h = impulseResponse (net, 4096);
        checkClose (h[0], 1.0f, 1.0e-6, "flat EQ impulse response should be a unit impulse");

        double tail = 0.0;
        for (size_t i = 1; i < h.size(); ++i) tail += std::abs ((double) h[i]);
        checkClose (tail, 0.0, 1.0e-5, "flat EQ should have no impulse tail");
    }

    //== 2. Single-band gain lands on target ==================================
    {
        EqSettings s;
        s.midFreqHz = 1600.0f;
        s.midGainDb = 12.0f;
        auto net = makeNetwork (s);

        // Not exactly 12.00: the other branches still load the shared feedback
        // path even at unity gain, which shifts the realised peak by a fraction
        // of a dB. That is the topology behaving correctly, not slop.
        checkClose (net.magnitudeDbAt (1600.0), 12.0, 0.15,
                    "mid bell should hit its nominal boost at centre");

        s.midGainDb = -12.0f;
        net.setSettings (s);
        checkClose (net.magnitudeDbAt (1600.0), -12.0, 0.15,
                    "mid bell should be symmetric in cut");
    }
    {
        EqSettings s;
        s.lfFreqHz = 60.0f;
        s.lfGainDb = 10.0f;
        auto net = makeNetwork (s);

        checkClose (net.magnitudeDbAt (2.0), 10.0, 0.4,
                    "low shelf should approach its nominal boost well below the corner");
    }

    //== 3. The analytic response matches the audio path ======================
    // responseAt() drives the curve display and auto-gain. If it disagrees with
    // what processSample() actually does, the plugin lies to the user.
    {
        EqSettings s;
        s.model     = Model::m1084;
        s.lfFreqHz  = 110.0f; s.lfGainDb  = 8.0f;
        s.midFreqHz = 3200.0f; s.midGainDb = -10.0f; s.midHiQ = true;
        s.hfFreqHz  = 16000.0f; s.hfGainDb = 6.0f;
        s.hpfFreqHz = 45.0f;
        s.lpfFreqHz = 14000.0f;

        auto net = makeNetwork (s);
        auto h   = impulseResponse (net, 1 << 16);

        for (double hz : { 30.0, 60.0, 110.0, 400.0, 1000.0, 3200.0, 8000.0, 14000.0, 18000.0 })
        {
            const auto measured = toDb (std::abs (dftAt (h, hz)));
            const auto analytic = net.magnitudeDbAt (hz);

            checkClose (measured, analytic, 0.05,
                        "analytic response must match the measured impulse response at "
                            + std::to_string ((int) hz) + " Hz");
        }
    }

    //== 4. Bands interact -- the defining property ===========================
    // Cascaded independent biquads would sum in dB. A shared feedback path
    // yields strictly less than that sum wherever two boosted bands overlap.
    // Probe at the low shelf corner and the mid centre, which is where the
    // overlap actually is -- far apart, the bands barely see each other.
    {
        EqSettings lfOnly;  lfOnly.lfFreqHz   = 220.0f; lfOnly.lfGainDb   = 12.0f;
        EqSettings midOnly; midOnly.midFreqHz = 360.0f; midOnly.midGainDb = 12.0f;

        EqSettings both = lfOnly;
        both.midFreqHz = 360.0f;
        both.midGainDb = 12.0f;

        auto a = makeNetwork (lfOnly);
        auto b = makeNetwork (midOnly);
        auto c = makeNetwork (both);

        for (double hz : { 220.0, 360.0 })
        {
            const auto sumOfParts = a.magnitudeDbAt (hz) + b.magnitudeDbAt (hz);
            const auto combined   = c.magnitudeDbAt (hz);

            check (combined < sumOfParts - 2.0,
                   "two overlapping boosted bands must produce clearly less than the sum "
                   "of their curves at " + std::to_string ((int) hz) + " Hz (got "
                   + std::to_string (combined) + " dB vs a naive sum of "
                   + std::to_string (sumOfParts) + " dB)");

            check (combined > 0.0, "two boosted bands should still boost");
        }
    }

    //== 4b. A boosted shelf must not dip ====================================
    // Regression guard. Second-order shelving branches rotate phase far enough
    // that the branch response goes negative past the corner, carving a ~6 dB
    // hole an octave away from a boost. First-order branches do not.
    {
        for (bool high : { false, true })
        {
            EqSettings s;
            (high ? s.hfGainDb : s.lfGainDb) = 12.0f;
            (high ? s.hfFreqHz : s.lfFreqHz) = high ? 12000.0f : 110.0f;

            auto net = makeNetwork (s);

            double worst = 0.0;
            double worstHz = 0.0;

            for (int i = 0; i < 400; ++i)
            {
                const auto hz = 20.0 * std::pow (1000.0, (double) i / 399.0);
                const auto db = net.magnitudeDbAt (hz);

                if (db < worst) { worst = db; worstHz = hz; }
            }

            check (worst > -0.5,
                   std::string (high ? "high" : "low") + " shelf boost must not dip below unity "
                   "anywhere (worst " + std::to_string (worst) + " dB at "
                   + std::to_string ((int) worstHz) + " Hz)");
        }
    }

    //== 5. Proportional Q ====================================================
    // The bell narrows as gain rises, and nothing in the code models that
    // explicitly -- it falls out of the shared feedback structure.
    {
        EqSettings s; s.midFreqHz = 1600.0f;

        s.midGainDb = 6.0f;
        auto gentle = makeNetwork (s);
        const auto wideOct = bellWidthOctaves (gentle, 1600.0, 3.0);

        s.midGainDb = 18.0f;
        auto steep = makeNetwork (s);
        const auto narrowOct = bellWidthOctaves (steep, 1600.0, 3.0);

        check (narrowOct < wideOct - 0.5,
               "bell should narrow with increasing gain (proportional Q): "
               + std::to_string (wideOct) + " oct at +6 dB vs "
               + std::to_string (narrowOct) + " oct at +18 dB");
    }

    //== 6. High-pass slope is 18 dB/octave ===================================
    {
        EqSettings s; s.hpfFreqHz = 300.0f;
        auto net = makeNetwork (s);

        const auto perOctave = net.magnitudeDbAt (75.0) - net.magnitudeDbAt (37.5);
        checkClose (perOctave, 18.0, 2.0, "high-pass should fall about 18 dB per octave");

        check (net.magnitudeDbAt (20000.0) > -0.1, "high-pass must not affect the top end");
    }

    //== 7. Stability under extremes ==========================================
    {
        for (int model = 0; model < 2; ++model)
            for (int f = 0; f < 6; ++f)
            {
                EqSettings s;
                s.model     = (Model) model;
                s.midFreqHz = kMidFreqs[(size_t) f];
                s.midGainDb = 18.0f;
                s.lfGainDb  = 16.0f;
                s.hfGainDb  = 16.0f;
                s.midHiQ    = true;
                s.hpfFreqHz = 45.0f;
                s.lpfFreqHz = model == 1 ? 6000.0f : 0.0f;

                auto net = makeNetwork (s);
                auto h   = impulseResponse (net, 8192);

                bool finite = true, decayed = true;
                for (float v : h) finite = finite && std::isfinite (v);
                for (size_t i = 6000; i < h.size(); ++i) decayed = decayed && std::abs (h[i]) < 0.01f;

                check (finite,  "impulse response must stay finite at extreme settings");
                check (decayed, "impulse response must decay at extreme settings");
            }
    }

    //== 8. DspCore plumbing ==================================================
    {
        constexpr int n = 4096;
        std::vector<float> left ((size_t) n), right ((size_t) n);
        float* channels[2] { left.data(), right.data() };

        const auto fill = [&]
        {
            for (int i = 0; i < n; ++i)
            {
                const auto v = 0.25f * std::sin (2.0f * (float) kPi * 220.0f * (float) i / 48000.0f);
                left[(size_t) i] = right[(size_t) i] = v;
            }
        };

        // Phase invert should negate the signal. Checked well below the knee,
        // where the colour stage is linear -- higher up the saturation makes
        // this a comparison of two slightly different waveforms rather than a
        // test of polarity.
        {
            DspCore core;
            core.prepare (kSampleRate, n, 1);
            DspCore::Params p;
            p.eqIn = false;
            p.phaseInvert = true;
            p.oversampling = 1;          // keep it latency-free for a direct comparison
            core.setParams (p);

            constexpr float amplitude = 1.0e-4f;

            for (int i = 0; i < n; ++i)
                left[(size_t) i] = right[(size_t) i] = amplitude
                    * std::sin (2.0f * (float) kPi * 220.0f * (float) i / 48000.0f);

            std::vector<float> reference = left;
            core.process (channels, 2, n);

            // Correlation rather than a sample-by-sample match: the
            // transformer's low-frequency rolloff shifts phase by a degree or
            // so at 220 Hz, which is correct behaviour and would defeat a
            // direct comparison without saying anything about polarity.
            double dot = 0.0, energyA = 0.0, energyB = 0.0;

            for (int i = 512; i < n; ++i)
            {
                const double a = left[(size_t) i], b = reference[(size_t) i];
                dot += a * b;
                energyA += a * a;
                energyB += b * b;
            }

            const auto correlation = dot / std::sqrt (energyA * energyB);

            checkClose (correlation, -1.0, 0.01,
                        "phase invert should negate the signal");
        }

        // Mix at 0% must be bit-identical to the input, whatever the EQ does.
        {
            DspCore core;
            core.prepare (kSampleRate, n, 2);
            DspCore::Params p;
            p.mixPercent = 0.0f; p.midGainDb = 18.0f; p.lfGainDb = -16.0f; p.hpfIndex = 3;
            core.setParams (p);
            fill();
            std::vector<float> reference = left;
            core.process (channels, 2, n);

            bool identical = true;
            for (int i = 0; i < n; ++i)
                identical = identical && std::abs (left[(size_t) i] - reference[(size_t) i]) < 1.0e-6f;

            check (identical, "mix at 0% must pass the dry signal through untouched");
        }

        // Auto-gain should pull a heavily boosted setting back toward unity.
        {
            const auto rms = [&] (bool autoGain)
            {
                DspCore core;
                core.prepare (kSampleRate, n, 2);
                DspCore::Params p;
                p.lfGainDb = 16.0f; p.midGainDb = 12.0f; p.autoGain = autoGain;
                core.setParams (p);
                fill();
                core.process (channels, 2, n);

                double acc = 0.0;
                for (int i = n / 2; i < n; ++i) acc += (double) left[(size_t) i] * left[(size_t) i];
                return std::sqrt (acc / (double) (n / 2));
            };

            const auto boosted = rms (false);
            const auto levelled = rms (true);

            check (levelled < boosted,
                   "auto-gain should reduce level relative to the uncompensated boost");
        }

        // Nothing may allocate or blow up when the model is switched mid-stream.
        {
            DspCore core;
            core.prepare (kSampleRate, 512, 2);
            std::vector<float> l (512), r (512);
            float* ch[2] { l.data(), r.data() };

            bool finite = true;
            for (int block = 0; block < 200; ++block)
            {
                DspCore::Params p;
                p.model        = (block % 2) ? Model::m1084 : Model::m1073;
                p.midFreqIndex = block % 6;
                p.hpfIndex     = block % 5;
                p.midGainDb    = ((block % 7) - 3) * 6.0f;
                core.setParams (p);

                for (int i = 0; i < 512; ++i)
                    l[(size_t) i] = r[(size_t) i] = 0.3f * std::sin (0.05f * (float) (block * 512 + i));

                core.process (ch, 2, 512);

                for (int i = 0; i < 512; ++i)
                    finite = finite && std::isfinite (l[(size_t) i]) && std::abs (l[(size_t) i]) < 50.0f;
            }

            check (finite, "rapid model and selector changes must stay finite and bounded");
        }
    }


    //== 9. The colour stage leaves the response alone when it is not pushed ==
    // The transformer emphasises into a flux-like domain, saturates there, and
    // de-emphasises by the exact inverse. If that pair does not cancel, the
    // plugin tilts the spectrum even at rest, which would be a far worse fault
    // than any amount of distortion.
    {
        for (double hz : { 30.0, 60.0, 200.0, 1000.0, 5000.0, 12000.0 })
        {
            const auto f0 = snapToBin (hz);
            const auto x  = renderChain (f0, -60.0, 0.0f, 2);

            const auto db = 20.0 * std::log10 (magnitudeAt (x, f0) / std::pow (10.0, -60.0 / 20.0));

            checkClose (db, 0.0, 0.6,
                        "chain should be within a fraction of a dB of flat at low level at "
                            + std::to_string ((int) hz) + " Hz");
        }
    }

    //== 10. Distortion falls with level, and keeps falling =================
    // Guards a real defect this code had: log(cosh(u)) computed the obvious
    // way loses all precision for small u, and because ADAA divides the
    // difference of two antiderivatives by a small dx, that error was
    // amplified into a constant noise floor -- 66 % "THD" on a quiet signal.
    {
        double previous = 100.0;

        for (double level : { 0.0, -12.0, -24.0, -36.0, -48.0, -60.0 })
        {
            const auto thd = thdPercent (1000.0, level, 0.0f, 2);

            check (thd < previous,
                   "THD must fall as level falls (at " + std::to_string ((int) level)
                       + " dBFS got " + std::to_string (thd) + " %, previously "
                       + std::to_string (previous) + " %)");

            previous = thd;
        }

        check (previous < 0.01,
               "a -60 dBFS signal should be essentially undistorted, got "
                   + std::to_string (previous) + " %");
    }

    //== 11. Low frequencies distort far harder -- the transformer's signature =
    // Core flux is the integral of applied voltage, so at a fixed level the
    // bottom of the band drives the core far harder than the top. Marinair
    // measured 0.1 % at 40 Hz against 0.01 % at 1 kHz for the line transformer
    // in these units. A memoryless waveshaper distorts every frequency alike
    // and cannot produce this at all.
    {
        const auto low  = thdPercent (40.0,   0.0, 0.0f, 2);
        const auto mid  = thdPercent (1000.0, 0.0, 0.0f, 2);

        check (low > mid * 3.0,
               "40 Hz should distort several times harder than 1 kHz at the same level ("
                   + std::to_string (low) + " % against " + std::to_string (mid) + " %)");

        check (mid < 2.0, "1 kHz at full scale should stay civil, got " + std::to_string (mid) + " %");
    }

    //== 12. Second harmonic dominates through the midrange =================
    // Single-ended class-A stages are asymmetric, and the second harmonic is
    // an octave, so it stays consonant with whatever produced it.
    {
        const auto f0 = snapToBin (1000.0);
        const auto x  = renderChain (f0, 0.0, 0.0f, 2);

        const auto h2 = magnitudeAt (x, f0 * 2.0);
        const auto h3 = magnitudeAt (x, f0 * 3.0);

        check (h2 > h3 * 2.0,
               "second harmonic should dominate the third in the midrange ("
                   + std::to_string (20.0 * std::log10 (h2 / h3)) + " dB apart)");
    }

    //== 13. Aliasing stays buried ===========================================
    {
        const auto f0 = snapToBin (7500.0);
        const auto x  = renderChain (f0, -3.0, 18.0f, 2);

        const auto fundamental = magnitudeAt (x, f0);
        double worst = 0.0;

        for (int bin = 4; bin < kChainSize / 2; bin += 7)
        {
            const auto f = bin * kSampleRate / (double) kChainSize;
            bool harmonic = false;

            for (int n = 1; n <= 12; ++n)
                if (std::abs (f - n * f0) < 4.0 * kSampleRate / (double) kChainSize)
                    harmonic = true;

            if (! harmonic)
                worst = std::max (worst, magnitudeAt (x, f));
        }

        const auto rejection = 20.0 * std::log10 (worst / fundamental);
        check (rejection < -80.0,
               "folded images should sit below -80 dB even when driven hard, got "
                   + std::to_string (rejection) + " dB");
    }

    //== 14. The dry path is delayed to match the oversampling filters =======
    // Otherwise a partial Mix combs and a full bypass fails to null.
    {
        for (int factor : { 1, 2, 4 })
        {
            DspCore core;
            core.prepare (kSampleRate, 512, 1, factor);

            DspCore::Params p;
            p.mixPercent   = 0.0f;      // dry only
            p.oversampling = factor;
            p.midGainDb    = 18.0f;     // the wet path is doing plenty; it must not leak
            p.inputGainDb  = 12.0f;
            core.setParams (p);

            constexpr int n = 2048;
            std::vector<float> buffer ((size_t) n, 0.0f);
            buffer[64] = 1.0f;          // a lone impulse is easy to locate
            float* channels[1] { buffer.data() };

            core.process (channels, 1, n);

            int peak = 0;
            for (int i = 0; i < n; ++i)
                if (std::abs (buffer[(size_t) i]) > std::abs (buffer[(size_t) peak]))
                    peak = i;

            check (peak - 64 == core.getLatencySamples(),
                   "at 0 % mix the output should be the input delayed by exactly the "
                   "reported latency (factor " + std::to_string (factor) + ": delay "
                       + std::to_string (peak - 64) + ", reported "
                       + std::to_string (core.getLatencySamples()) + ")");

            checkClose (buffer[(size_t) peak], 1.0f, 1.0e-3,
                        "the dry path must pass at unity");
        }
    }

    //== 15. Nothing blows up when driven into the weeds =====================
    {
        DspCore core;
        core.prepare (kSampleRate, 512, 2, 4);

        DspCore::Params p;
        p.inputGainDb = 24.0f;
        p.midGainDb   = 18.0f;
        p.lfGainDb    = 16.0f;
        p.hfGainDb    = 16.0f;
        p.oversampling = 4;
        core.setParams (p);

        std::vector<float> l (512), r (512);
        float* ch[2] { l.data(), r.data() };
        bool sane = true;

        for (int block = 0; block < 100; ++block)
        {
            for (int i = 0; i < 512; ++i)
                l[(size_t) i] = r[(size_t) i] = (float) std::sin (0.03 * (block * 512 + i));

            core.process (ch, 2, 512);

            for (int i = 0; i < 512; ++i)
                sane = sane && std::isfinite (l[(size_t) i]) && std::abs (l[(size_t) i]) < 100.0f;
        }

        check (sane, "the chain must stay finite and bounded when driven hard");
    }

    if (failures == 0)
        std::cout << "All DSP tests passed.\n";

    return failures == 0 ? 0 : 1;
}
