#pragma once

#include <array>

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

// High shelf. The 1073 has a single fixed 12 kHz shelf; the control is present
// but inert in 1073 mode, and the editor greys it out.
inline constexpr std::array<float, 3> kHighShelfFreqs1073 { 12000.0f, 12000.0f, 12000.0f };
inline constexpr std::array<float, 3> kHighShelfFreqs1084 { 10000.0f, 12000.0f, 16000.0f };

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
inline constexpr float highShelfFreq (Model m, int position) noexcept
{
    const auto i = (position < 0 ? 0 : (position > 2 ? 2 : position));
    return m == Model::m1084 ? kHighShelfFreqs1084[(size_t) i]
                             : kHighShelfFreqs1073[(size_t) i];
}

inline constexpr float hpfFreq (Model m, int position) noexcept
{
    const auto i = (position < 0 ? 0 : (position > 3 ? 3 : position));
    return m == Model::m1084 ? kHpfFreqs1084[(size_t) i] : kHpfFreqs1073[(size_t) i];
}

/** True for controls the 1073 does not have. The DSP ignores them and the
    editor greys them out, but the parameters always exist so that automation
    and saved state survive a model switch. */
inline constexpr bool isHighShelfSelectable (Model m) noexcept { return m == Model::m1084; }
inline constexpr bool isMidHiQAvailable     (Model m) noexcept { return m == Model::m1084; }
inline constexpr bool isLowPassAvailable    (Model m) noexcept { return m == Model::m1084; }

} // namespace frostyeq
