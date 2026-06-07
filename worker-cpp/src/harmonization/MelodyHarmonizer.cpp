#include "harmonization/MelodyHarmonizer.h"
#include "harmonization/NoteDegreesResolver.h"

std::vector<HarmonizationVariant> MelodyHarmonizer::harmonize(const ScoreInput& input,
                                                               const HarmonizationSettings& settings) {
    auto positions = harmonicPositionBuilder.build(input, settings, HarmonizationMode::HarmonizeMelody);
    if (positions.empty()) return {};

    NoteDegreesResolver{}.resolveInPlace(positions, settings);

    auto chordsByPosition = buildChordsByPosition(positions, settings);
    harmonyGraph.build(chordsByPosition);
    harmonyVariantPlanner.setGraphPositions(positions);

    if (harmonyGraph.empty()) return {};
    for (const auto& level : harmonyGraph.getLevels()) {
        if (level.empty()) return {};
    }

    size_t maxVariants = harmonyGraph.getMaxDistinctChordNameCount();
    if (maxVariants == 0) return {};

    auto paths = harmonyVariantPlanner.buildDiversePaths(harmonyGraph, maxVariants);
    auto variants = buildVariantsFromPaths(paths, settings);
    variantScorer.sortVariants(variants, settings);
    return variants;
}

std::vector<std::vector<Chord>> MelodyHarmonizer::buildChordsByPosition(
    const std::vector<HarmonicPosition>& positions,
    const HarmonizationSettings& settings)
{
    std::vector<std::vector<Chord>> result;
    result.reserve(positions.size());
    for (const auto& position : positions) {
        result.push_back(buildChordsForPosition(position, settings));
    }
    return result;
}

std::vector<Chord> MelodyHarmonizer::buildChordsForPosition(
    const HarmonicPosition& position,
    const HarmonizationSettings& settings)
{
    return chordBuilder.buildForFixedMelodyNote(position.fixedNote, settings);
}

std::vector<HarmonizationVariant> MelodyHarmonizer::buildVariantsFromPaths(
    const std::vector<HarmonyPath>& paths,
    const HarmonizationSettings& settings)
{
    std::vector<HarmonizationVariant> variants;
    for (const HarmonyPath& path : paths) {
        Score score;
        score.chords = path.chords;

        score.positions = path.positions;

        HarmonizationVariant variant;
        variant.musicScore = score;
        variant.score = path.score;

        if (variant.score == 0) {
            variant.score = variantScorer.score(variant, settings);
        }

        variants.push_back(std::move(variant));
    }
    return variants;
}
