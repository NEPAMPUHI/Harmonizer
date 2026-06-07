#ifndef HARM_HARMONYGRAPH_H
#define HARM_HARMONYGRAPH_H

#include <string>
#include <unordered_set>
#include <vector>
#include "domain/Chord.h"
#include "domain/HarmonyRules.h"

struct HarmonyGraphNode {
    int nodeId;
    int position;
    Chord chord;
};

struct HarmonyGraphEdge {
    int fromNodeId;
    int toNodeId;
};

class HarmonyGraph {
public:
    void clear();
    void buildNodes(const std::vector<std::vector<Chord>>& chordsByPosition);
    void build(const std::vector<std::vector<Chord>>& chordsByPosition);
    void buildEdges();
    void pruneDeadEnds();

    const std::vector<std::vector<HarmonyGraphNode>>& getLevels() const;
    const std::vector<HarmonyGraphEdge>& getEdges() const;
    const std::vector<std::unordered_set<std::string>>& getUniqueChordNamesByLevel() const;
    bool empty() const;

    bool hasIncomingEdge(int nodeId) const;
    bool hasOutgoingEdge(int nodeId) const;
    bool containsNode(int nodeId) const;

    size_t getMaxDistinctChordNameCount() const;
    int getLevelWithMaxDistinctChordNames() const;

private:
    void removeNode(int nodeId);
    void rebuildUniqueChordNamesByLevel();

    std::vector<std::vector<HarmonyGraphNode>> levels;
    std::vector<HarmonyGraphEdge> edges;
    std::vector<std::unordered_set<std::string>> uniqueChordNamesByLevel;
    int nextNodeId = 0;
};

#endif
