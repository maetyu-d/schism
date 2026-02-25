#pragma once

#include <string>
#include <vector>

namespace duodsp::sync
{
struct NodeTextRange
{
    std::string nodeId;
    int start = 0;
    int end = 0;
};

class SyncMap
{
public:
    void clear();
    void addRange(const std::string& nodeId, int start, int end);
    const NodeTextRange* findByNode(const std::string& nodeId) const;
    const NodeTextRange* findByPosition(int pos) const;
    const std::vector<NodeTextRange>& ranges() const { return nodeRanges; }

private:
    std::vector<NodeTextRange> nodeRanges;
};
} // namespace duodsp::sync

