#include "harmonization/VariantScorer.h"
#include <algorithm>

int VariantScorer::score(const HarmonizationVariant& variant,
                          const HarmonizationSettings& /*settings*/,
                          HarmonizationMode mode) const {
    int base = calculateSmoothnessScore(variant) + calculateFunctionalScore(variant);
    return base - calculateMelodicLinePenalty(variant, mode);
}

void VariantScorer::sortVariants(std::vector<HarmonizationVariant>& variants,
                                  const HarmonizationSettings& /*settings*/) const {
    std::sort(variants.begin(), variants.end(),
        [](const HarmonizationVariant& a, const HarmonizationVariant& b) {
            return a.score > b.score;
        });
}

void VariantScorer::applyFinalRanking(std::vector<HarmonizationVariant>& variants) const {
    std::sort(variants.begin(), variants.end(),
        [](const HarmonizationVariant& a, const HarmonizationVariant& b) {
            return a.score > b.score;
        });
    if (variants.size() > static_cast<size_t>(MAX_VARIANTS_TO_FRONTEND))
        variants.resize(static_cast<size_t>(MAX_VARIANTS_TO_FRONTEND));
}

int VariantScorer::calculateSmoothnessScore(const HarmonizationVariant& /*variant*/) const {
    // TODO: evaluate voice-leading smoothness between consecutive chords
    return 50;
}

int VariantScorer::calculateFunctionalScore(const HarmonizationVariant& /*variant*/) const {
    // TODO: evaluate harmonic functional logic (T→S→D→T progressions, etc.)
    return 50;
}

int VariantScorer::calculateMelodicLinePenalty(const HarmonizationVariant& variant,
                                                HarmonizationMode mode) const {
    const auto& chords = variant.musicScore.chords;
    if (chords.size() < 2) return 0;

    int totalPenalty = 0;
    for (size_t i = 1; i < chords.size(); ++i) {
        totalPenalty += calculateOuterLinePenalty(chords[i - 1], chords[i], mode);
        totalPenalty += calculateInnerVoicePenalty(chords[i - 1], chords[i]);
    }
    return totalPenalty;
}

int VariantScorer::calculateOuterLinePenalty(const Chord& previous, const Chord& current,
                                              HarmonizationMode mode) const {
    Note prev = (mode == HarmonizationMode::HarmonizeMelody)
                    ? previous.getBass()
                    : previous.getSoprano();
    Note curr = (mode == HarmonizationMode::HarmonizeMelody)
                    ? current.getBass()
                    : current.getSoprano();
    return getPenaltyForInterval(prev.getInterval(curr), false);
}

int VariantScorer::calculateInnerVoicePenalty(const Chord& previous, const Chord& current) const {
    int penalty = 0;
    if (previous.getAlto().getInterval(current.getAlto()).quality == IntervalQuality::Augmented)
        penalty += 20;
    if (previous.getTenor().getInterval(current.getTenor()).quality == IntervalQuality::Augmented)
        penalty += 20;
    return penalty;
}

int VariantScorer::getPenaltyForInterval(const Interval& interval, bool isInnerVoice) const {
    if (interval.quality == IntervalQuality::Augmented)
        return isInnerVoice ? 20 : 10;
    if (!isInnerVoice) {
        if (interval.number == 4) return 1;
        if (interval.number == 5) return 2;
        if (interval.number == 6) return 3;
        if (interval.number == 7) return 5;
        if (interval.number == 8) return 4;
    }
    return 0;
}
