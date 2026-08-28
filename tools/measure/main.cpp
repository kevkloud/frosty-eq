// Offline measurement harness.
//
// Links the DSP core directly -- no plugin host, no GUI, no JUCE -- so a sweep
// runs in milliseconds and is trivial to debug. This is the ground truth the
// EQ is developed against; see docs/plan.md, Part 7.
//
//   measure curve  [--model 1073|1084] [--lf i:dB] [--mid i:dB] [--hf i:dB]
//                  [--hpf i] [--lpf i] [--hiq]
//   measure bands  -- band-interaction table
//   measure q      -- realised bell width vs gain

#include "dsp/EqNetwork.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace frostyeq;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kPi = 3.14159265358979323846;   // kPi is not portable to MSVC

    EqNetwork make (const EqSettings& s)
    {
        EqNetwork n;
        n.prepare (kSampleRate);
        n.setSettings (s);
        return n;
    }

    /** Parse "index:gainDb", e.g. "2:-6.5". */
    bool parseBand (const char* arg, int& index, float& gainDb)
    {
        const auto* colon = std::strchr (arg, ':');

        if (colon == nullptr)
            return false;

        index  = std::atoi (std::string (arg, colon).c_str());
        gainDb = (float) std::atof (colon + 1);
        return true;
    }

    void printCurve (const EqNetwork& net)
    {
        std::printf ("%9s  %9s  %9s\n", "Hz", "dB", "phase deg");

        for (int i = 0; i < 61; ++i)
        {
            const auto hz = 20.0 * std::pow (1000.0, (double) i / 60.0);
            const auto h  = net.responseAt (hz);

            std::printf ("%9.1f  %+9.2f  %+9.1f\n",
                         hz, net.magnitudeDbAt (hz), std::arg (h) * 180.0 / kPi);
        }

        std::printf ("\nbroadband gain: %+.2f dB\n", 20.0 * std::log10 (net.broadbandGain()));
    }

    void printInteraction()
    {
        std::printf ("Band interaction: LF 220 Hz +12 dB against mid 360 Hz +12 dB.\n"
                     "A cascade of independent biquads would sum the two dB columns.\n\n");

        EqSettings a; a.lfFreqHz  = 220.0f; a.lfGainDb  = 12.0f;
        EqSettings b; b.midFreqHz = 360.0f; b.midGainDb = 12.0f;
        EqSettings c = a; c.midFreqHz = 360.0f; c.midGainDb = 12.0f;

        auto A = make (a), B = make (b), C = make (c);

        std::printf ("%7s %8s %8s %9s %10s %8s\n", "Hz", "LF", "mid", "naive sum", "combined", "delta");

        for (double hz : { 50.0, 110.0, 220.0, 360.0, 700.0, 1600.0, 5000.0 })
        {
            const auto da = A.magnitudeDbAt (hz), db = B.magnitudeDbAt (hz), dc = C.magnitudeDbAt (hz);
            std::printf ("%7.0f %+8.2f %+8.2f %+9.2f %+10.2f %+8.2f\n", hz, da, db, da + db, dc, dc - (da + db));
        }
    }

    void printQ()
    {
        std::printf ("Proportional Q: realised mid-bell width at 1.6 kHz, measured 3 dB\n"
                     "below the peak. Nothing models this explicitly -- it falls out of\n"
                     "the shared feedback structure.\n\n%8s %10s\n", "gain dB", "octaves");

        for (double gain : { 3.0, 6.0, 9.0, 12.0, 15.0, 18.0 })
        {
            EqSettings s; s.midFreqHz = 1600.0f; s.midGainDb = (float) gain;
            auto n = make (s);

            const auto threshold = n.magnitudeDbAt (1600.0) - 3.0;

            const auto edge = [&] (double dir)
            {
                double hz = 1600.0;

                for (int i = 0; i < 6000; ++i)
                {
                    hz *= std::pow (2.0, dir * 0.001);

                    if (hz < 10.0 || hz > 22000.0 || n.magnitudeDbAt (hz) < threshold)
                        break;
                }

                return hz;
            };

            std::printf ("%+8.1f %10.2f\n", gain, std::log2 (edge (1.0) / edge (-1.0)));
        }
    }
}

//==============================================================================
int main (int argc, char** argv)
{
    const std::string command = argc > 1 ? argv[1] : "curve";

    if (command == "bands") { printInteraction(); return 0; }
    if (command == "q")     { printQ();           return 0; }

    EqSettings s;
    int index = 0;
    float gainDb = 0.0f;

    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto* next = (i + 1 < argc) ? argv[i + 1] : nullptr;

        if      (a == "--hiq")                    s.midHiQ = true;
        else if (a == "--model" && next)          s.model  = (std::string (next) == "1084") ? Model::m1084 : Model::m1073;
        else if (a == "--lf"  && next && parseBand (next, index, gainDb)) { s.lfFreqHz  = lowShelfFreq (index);        s.lfGainDb  = gainDb; }
        else if (a == "--mid" && next && parseBand (next, index, gainDb)) { s.midFreqHz = midFreq (index);             s.midGainDb = gainDb; }
        else if (a == "--hf"  && next && parseBand (next, index, gainDb)) { s.hfFreqHz  = highShelfFreq (s.model, index); s.hfGainDb = gainDb; }
        else if (a == "--hpf" && next)            s.hpfFreqHz = hpfFreq (s.model, std::atoi (next));
        else if (a == "--lpf" && next)            s.lpfFreqHz = lpfFreq (s.model, std::atoi (next));
    }

    auto net = make (s);
    printCurve (net);
    return 0;
}
