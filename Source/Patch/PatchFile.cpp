#include "PatchFile.h"

#include <juce_data_structures/juce_data_structures.h>

namespace duodsp::patch
{
namespace
{
juce::var nodeToVar(const ir::Node& n)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("id", juce::String(n.id));
    o->setProperty("op", juce::String(n.op));
    o->setProperty("label", juce::String(n.label));
    if (n.literal.has_value())
        o->setProperty("literal", *n.literal);
    else
        o->setProperty("literal", juce::var());
    return juce::var(o);
}

juce::var edgeToVar(const ir::Edge& e)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("from", juce::String(e.fromNodeId));
    o->setProperty("to", juce::String(e.toNodeId));
    o->setProperty("port", e.toPort);
    return juce::var(o);
}
} // namespace

bool saveToFile(const juce::File& file, const PatchDocument& doc, juce::String& error)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("schema", "schism-patch-v1");
    root->setProperty("code", doc.codeText);
    root->setProperty("splitRatio", doc.splitRatio);

    juce::Array<juce::var> nodes;
    for (const auto& n : doc.graph.nodes)
        nodes.add(nodeToVar(n));
    root->setProperty("nodes", nodes);

    juce::Array<juce::var> edges;
    for (const auto& e : doc.graph.edges)
        edges.add(edgeToVar(e));
    root->setProperty("edges", edges);

    auto* bindings = new juce::DynamicObject();
    for (const auto& [name, id] : doc.graph.bindings)
        bindings->setProperty(juce::Identifier(name), juce::String(id));
    root->setProperty("bindings", juce::var(bindings));

    auto* layout = new juce::DynamicObject();
    for (const auto& [id, p] : doc.layout)
    {
        auto* pObj = new juce::DynamicObject();
        pObj->setProperty("x", p.x);
        pObj->setProperty("y", p.y);
        layout->setProperty(juce::Identifier(id), juce::var(pObj));
    }
    root->setProperty("layout", juce::var(layout));

    const auto json = juce::JSON::toString(juce::var(root), true);
    if (!file.replaceWithText(json))
    {
        error = "Failed to write patch file.";
        return false;
    }
    return true;
}

bool loadFromFile(const juce::File& file, PatchDocument& doc, juce::String& error)
{
    const auto text = file.loadFileAsString();
    if (text.isEmpty())
    {
        error = "Failed to read patch file.";
        return false;
    }

    juce::var parsed;
    auto result = juce::JSON::parse(text, parsed);
    if (result.failed() || !parsed.isObject())
    {
        error = "Invalid patch JSON.";
        return false;
    }

    const auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        error = "Invalid patch root object.";
        return false;
    }

    PatchDocument loaded;
    loaded.codeText = root->getProperty("code").toString();
    loaded.splitRatio = static_cast<float>(root->getProperty("splitRatio"));
    if (!(loaded.splitRatio > 0.0f && loaded.splitRatio < 1.0f))
        loaded.splitRatio = 0.54f;

    if (const auto& nodesVar = root->getProperty("nodes"); nodesVar.isArray())
    {
        for (const auto& nv : *nodesVar.getArray())
        {
            if (!nv.isObject())
                continue;
            const auto* o = nv.getDynamicObject();
            ir::Node n;
            n.id = o->getProperty("id").toString().toStdString();
            n.op = o->getProperty("op").toString().toStdString();
            n.label = o->getProperty("label").toString().toStdString();
            const auto lit = o->getProperty("literal");
            if (lit.isDouble() || lit.isInt() || lit.isInt64())
                n.literal = static_cast<double>(lit);
            loaded.graph.nodes.push_back(std::move(n));
        }
    }

    if (const auto& edgesVar = root->getProperty("edges"); edgesVar.isArray())
    {
        for (const auto& ev : *edgesVar.getArray())
        {
            if (!ev.isObject())
                continue;
            const auto* o = ev.getDynamicObject();
            ir::Edge e;
            e.fromNodeId = o->getProperty("from").toString().toStdString();
            e.toNodeId = o->getProperty("to").toString().toStdString();
            e.toPort = static_cast<int>(o->getProperty("port"));
            loaded.graph.edges.push_back(std::move(e));
        }
    }

    if (const auto bindingsVar = root->getProperty("bindings"); bindingsVar.isObject())
    {
        const auto* b = bindingsVar.getDynamicObject();
        for (const auto& p : b->getProperties())
            loaded.graph.bindings[p.name.toString().toStdString()] = p.value.toString().toStdString();
    }

    if (const auto layoutVar = root->getProperty("layout"); layoutVar.isObject())
    {
        const auto* l = layoutVar.getDynamicObject();
        for (const auto& p : l->getProperties())
        {
            if (!p.value.isObject())
                continue;
            const auto* po = p.value.getDynamicObject();
            loaded.layout[p.name.toString().toStdString()] = { static_cast<float>(po->getProperty("x")), static_cast<float>(po->getProperty("y")) };
        }
    }

    doc = std::move(loaded);
    return true;
}
} // namespace duodsp::patch

