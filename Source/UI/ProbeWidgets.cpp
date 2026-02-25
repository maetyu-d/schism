#include "ProbeWidgets.h"

namespace duodsp::ui
{
void ScopeWidget::setSamples(std::vector<float> samples)
{
    data = std::move(samples);
    repaint();
}

void ScopeWidget::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
    g.setColour(juce::Colour(0xff2e3a4d));
    g.drawRect(getLocalBounds());

    if (data.size() < 2)
        return;

    juce::Path p;
    const auto w = static_cast<float>(getWidth());
    const auto h = static_cast<float>(getHeight());
    const auto mid = h * 0.5f;
    p.startNewSubPath(0.0f, mid);
    for (size_t i = 0; i < data.size(); ++i)
    {
        const auto x = static_cast<float>(i) / static_cast<float>(data.size() - 1) * w;
        const auto y = mid - juce::jlimit(-1.0f, 1.0f, data[i]) * (h * 0.45f);
        if (i == 0)
            p.startNewSubPath(x, y);
        else
            p.lineTo(x, y);
    }
    g.setColour(juce::Colour(0xff8ecae6));
    g.strokePath(p, juce::PathStrokeType(1.5f));
}

void SpectrumWidget::setBins(std::vector<float> bins)
{
    data = std::move(bins);
    repaint();
}

void SpectrumWidget::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
    g.setColour(juce::Colour(0xff2e3a4d));
    g.drawRect(getLocalBounds());

    if (data.empty())
        return;

    const auto w = static_cast<float>(getWidth());
    const auto h = static_cast<float>(getHeight());
    const auto barW = w / static_cast<float>(data.size());
    for (size_t i = 0; i < data.size(); ++i)
    {
        const auto v = juce::jlimit(0.0f, 1.0f, data[i]);
        const auto bh = v * h;
        juce::Rectangle<float> r(static_cast<float>(i) * barW, h - bh, juce::jmax(1.0f, barW - 1.0f), bh);
        g.setColour(juce::Colour(0xfff4a261));
        g.fillRect(r);
    }
}
} // namespace duodsp::ui

