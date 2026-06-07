#ifndef HARM_HARMONYVARIANTPLANNER_H
#define HARM_HARMONYVARIANTPLANNER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "HarmonyGraph.h"
#include "domain/HarmonicPosition.h"

struct HarmonyNodeUsage {
    bool covered = false;
    bool unreachable = false;
    int priority = 0;
    int usedCount = 0;
};

struct ChordNameCoverageKey {
    int levelIndex;
    std::string chordName;
    bool operator==(const ChordNameCoverageKey& other) const;
};

struct ChordNameCoverageKeyHash {
    size_t operator()(const ChordNameCoverageKey& key) const;
};

struct HarmonyPath {
    std::vector<int> nodeIds;
    std::vector<Chord> chords;
    std::vector<HarmonicPosition> positions;
    int score = 0;
};

class HarmonyVariantPlanner {
public:
    void initializeUsage(const HarmonyGraph& graph);
    bool hasRemainingImportantNodes(const HarmonyGraph& graph) const;
    int selectTargetNodeId(const HarmonyGraph& graph) const;
    bool findPathThroughNode(const HarmonyGraph& graph, int targetNodeId, HarmonyPath& path);
    void markSuccessfulPath(const HarmonyGraph& graph, const HarmonyPath& path);
    std::vector<HarmonyPath> buildDiversePaths(const HarmonyGraph& graph, size_t maxVariants);
    void setGraphPositions(const std::vector<HarmonicPosition>& positions);

private:
    std::unordered_map<int, HarmonyNodeUsage> usageByNodeId;
    std::unordered_set<ChordNameCoverageKey, ChordNameCoverageKeyHash> coveredChordNames;
    std::vector<HarmonicPosition> graphPositions;

    static constexpr int MIN_PRIORITY_TO_COVER = 1;

    int calculateNodePriority(const HarmonyGraphNode& node) const;
    bool isChordNameCovered(const HarmonyGraphNode& node) const;
    void markChordNameCovered(const HarmonyGraphNode& node);

    const HarmonyGraphNode* findNodeById(const HarmonyGraph& graph, int nodeId) const;
    bool areNodesConnected(const HarmonyGraph& graph, int fromNodeId, int toNodeId) const;
    std::vector<const HarmonyGraphNode*> getCandidateNodesForLevel(
        const HarmonyGraph& graph,
        int levelIndex,
        const std::vector<int>& currentNodeIds
    ) const;

    bool buildPathRecursive(
        const HarmonyGraph& graph,
        int levelIndex,
        int targetNodeId,
        std::vector<int>& currentNodeIds,
        std::vector<Chord>& currentChords,
        std::vector<HarmonicPosition>& currentPositions,
        int& currentScore
    );
};

#endif
