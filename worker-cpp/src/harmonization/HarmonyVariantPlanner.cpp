#include "HarmonyVariantPlanner.h"
#include "domain/ChordTemplate.h"
#include "domain/HarmonyRules.h"
#include <algorithm>
#include <climits>
#include <functional>

bool ChordNameCoverageKey::operator==(const ChordNameCoverageKey& other) const {
    return levelIndex == other.levelIndex && chordName == other.chordName;
}

size_t ChordNameCoverageKeyHash::operator()(const ChordNameCoverageKey& key) const {
    size_t h1 = std::hash<int>{}(key.levelIndex);
    size_t h2 = std::hash<std::string>{}(key.chordName);
    return h1 ^ (h2 << 1);
}

int HarmonyVariantPlanner::calculateNodePriority(const HarmonyGraphNode& node) const {
    int priority = 0;

    switch (node.chord.getType()) {
        case ChordType::Triad:            priority += 0;  break;
        case ChordType::Six:              priority += 10; break;
        case ChordType::SixFour:          priority += 10; break;
        case ChordType::CadentialSixFour: priority += 15; break;
        case ChordType::Seventh:          priority += 20; break;
        case ChordType::SixFive:          priority += 22; break;
        case ChordType::FourThree:        priority += 22; break;
        case ChordType::Two:              priority += 25; break;
        case ChordType::Ninth:            priority += 30; break;
    }

    switch (node.chord.getDegree()) {
        case 2: priority += 14; break;
        case 3: priority += 8; break;
        case 6: priority += 14; break;
        case 7: priority += 15; break;
        default: break;
    }

    return priority;
}

void HarmonyVariantPlanner::initializeUsage(const HarmonyGraph& graph) {
    usageByNodeId.clear();
    coveredChordNames.clear();

    for (const auto& level : graph.getLevels()) {
        for (const auto& node : level) {
            HarmonyNodeUsage usage;
            usage.priority = calculateNodePriority(node);
            usage.covered = false;
            usage.unreachable = false;
            usage.usedCount = 0;
            usageByNodeId[node.nodeId] = usage;
        }
    }
}

bool HarmonyVariantPlanner::hasRemainingImportantNodes(const HarmonyGraph& graph) const {
    for (const auto& level : graph.getLevels()) {
        for (const auto& node : level) {
            auto it = usageByNodeId.find(node.nodeId);
            if (it == usageByNodeId.end()) continue;
            const HarmonyNodeUsage& usage = it->second;
            if (usage.priority >= MIN_PRIORITY_TO_COVER
                && !usage.unreachable
                && !isChordNameCovered(node)) {
                return true;
            }
        }
    }
    return false;
}

int HarmonyVariantPlanner::selectTargetNodeId(const HarmonyGraph& graph) const {
    int bestId = -1;
    bool bestNameUncovered = false;
    int bestPriority = -1;
    int bestUsedCount = INT_MAX;

    for (const auto& level : graph.getLevels()) {
        for (const auto& node : level) {
            auto it = usageByNodeId.find(node.nodeId);
            if (it == usageByNodeId.end()) continue;
            const HarmonyNodeUsage& usage = it->second;
            if (usage.priority < MIN_PRIORITY_TO_COVER || usage.unreachable) continue;

            bool nameUncovered = !isChordNameCovered(node);

            if (bestId == -1) {
                bestId = node.nodeId;
                bestNameUncovered = nameUncovered;
                bestPriority = usage.priority;
                bestUsedCount = usage.usedCount;
                continue;
            }

            // uncovered chord name wins over covered
            if (nameUncovered && !bestNameUncovered) {
                bestId = node.nodeId;
                bestNameUncovered = nameUncovered;
                bestPriority = usage.priority;
                bestUsedCount = usage.usedCount;
                continue;
            }
            if (!nameUncovered && bestNameUncovered) continue;

            // same coverage tier: higher priority wins
            if (usage.priority > bestPriority) {
                bestId = node.nodeId;
                bestNameUncovered = nameUncovered;
                bestPriority = usage.priority;
                bestUsedCount = usage.usedCount;
                continue;
            }
            if (usage.priority < bestPriority) continue;

            // same priority: lower usedCount wins
            if (usage.usedCount < bestUsedCount) {
                bestId = node.nodeId;
                bestNameUncovered = nameUncovered;
                bestPriority = usage.priority;
                bestUsedCount = usage.usedCount;
                continue;
            }
            if (usage.usedCount > bestUsedCount) continue;

            // same usedCount: lower nodeId wins
            if (node.nodeId < bestId) {
                bestId = node.nodeId;
                bestNameUncovered = nameUncovered;
                bestPriority = usage.priority;
                bestUsedCount = usage.usedCount;
            }
        }
    }
    return bestId;
}

const HarmonyGraphNode* HarmonyVariantPlanner::findNodeById(
    const HarmonyGraph& graph, int nodeId) const
{
    for (const auto& level : graph.getLevels()) {
        for (const auto& node : level) {
            if (node.nodeId == nodeId) return &node;
        }
    }
    return nullptr;
}

bool HarmonyVariantPlanner::areNodesConnected(
    const HarmonyGraph& graph, int fromNodeId, int toNodeId) const
{
    for (const auto& edge : graph.getEdges()) {
        if (edge.fromNodeId == fromNodeId && edge.toNodeId == toNodeId) return true;
    }
    return false;
}

std::vector<const HarmonyGraphNode*> HarmonyVariantPlanner::getCandidateNodesForLevel(
    const HarmonyGraph& graph,
    int levelIndex,
    const std::vector<int>& currentNodeIds) const
{
    const auto& levels = graph.getLevels();
    if (levelIndex < 0 || levelIndex >= static_cast<int>(levels.size())) return {};

    const auto& level = levels[levelIndex];
    std::vector<const HarmonyGraphNode*> candidates;

    if (currentNodeIds.empty()) {
        for (const auto& node : level) {
            if (usageByNodeId.count(node.nodeId)) candidates.push_back(&node);
        }
    } else {
        int previousNodeId = currentNodeIds.back();
        for (const auto& node : level) {
            if (!usageByNodeId.count(node.nodeId)) continue;
            if (areNodesConnected(graph, previousNodeId, node.nodeId))
                candidates.push_back(&node);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [this](const HarmonyGraphNode* a, const HarmonyGraphNode* b) {
            bool aCovered = isChordNameCovered(*a);
            bool bCovered = isChordNameCovered(*b);
            if (aCovered != bCovered) return !aCovered; // uncovered first

            const HarmonyNodeUsage& ua = usageByNodeId.at(a->nodeId);
            const HarmonyNodeUsage& ub = usageByNodeId.at(b->nodeId);
            if (ua.priority != ub.priority) return ua.priority > ub.priority;
            if (ua.usedCount != ub.usedCount) return ua.usedCount < ub.usedCount;
            return a->nodeId < b->nodeId;
        });

    return candidates;
}

std::vector<HarmonyPath> HarmonyVariantPlanner::buildDiversePaths(
    const HarmonyGraph& graph, size_t maxVariants)
{
    std::vector<HarmonyPath> paths;

    if (graph.empty() || maxVariants == 0) return paths;

    initializeUsage(graph);

    size_t attempts = 0;
    size_t maxAttempts = maxVariants * 10;
    if (maxAttempts == 0) maxAttempts = 1;

    while (paths.size() < maxVariants
           && hasRemainingImportantNodes(graph)
           && attempts < maxAttempts)
    {
        attempts++;

        int targetNodeId = selectTargetNodeId(graph);
        if (targetNodeId == -1) break;

        HarmonyPath path;
        bool success = findPathThroughNode(graph, targetNodeId, path);

        if (!success) {
            auto it = usageByNodeId.find(targetNodeId);
            if (it != usageByNodeId.end()) it->second.unreachable = true;
            continue;
        }

        markSuccessfulPath(graph, path);
        paths.push_back(std::move(path));
    }

    return paths;
}

void HarmonyVariantPlanner::markSuccessfulPath(
    const HarmonyGraph& graph, const HarmonyPath& path)
{
    for (int nodeId : path.nodeIds) {
        const HarmonyGraphNode* node = findNodeById(graph, nodeId);
        if (!node) continue;

        auto it = usageByNodeId.find(nodeId);
        if (it == usageByNodeId.end()) continue;

        it->second.covered = true;
        it->second.usedCount++;
        markChordNameCovered(*node);
    }
}

bool HarmonyVariantPlanner::findPathThroughNode(
    const HarmonyGraph& graph, int targetNodeId, HarmonyPath& path)
{
    path.nodeIds.clear();
    path.chords.clear();
    path.positions.clear();
    path.score = 0;

    std::vector<int> currentNodeIds;
    std::vector<Chord> currentChords;
    std::vector<HarmonicPosition> currentPositions;
    int currentScore = 0;

    if (!buildPathRecursive(graph, 0, targetNodeId, currentNodeIds, currentChords, currentPositions, currentScore))
        return false;

    path.nodeIds   = currentNodeIds;
    path.chords    = currentChords;
    path.positions = currentPositions;
    path.score     = currentScore;
    return true;
}

void HarmonyVariantPlanner::setGraphPositions(const std::vector<HarmonicPosition>& positions) {
    graphPositions = positions;
}

bool HarmonyVariantPlanner::buildPathRecursive(
    const HarmonyGraph& graph,
    int levelIndex,
    int targetNodeId,
    std::vector<int>& currentNodeIds,
    std::vector<Chord>& currentChords,
    std::vector<HarmonicPosition>& currentPositions,
    int& currentScore)
{
    if (levelIndex >= static_cast<int>(graph.getLevels().size())) return true;

    auto candidates = getCandidateNodesForLevel(graph, levelIndex, currentNodeIds);

    // If the target node hasn't been visited yet and is available on this level,
    // force the path through it — skip all other candidates.
    bool targetAlreadyUsed = false;
    for (int id : currentNodeIds) {
        if (id == targetNodeId) { targetAlreadyUsed = true; break; }
    }
    if (!targetAlreadyUsed) {
        bool targetOnLevel = false;
        for (const HarmonyGraphNode* c : candidates) {
            if (c->nodeId == targetNodeId) { targetOnLevel = true; break; }
        }
        if (targetOnLevel) {
            candidates.erase(
                std::remove_if(candidates.begin(), candidates.end(),
                    [targetNodeId](const HarmonyGraphNode* c) {
                        return c->nodeId != targetNodeId;
                    }),
                candidates.end());
        }
    }

    for (const HarmonyGraphNode* candidate : candidates) {
        // Triple-rule check
        if (currentChords.size() >= 2) {
            if (!HarmonyRules::isValidConnection(
                    currentChords[currentChords.size() - 2],
                    currentChords[currentChords.size() - 1],
                    candidate->chord))
                continue;
        }

        currentNodeIds.push_back(candidate->nodeId);
        currentChords.push_back(candidate->chord);
        if (levelIndex < static_cast<int>(graphPositions.size()))
            currentPositions.push_back(graphPositions[levelIndex]);

        int addedScore = 0;
        auto it = usageByNodeId.find(candidate->nodeId);
        if (it != usageByNodeId.end()) addedScore = it->second.priority;
        currentScore += addedScore;

        if (buildPathRecursive(graph, levelIndex + 1, targetNodeId,
                               currentNodeIds, currentChords, currentPositions, currentScore))
            return true;

        currentScore -= addedScore;
        currentNodeIds.pop_back();
        currentChords.pop_back();
        if (levelIndex < static_cast<int>(graphPositions.size()))
            currentPositions.pop_back();
    }

    return false;
}

bool HarmonyVariantPlanner::isChordNameCovered(const HarmonyGraphNode& node) const {
    return coveredChordNames.count({ node.position, node.chord.getName() }) > 0;
}

void HarmonyVariantPlanner::markChordNameCovered(const HarmonyGraphNode& node) {
    coveredChordNames.insert({ node.position, node.chord.getName() });
}
