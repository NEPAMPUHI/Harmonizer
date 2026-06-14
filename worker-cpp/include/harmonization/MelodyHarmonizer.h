#ifndef HARM_MELODYHARMONIZER_H
#define HARM_MELODYHARMONIZER_H

#include <vector>
#include "domain/Chord.h"
#include "domain/HarmonizationSettings.h"
#include "domain/HarmonizationVariant.h"
#include "domain/Note.h"
#include "domain/ScoreInput.h"
#include "domain/HarmonicPosition.h"
#include "domain/HarmonicPositionBuilder.h"
#include "harmonization/HarmonicRhythmPlanner.h"
#include "harmonization/HarmonicRhythmPlan.h"
#include "harmonization/ChordBuilder.h"
#include "harmonization/HarmonyGraph.h"
#include "harmonization/HarmonyVariantPlanner.h"
#include "harmonization/VariantScorer.h"

class MelodyHarmonizer {
public:
    std::vector<HarmonizationVariant> harmonize(const ScoreInput& input,
                                                const HarmonizationSettings& settings);

private:
    std::vector<HarmonizationVariant> harmonizePlan(
        const HarmonicRhythmPlan& plan,
        const HarmonizationSettings& settings);

    std::vector<std::vector<Chord>> buildChordsByPosition(
        const std::vector<HarmonicPosition>& positions,
        const HarmonizationSettings& settings);
    std::vector<Chord> buildChordsForPosition(const HarmonicPosition& position,
                                              const HarmonizationSettings& settings);
    std::vector<HarmonizationVariant> buildVariantsFromPaths(
        const std::vector<HarmonyPath>& paths,
        const HarmonizationSettings& settings);

    ChordBuilder chordBuilder;
    VariantScorer variantScorer;
    HarmonyGraph harmonyGraph;
    HarmonyVariantPlanner harmonyVariantPlanner;
    HarmonicRhythmPlanner harmonicRhythmPlanner;
    HarmonicPositionBuilder harmonicPositionBuilder;
};

#endif
