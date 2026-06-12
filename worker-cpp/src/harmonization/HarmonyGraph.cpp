#include "harmonization/HarmonyGraph.h"
#include <algorithm>
#include <unordered_set>

void HarmonyGraph::clear() {
    levels.clear();
    edges.clear();
    uniqueChordNamesByLevel.clear();
    nextNodeId = 0;
}

void HarmonyGraph::buildNodes(const std::vector<std::vector<Chord>>& chordsByPosition) {
    clear();

    for (int pos = 0; pos < static_cast<int>(chordsByPosition.size()); ++pos) {
        std::vector<HarmonyGraphNode> level;
        for (const Chord& chord : chordsByPosition[pos]) {
            HarmonyGraphNode node;
            node.nodeId = nextNodeId++;
            node.position = pos;
            node.chord = chord;
            level.push_back(node);
        }
        levels.push_back(std::move(level));
    }
}

void HarmonyGraph::build(const std::vector<std::vector<Chord>>& chordsByPosition,
                         const std::vector<HarmonicPosition>& positions) {
    build(chordsByPosition, positions, ActiveRuleSet::allEnabled());
}

void HarmonyGraph::build(const std::vector<std::vector<Chord>>& chordsByPosition,
                         const std::vector<HarmonicPosition>& positions,
                         const ActiveRuleSet& rules) {
    activeRules = rules;
    harmonicPositions = positions;
    buildNodes(chordsByPosition);
    buildEdges();
    pruneDeadEnds();
    rebuildUniqueChordNamesByLevel();
}

void HarmonyGraph::buildEdges() {
    edges.clear();

    if (levels.empty()) return;

    std::unordered_set<int> validStartIds;
    for (const HarmonyGraphNode& node : levels[0]) {
        bool valid = harmonicPositions.empty()
            ? HarmonyRules::isValidChord(node.chord, activeRules)
            : HarmonyRules::isValidChord(node.chord, harmonicPositions[0], activeRules);
        if (valid) validStartIds.insert(node.nodeId);
    }

    for (int i = 0; i < static_cast<int>(levels.size()) - 1; ++i) {
        for (const HarmonyGraphNode& prev : levels[i]) {
            if (i == 0 && validStartIds.count(prev.nodeId) == 0) continue;

            for (const HarmonyGraphNode& curr : levels[i + 1]) {
                if (!harmonicPositions.empty()
                    && (i + 1) < static_cast<int>(harmonicPositions.size())
                    && !HarmonyRules::isValidChord(curr.chord, harmonicPositions[i + 1], activeRules))
                    continue;

                bool connected = harmonicPositions.empty()
                    ? HarmonyRules::isValidConnection(prev.chord, curr.chord, activeRules)
                    : HarmonyRules::isValidConnection(prev.chord, curr.chord,
                                                       harmonicPositions[i], harmonicPositions[i + 1],
                                                       activeRules);
                if (connected) {
                    edges.push_back({prev.nodeId, curr.nodeId});
                }
            }
        }
    }
}

const std::vector<std::vector<HarmonyGraphNode>>& HarmonyGraph::getLevels() const {
    return levels;
}

const std::vector<HarmonyGraphEdge>& HarmonyGraph::getEdges() const {
    return edges;
}

bool HarmonyGraph::empty() const {
    return levels.empty();
}

bool HarmonyGraph::hasIncomingEdge(int nodeId) const {
    for (const HarmonyGraphEdge& edge : edges) {
        if (edge.toNodeId == nodeId) return true;
    }
    return false;
}

bool HarmonyGraph::hasOutgoingEdge(int nodeId) const {
    for (const HarmonyGraphEdge& edge : edges) {
        if (edge.fromNodeId == nodeId) return true;
    }
    return false;
}

bool HarmonyGraph::containsNode(int nodeId) const {
    for (const auto& level : levels) {
        for (const HarmonyGraphNode& node : level) {
            if (node.nodeId == nodeId) return true;
        }
    }
    return false;
}

void HarmonyGraph::removeNode(int nodeId) {
    edges.erase(
        std::remove_if(edges.begin(), edges.end(), [nodeId](const HarmonyGraphEdge& e) {
            return e.fromNodeId == nodeId || e.toNodeId == nodeId;
        }),
        edges.end()
    );

    for (auto& level : levels) {
        level.erase(
            std::remove_if(level.begin(), level.end(), [nodeId](const HarmonyGraphNode& n) {
                return n.nodeId == nodeId;
            }),
            level.end()
        );
    }
}

const std::vector<std::unordered_set<std::string>>& HarmonyGraph::getUniqueChordNamesByLevel() const {
    return uniqueChordNamesByLevel;
}

size_t HarmonyGraph::getMaxDistinctChordNameCount() const {
    size_t maxCount = 0;
    for (const auto& names : uniqueChordNamesByLevel) {
        if (names.size() > maxCount) maxCount = names.size();
    }
    return maxCount;
}

int HarmonyGraph::getLevelWithMaxDistinctChordNames() const {
    if (uniqueChordNamesByLevel.empty()) return -1;

    int bestLevel = -1;
    size_t bestCount = 0;
    for (int i = 0; i < static_cast<int>(uniqueChordNamesByLevel.size()); ++i) {
        if (uniqueChordNamesByLevel[i].size() > bestCount) {
            bestCount = uniqueChordNamesByLevel[i].size();
            bestLevel = i;
        }
    }
    return bestLevel;
}

void HarmonyGraph::rebuildUniqueChordNamesByLevel() {
    uniqueChordNamesByLevel.clear();
    uniqueChordNamesByLevel.resize(levels.size());

    for (int i = 0; i < static_cast<int>(levels.size()); ++i) {
        for (const HarmonyGraphNode& node : levels[i]) {
            uniqueChordNamesByLevel[i].insert(node.chord.getName());
        }
    }
}

void HarmonyGraph::pruneDeadEnds() {
    if (levels.empty()) return;

    const int lastLevel = static_cast<int>(levels.size()) - 1;

    bool changed = true;
    while (changed) {
        changed = false;

        for (int i = 0; i <= lastLevel; ++i) {
            std::vector<int> toRemove;

            for (const HarmonyGraphNode& node : levels[i]) {
                bool needsIncoming = (i > 0);
                bool needsOutgoing = (i < lastLevel);

                bool ok = true;
                if (needsIncoming && !hasIncomingEdge(node.nodeId)) ok = false;
                if (needsOutgoing && !hasOutgoingEdge(node.nodeId)) ok = false;

                if (!ok) toRemove.push_back(node.nodeId);
            }

            for (int nodeId : toRemove) {
                removeNode(nodeId);
                changed = true;
            }
        }
    }
}
