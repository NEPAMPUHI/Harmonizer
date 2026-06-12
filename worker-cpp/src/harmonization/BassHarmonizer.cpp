#include "harmonization/BassHarmonizer.h"
#include "domain/HarmonizationJob.h"
#include "domain/ActiveRuleSet.h"
#include "harmonization/NoteDegreesResolver.h"
#include "harmonization/HarmonicRhythmValidator.h"
#include "domain/HarmonyRules.h"
#include <algorithm>
#include <string>
#include <unordered_set>

namespace {

std::string variantChordKey(const HarmonizationVariant& v) {
    std::string key;
    key.reserve(v.musicScore.chords.size() * 14);
    for (const auto& chord : v.musicScore.chords) {
        key += std::to_string(chord.getSoprano().getSemitone()); key += ',';
        key += std::to_string(chord.getAlto().getSemitone());    key += ',';
        key += std::to_string(chord.getTenor().getSemitone());   key += ',';
        key += std::to_string(chord.getBass().getSemitone());    key += '|';
    }
    return key;
}

void deduplicateAndSort(std::vector<HarmonizationVariant>& variants) {
    std::stable_sort(variants.begin(), variants.end(),
        [](const HarmonizationVariant& a, const HarmonizationVariant& b) {
            if (a.rhythmPlanPriority != b.rhythmPlanPriority)
                return a.rhythmPlanPriority < b.rhythmPlanPriority;
            return a.score > b.score;
        });
    std::unordered_set<std::string> seen;
    variants.erase(
        std::remove_if(variants.begin(), variants.end(),
            [&](const HarmonizationVariant& v) {
                return !seen.insert(variantChordKey(v)).second;
            }),
        variants.end());
}

} // namespace

std::vector<HarmonizationVariant> BassHarmonizer::harmonize(const ScoreInput& input,
                                                             const HarmonizationSettings& settings) {
    auto plans = harmonicRhythmPlanner.buildPlans(input.notes, settings);

    HarmonicRhythmValidator validator;
    std::vector<HarmonizationVariant> allVariants;
    for (int i = 0; i < static_cast<int>(plans.size()); ++i) {
        if (!validator.validate(plans[i], input.notes, settings).ok) continue;
        auto variants = harmonizePlan(plans[i], settings, i);
        allVariants.insert(allVariants.end(),
                           std::make_move_iterator(variants.begin()),
                           std::make_move_iterator(variants.end()));
    }
    deduplicateAndSort(allVariants);
    variantScorer.applyFinalRanking(allVariants);
    return allVariants;
}

std::vector<HarmonizationVariant> BassHarmonizer::harmonizePlan(
    const HarmonicRhythmPlan& plan,
    const HarmonizationSettings& settings,
    int planIndex)
{
    auto positions = harmonicPositionBuilder.build(
        plan.segments, settings, HarmonizationMode::HarmonizeBass);
    if (positions.empty()) return {};

    NoteDegreesResolver{}.resolveInPlace(positions, settings);

    const ActiveRuleSet rules = ActiveRuleSet::fromSettings(settings);
    auto chordsByPosition = buildChordsByPosition(positions, settings);
    harmonyGraph.build(chordsByPosition, positions, rules);
    harmonyVariantPlanner.setGraphPositions(positions);

    if (harmonyGraph.empty()) return {};
    for (const auto& level : harmonyGraph.getLevels()) {
        if (level.empty()) return {};
    }

    size_t maxVariants = harmonyGraph.getMaxDistinctChordNameCount();
    if (maxVariants == 0) return {};

    auto paths = harmonyVariantPlanner.buildDiversePaths(harmonyGraph, maxVariants);
    auto variants = buildVariantsFromPaths(paths, settings);
    for (auto& v : variants) {
        v.rhythmPlanIndex    = planIndex;
        v.rhythmPlanPriority = plan.priorityScore;
    }
    variantScorer.sortVariants(variants, settings);
    return variants;
}

std::vector<std::vector<Chord>> BassHarmonizer::buildChordsByPosition(
    const std::vector<HarmonicPosition>& positions,
    const HarmonizationSettings& settings)
{
    std::vector<std::vector<Chord>> result;
    result.reserve(positions.size());
    const int firstPositionIndex = positions.empty() ? -1 : positions.front().index;
    const int lastPositionIndex  = positions.empty() ? -1 : positions.back().index;
    for (const auto& position : positions) {
        auto chords = buildChordsForPosition(position, settings);
        if (position.index == firstPositionIndex && !position.fixedNote.isRest()) {
            chords.erase(
                std::remove_if(chords.begin(), chords.end(), [&](const Chord& c) {
                    return !HarmonyRules::checkInitialChordByBass(
                        c, position.fixedNote, position, firstPositionIndex);
                }),
                chords.end());
        }
        if (position.index == lastPositionIndex && !position.fixedNote.isRest()) {
            bool anyValid = std::any_of(chords.begin(), chords.end(), [&](const Chord& c) {
                return HarmonyRules::checkFinalChordByBass(
                    c, position.fixedNote, position, lastPositionIndex);
            });
            if (anyValid) {
                chords.erase(
                    std::remove_if(chords.begin(), chords.end(), [&](const Chord& c) {
                        return !HarmonyRules::checkFinalChordByBass(
                            c, position.fixedNote, position, lastPositionIndex);
                    }),
                    chords.end());
            }
        }
        result.push_back(std::move(chords));
    }
    return result;
}

std::vector<Chord> BassHarmonizer::buildChordsForPosition(
    const HarmonicPosition& position,
    const HarmonizationSettings& settings)
{
    if (position.fixedNote.isRest())
        return chordBuilder.buildAllValid(settings);
    return chordBuilder.buildForFixedBassNote(position.fixedNote, settings);
}

std::vector<HarmonizationVariant> BassHarmonizer::buildVariantsFromPaths(
    const std::vector<HarmonyPath>& paths,
    const HarmonizationSettings& settings)
{
    std::vector<HarmonizationVariant> variants;
    for (const HarmonyPath& path : paths) {
        Score score;
        score.chords          = path.chords;
        score.positions       = path.positions;
        score.fixedVoiceIndex = 3;  // bass is the fixed (bass-line) voice

        HarmonizationVariant variant;
        variant.musicScore = score;
        variant.score = path.score;

        if (variant.score == 0) {
            variant.score = variantScorer.score(variant, settings, HarmonizationMode::HarmonizeBass);
        } else {
            variant.score -= variantScorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeBass);
        }

        variants.push_back(std::move(variant));
    }
    return variants;
}
