#pragma once

#include "../IR/GraphIR.h"
#include "../Sync/SyncMap.h"

#include <string>
#include <vector>

namespace duodsp::text
{
struct Diagnostic
{
    std::string message;
    int start = 0;
    int end = 0;
};

struct CompileResult
{
    ir::Graph graph;
    sync::SyncMap syncMap;
    std::vector<Diagnostic> diagnostics;
    std::string prettyPrinted;
};

CompileResult compile(const std::string& source, const ir::Graph* previousGraph = nullptr);
std::string prettyPrint(const ir::Graph& graph);
} // namespace duodsp::text

