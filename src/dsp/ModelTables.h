#pragma once

#include <array>
#include <cmath>

namespace frostyeq
{

/** The two console modules we model. The 1084 is very nearly a superset of the
    1073: same low-shelf and mid-bell frequencies, more high-shelf options, a
    switchable narrow Q on the mid band, a different high-pass table, and an
    additional low-pass filter.
*/
enum class Model { m1073 = 0, m1084 = 1 };

inline constexpr int kNumModels = 2;

//==============================================================================
// Frequency tables, in Hz.
//
// These are the nominal published switch positions. They are the *labels* and
// the starting point for the network model; the realised curve shapes (Q, shelf
// slope, the high-pass filter's resonant bump) come out of the LC network in
// EqNetwork, not from these numbers.
//
// Note the tables are indexed *positionally*. A frequency selector's automation
// value is the switch position, not the frequency, so switching models moves
// you to the corresponding detent on the other unit's table. That mapping is
// deterministic and round-trip-safe, which is what we want for saved sessions.
//==============================================================================

// High shelf. The 1073 has a single fixed 12 kHz shelf; the selector is present
// but inert in 1073 mode, and the editor greys it out.
inline constexpr std::array<float, 3> kHighShelfFreqs1084 { 10000.0f, 12000.0f, 16000.0f };
inline constexpr float kHighShelfFreq1073 = 12000.0f;

// Mid bell. Identical on both units.
inline constexpr std::array<float, 6> kMidFreqs { 360.0f, 700.0f, 1600.0f, 3200.0f, 4800.0f, 7200.0f };

// Low shelf. Identical on both units.
inline constexpr std::array<float, 4> kLowShelfFreqs { 35.0f, 60.0f, 110.0f, 220.0f };

// High-pass, 18 dB/octave. The two units differ here.
inline constexpr std::array<float, 4> kHpfFreqs1073 { 50.0f, 80.0f, 160.0f, 300.0f };
inline constexpr std::array<float, 4> kHpfFreqs1084 { 45.0f, 70.0f, 160.0f, 360.0f };

// Low-pass. 1084 only; inert in 1073 mode.
inline constexpr std::array<float, 5> kLpfFreqs1084 { 6000.0f, 8000.0f, 10000.0f, 14000.0f, 18000.0f };

//==============================================================================
// Branch Q values.
//
// PROVISIONAL. The *structure* of the EQ (see EqNetwork) follows the topology
// of the original: a set of resonant branches sharing one feedback path, which
// is what produces band interaction and proportional Q. These Q numbers, by
// contrast, are first-pass estimates chosen to land near the published curve
// shapes. They are the main thing to refine against measured targets once the
// scipy analysis in tools/python is in place.
//==============================================================================

//------------------------------------------------------------------------------
// Mid band Q, per switch position, derived from the circuit rather than guessed.
//
// A series LC branch has Q = w0 * L / R, so Q tracks the product of centre
// frequency and inductance. The mid band does not switch these uniformly: the
// lower three positions switch both inductance and capacitance, using taps of
// 10 H, 7 H and 3 H, which holds Q roughly level; the upper three share a
// single 200 mH winding and switch capacitance alone, so there Q climbs in
// proportion to frequency. That asymmetry is why 360 Hz is broad and musical
// while 7.2 kHz is a focused presence peak -- behaviour a single Q constant
// cannot produce, and this model previously did not have.
//
// R is not published, so each group is anchored by one measurement: the lower
// group so that 1.6 kHz lands at Q = 1.20, which is where measurements of real
// units put it, and the upper group just below that at the inductor change.
//------------------------------------------------------------------------------

inline constexpr std::array<float, 6> kMidBranchQ { 0.90f, 1.22f, 1.20f, 1.15f, 1.72f, 2.59f };

/** The 1084's Hi-Q switch narrows the mid band; it scales whatever the
    position's Q already is rather than replacing it. */
inline constexpr float kMidHiQFactor = 2.0f;
inline constexpr float kLowShelfQ  = 0.85f;  // >0.707 gives the inductor shelf's slight dip
inline constexpr float kHighShelfQ = 0.75f;
inline constexpr float kHpfQ       = 1.30f;  // passive LC filter's resonant bump
inline constexpr float kLpfQ       = 0.80f;

//==============================================================================
inline constexpr float highShelfFreq (Model m, int position) noexcept
{
    if (m != Model::m1084)
        return kHighShelfFreq1073;

    const auto i = (position < 0 ? 0 : (position > 2 ? 2 : position));
    return kHighShelfFreqs1084[(size_t) i];
}

inline constexpr float midFreq (int position) noexcept
{
    const auto i = (position < 0 ? 0 : (position > 5 ? 5 : position));
    return kMidFreqs[(size_t) i];
}

inline constexpr float lowShelfFreq (int position) noexcept
{
    const auto i = (position < 0 ? 0 : (position > 3 ? 3 : position));
    return kLowShelfFreqs[(size_t) i];
}

/** Mid Q as a function of centre frequency, interpolated in log frequency
    between the switch positions.

    Taking frequency rather than an index means a selector gliding between two
    detents carries its Q along with it, and the curve display and the audio
    path derive it the same way from the same value. */
inline float midBranchQ (float hz, bool hiQ) noexcept
{
    const auto f = hz <= 0.0f ? kMidFreqs[0] : hz;

    float q = kMidBranchQ[0];

    if (f >= kMidFreqs[5])
    {
        q = kMidBranchQ[5];
    }
    else if (f > kMidFreqs[0])
    {
        for (size_t i = 0; i + 1 < kMidFreqs.size(); ++i)
        {
            if (f <= kMidFreqs[i + 1])
            {
                const auto t = (std::log2 (f) - std::log2 (kMidFreqs[i]))
                             / (std::log2 (kMidFreqs[i + 1]) - std::log2 (kMidFreqs[i]));

                q = kMidBranchQ[i] + t * (kMidBranchQ[i + 1] - kMidBranchQ[i]);
                break;
            }
        }
    }

    return hiQ ? q * kMidHiQFactor : q;
}

/** Low-pass, 1084 only. Position 0 is Off, hence the -1. Returns 0 for Off. */
inline constexpr float lpfFreq (Model m, int position) noexcept
{
    if (m != Model::m1084 || position <= 0)
        return 0.0f;

    const auto i = (position > 5 ? 5 : position) - 1;
    return kLpfFreqs1084[(size_t) i];
}

/** High-pass. Position 0 is Off, hence the -1. Returns 0 for Off. */
inline constexpr float hpfFreq (Model m, int position) noexcept
{
    if (position <= 0)
        return 0.0f;

    const auto i = (position > 4 ? 4 : position) - 1;
    return m == Model::m1084 ? kHpfFreqs1084[(size_t) i] : kHpfFreqs1073[(size_t) i];
}

/** True for controls the 1073 does not have. The DSP ignores them and the
    editor greys them out, but the parameters always exist so that automation
    and saved state survive a model switch. */
inline constexpr bool isHighShelfSelectable (Model m) noexcept { return m == Model::m1084; }
inline constexpr bool isMidHiQAvailable     (Model m) noexcept { return m == Model::m1084; }
inline constexpr bool isLowPassAvailable    (Model m) noexcept { return m == Model::m1084; }

} // namespace frostyeq
