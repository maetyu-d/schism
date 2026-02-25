#include "SyncMap.h"

namespace duodsp::sync
{
void SyncMap::clear()
{
    nodeRanges.clear();
}

void SyncMap::addRange(const std::string& nodeId, const int start, const int end)
{
    nodeRanges.push_back({ nodeId, start, end });
}

const NodeTextRange* SyncMap::findByNode(const std::string& nodeId) const
{
    for (const auto& range : nodeRanges)
        if (range.nodeId == nodeId)
            return &range;
    return nullptr;
}

const NodeTextRange* SyncMap::findByPosition(const int pos) const
{
    for (const auto& range : nodeRanges)
        if (pos >= range.start && pos <= range.end)
            return &range;
    return nullptr;
}
} // namespace duodsp::sync

