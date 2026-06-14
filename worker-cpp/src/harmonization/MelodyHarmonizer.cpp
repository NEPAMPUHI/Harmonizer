#include "harmonization/MelodyHarmonizer.h"
#include "domain/HarmonizationJob.h"
#include "domain/ActiveRuleSet.h"
#include "harmonization/NoteDegreesResolver.h"
#include "domain/HarmonyRules.h"
#include <algorithm>

std::vector<HarmonizationVariant> MelodyHarmonizer::harmonize(const ScoreInput& input,
                                                               const HarmonizationSettings& settings) {
    auto plans = harmonicRhythmPlanner.buildPlans(input.notes, settings);
    if (plans.empty()) return {};
    auto variants = harmonizePlan(plans[0], settings);
    variantScorer.applyFinalRanking(variants);
    return variants;
}

std::vector<HarmonizationVariant> MelodyHarmonizer::harmonizePlan(
    const HarmonicRhythmPlan& plan,
    const HarmonizationSettings& settings)
{
    auto positions = harmonicPositionBuilder.build(
        plan.segments, settings, HarmonizationMode::HarmonizeMelody);
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
        v.rhythmPlanIndex    = 0;
        v.rhythmPlanPriority = 0;
    }
    variantScorer.sortVariants(variants, settings);
    return variants;
}

std::vector<std::vector<Chord>> MelodyHarmonizer::buildChordsByPosition(
    const std::vector<HarmonicPosition>& positions,
    const HarmonizationSettings& settings)
{
    std::vector<std::vector<Chord>> result;
    result.reserve(positions.size());
    const int firstPositionIndex = positions.empty() ? -1 : positions.front().index;
    const int lastPositionIndex  = positions.empty() ? -1 : positions.back().index;

    const int lastMeasureIdx = positions.empty() ? -1 : positions.back().measureIndex;
    int firstPosOfLastMeasure = -1;
    for (const auto& pos : positions) {
        if (pos.measureIndex == lastMeasureIdx) {
            firstPosOfLastMeasure = pos.index;
            break;
        }
    }

    for (const auto& position : positions) {
        auto chords = buildChordsForPosition(position, settings);
        if (position.index == firstPositionIndex && !position.fixedNote.isRest()) {
            chords.erase(
                std::remove_if(chords.begin(), chords.end(), [&](const Chord& c) {
                    return !HarmonyRules::checkInitialChordByFixedNote(
                        c, position.fixedNote, position, firstPositionIndex);
                }),
                chords.end());
        }
        if (position.index == firstPosOfLastMeasure && !position.fixedNote.isRest()) {
            chords.erase(
                std::remove_if(chords.begin(), chords.end(), [&](const Chord& c) {
                    return !HarmonyRules::checkFirstChordOfLastMeasureByFixedNote(
                        c, position.fixedNote, position, firstPosOfLastMeasure);
                }),
                chords.end());
        }
        if (position.index == lastPositionIndex && !position.fixedNote.isRest()) {
            bool anyValid = std::any_of(chords.begin(), chords.end(), [&](const Chord& c) {
                return HarmonyRules::checkFinalChordByFixedNote(
                    c, position.fixedNote, position, lastPositionIndex);
            });
            if (anyValid) {
                chords.erase(
                    std::remove_if(chords.begin(), chords.end(), [&](const Chord& c) {
                        return !HarmonyRules::checkFinalChordByFixedNote(
                            c, position.fixedNote, position, lastPositionIndex);
                    }),
                    chords.end());
            }
        }
        result.push_back(std::move(chords));
    }
    return result;
}

std::vector<Chord> MelodyHarmonizer::buildChordsForPosition(
    const HarmonicPosition& position,
    const HarmonizationSettings& settings)
{
    if (position.fixedNote.isRest())
        return chordBuilder.buildAllValid(settings);
    return chordBuilder.buildForFixedMelodyNote(position.fixedNote, settings);
}

std::vector<HarmonizationVariant> MelodyHarmonizer::buildVariantsFromPaths(
    const std::vector<HarmonyPath>& paths,
    const HarmonizationSettings& settings)
{
    std::vector<HarmonizationVariant> variants;
    for (const HarmonyPath& path : paths) {
        Score score;
        score.chords          = path.chords;
        score.positions       = path.positions;
        score.fixedVoiceIndex = 0;  // soprano is the fixed (melody) voice

        HarmonizationVariant variant;
        variant.musicScore = score;
        variant.score = path.score;

        if (variant.score == 0) {
            variant.score = variantScorer.score(variant, settings, HarmonizationMode::HarmonizeMelody);
        } else {
            variant.score -= variantScorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeMelody);
        }

        variants.push_back(std::move(variant));
    }
    return variants;
}
