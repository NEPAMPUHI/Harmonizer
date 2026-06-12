#ifndef HARM_VARIANTSCORER_H
#define HARM_VARIANTSCORER_H

#include <vector>
#include "domain/HarmonizationVariant.h"
#include "domain/HarmonizationSettings.h"
#include "domain/HarmonizationJob.h"
#include "domain/Chord.h"

constexpr int MAX_VARIANTS_TO_FRONTEND = 30;

class VariantScorer {
public:
    int score(const HarmonizationVariant& variant, const HarmonizationSettings& settings,
              HarmonizationMode mode) const;
    void sortVariants(std::vector<HarmonizationVariant>& variants,
                      const HarmonizationSettings& settings) const;
    void applyFinalRanking(std::vector<HarmonizationVariant>& variants) const;

    int calculateMelodicLinePenalty(const HarmonizationVariant& variant, HarmonizationMode mode) const;

private:
    int calculateSmoothnessScore(const HarmonizationVariant& variant) const;
    int calculateFunctionalScore(const HarmonizationVariant& variant) const;
    int calculateOuterLinePenalty(const Chord& previous, const Chord& current, HarmonizationMode mode) const;
    int calculateInnerVoicePenalty(const Chord& previous, const Chord& current) const;
    int getPenaltyForInterval(const Interval& interval, bool isInnerVoice) const;
};

#endif
