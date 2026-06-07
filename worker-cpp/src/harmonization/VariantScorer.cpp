#include "harmonization/VariantScorer.h"
#include <algorithm>

int VariantScorer::score(const HarmonizationVariant& variant,
                          const HarmonizationSettings& /*settings*/) const {
    return calculateSmoothnessScore(variant) + calculateFunctionalScore(variant);
}

void VariantScorer::sortVariants(std::vector<HarmonizationVariant>& variants,
                                  const HarmonizationSettings& settings) const {
    std::sort(variants.begin(), variants.end(),
        [&](const HarmonizationVariant& a, const HarmonizationVariant& b) {
            return a.score > b.score;
        });
}

int VariantScorer::calculateSmoothnessScore(const HarmonizationVariant& /*variant*/) const {
    // TODO: evaluate voice-leading smoothness between consecutive chords
    return 50;
}

int VariantScorer::calculateFunctionalScore(const HarmonizationVariant& /*variant*/) const {
    // TODO: evaluate harmonic functional logic (T→S→D→T progressions, etc.)
    return 50;
}
