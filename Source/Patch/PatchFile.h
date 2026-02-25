#pragma once

#include "../IR/GraphIR.h"

#include <juce_core/juce_core.h>

#include <string>
#include <unordered_map>

namespace duodsp::patch
{
struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct PatchDocument
{
    juce::String codeText;
    ir::Graph graph;
    std::unordered_map<std::string, Vec2> layout;
    float splitRatio = 0.54f;
};

bool saveToFile(const juce::File& file, const PatchDocument& doc, juce::String& error);
bool loadFromFile(const juce::File& file, PatchDocument& doc, juce::String& error);
} // namespace duodsp::patch
