#include "EqCurveDisplay.h"
#include "params/ParameterLayout.h"

namespace frostyeq::gui
{

namespace P = frostyeq::params;

namespace
{
    float rawValue (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        auto* p = s.getRawParameterValue (id);
        return p != nullptr ? p->load (std::memory_order_relaxed) : 0.0f;
    }

    constexpr std::array<double, 10> kGridFrequencies {
        20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0
    };

    juce::String frequencyLabel (double hz)
    {
        return hz >= 1000.0 ? juce::String (hz / 1000.0, hz >= 10000.0 ? 0 : 0) + "k"
                            : juce::String ((int) hz);
    }
}

//==============================================================================
EqCurveDisplay::EqCurveDisplay (juce::AudioProcessorValueTreeState& s)
    : state (s)
{
    network.prepare (sampleRate);
    refreshSettings();
    startTimerHz (30);
}

//==============================================================================
void EqCurveDisplay::setEqSampleRate (double rate)
{
    if (rate <= 0.0 || juce::approximatelyEqual (rate, sampleRate))
        return;

    sampleRate = rate;
    network.prepare (sampleRate);
    network.setSettings (settings);
    rebuildCurve();
    repaint();
}

bool EqCurveDisplay::refreshSettings()
{
    const auto model = (Model) (int) rawValue (state, P::kModel);

    EqSettings next;
    next.model     = model;
    next.hfFreqHz  = highShelfFreq (model, (int) rawValue (state, P::kHfFreq));
    next.hfGainDb  = rawValue (state, P::kHfGain);
    next.midFreqHz = midFreq ((int) rawValue (state, P::kMidFreq));
    next.midGainDb = rawValue (state, P::kMidGain);
    next.midHiQ    = rawValue (state, P::kMidHiQ) > 0.5f;
    next.lfFreqHz  = lowShelfFreq ((int) rawValue (state, P::kLfFreq));
    next.lfGainDb  = rawValue (state, P::kLfGain);
    next.hpfFreqHz = hpfFreq (model, (int) rawValue (state, P::kHpfFreq));
    next.lpfFreqHz = lpfFreq (model, (int) rawValue (state, P::kLpfFreq));

    const auto nextEqIn = rawValue (state, P::kEqIn) > 0.5f;

    const auto same = next.model == settings.model
        && next.midHiQ == settings.midHiQ
        && nextEqIn == eqIn
        && juce::approximatelyEqual (next.hfFreqHz,  settings.hfFreqHz)
        && juce::approximatelyEqual (next.hfGainDb,  settings.hfGainDb)
        && juce::approximatelyEqual (next.midFreqHz, settings.midFreqHz)
        && juce::approximatelyEqual (next.midGainDb, settings.midGainDb)
        && juce::approximatelyEqual (next.lfFreqHz,  settings.lfFreqHz)
        && juce::approximatelyEqual (next.lfGainDb,  settings.lfGainDb)
        && juce::approximatelyEqual (next.hpfFreqHz, settings.hpfFreqHz)
        && juce::approximatelyEqual (next.lpfFreqHz, settings.lpfFreqHz);

    if (same)
        return false;

    settings = next;
    eqIn     = nextEqIn;
    network.setSettings (settings);
    return true;
}

void EqCurveDisplay::timerCallback()
{
    if (refreshSettings())
    {
        rebuildCurve();
        repaint();
    }
}

//==============================================================================
float EqCurveDisplay::frequencyToX (double hz) const noexcept
{
    const auto t = std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz);
    return plot.getX() + (float) t * plot.getWidth();
}

float EqCurveDisplay::decibelsToY (double db) const noexcept
{
    const auto t = (db + kRangeDb) / (2.0 * kRangeDb);
    return plot.getBottom() - (float) t * plot.getHeight();
}

void EqCurveDisplay::rebuildCurve()
{
    curve.clear();

    if (plot.isEmpty())
        return;

    const auto points = juce::jmax (2, (int) plot.getWidth());

    for (int i = 0; i < points; ++i)
    {
        const auto t  = (double) i / (double) (points - 1);
        const auto hz = kMinHz * std::pow (kMaxHz / kMinHz, t);
        const auto db = eqIn ? network.magnitudeDbAt (hz) : 0.0;

        const auto x = plot.getX() + (float) t * plot.getWidth();
        const auto y = decibelsToY (juce::jlimit (-kRangeDb, kRangeDb, db));

        if (i == 0) curve.startNewSubPath (x, y);
        else        curve.lineTo (x, y);
    }
}

void EqCurveDisplay::resized()
{
    // Inset horizontally so the 20 Hz and 20 kHz labels are not clipped by the
    // panel edges, and leave a strip at the bottom for the frequency scale.
    plot = getLocalBounds().toFloat().reduced (18.0f, 1.0f);
    plot.removeFromBottom (13.0f);
    rebuildCurve();
}

//==============================================================================
void EqCurveDisplay::paint (juce::Graphics& g)
{
    g.setColour (theme::panelDeep);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), theme::corner);

    g.setFont (theme::labelFont (9.0f));

    // Decibel grid.
    for (int db = -18; db <= 18; db += 6)
    {
        const auto y = decibelsToY (db);

        g.setColour (db == 0 ? theme::gridEmphasis : theme::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());

        if (db != 0)
        {
            g.setColour (theme::textDim.withAlpha (0.55f));
            g.drawText (juce::String (db > 0 ? "+" : "") + juce::String (db),
                        juce::Rectangle<float> (plot.getX() + 3.0f, y - 9.0f, 26.0f, 10.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    // Frequency grid and scale.
    for (auto hz : kGridFrequencies)
    {
        const auto x = frequencyToX (hz);

        g.setColour (theme::grid);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

        g.setColour (theme::textDim.withAlpha (0.55f));
        g.drawText (frequencyLabel (hz),
                    juce::Rectangle<float> (x - 16.0f, plot.getBottom() + 1.0f, 32.0f, 11.0f),
                    juce::Justification::centred, false);
    }

    if (curve.isEmpty())
        return;

    // Shade between the curve and the 0 dB line.
    {
        auto filled = curve;
        filled.lineTo (plot.getRight(), decibelsToY (0.0));
        filled.lineTo (plot.getX(),     decibelsToY (0.0));
        filled.closeSubPath();

        g.setColour (eqIn ? theme::accentSoft : theme::accentSoft.withAlpha (0.06f));
        g.fillPath (filled);
    }

    g.setColour (eqIn ? theme::accent : theme::accent.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

    if (! eqIn)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (11.0f));
        g.drawText ("EQ OUT", plot, juce::Justification::centred, false);
    }

    g.setColour (theme::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), theme::corner, 1.0f);
}

} // namespace frostyeq::gui
