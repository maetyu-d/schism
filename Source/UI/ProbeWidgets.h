#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace duodsp::ui
{
class ScopeWidget final : public juce::Component
{
public:
    void setSamples(std::vector<float> samples);
    void paint(juce::Graphics& g) override;

private:
    std::vector<float> data;
};

class SpectrumWidget final : public juce::Component
{
public:
    void setBins(std::vector<float> bins);
    void paint(juce::Graphics& g) override;

private:
    std::vector<float> data;
};
} // namespace duodsp::ui

