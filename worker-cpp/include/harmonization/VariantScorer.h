#ifndef HARM_VARIANTSCORER_H
#define HARM_VARIANTSCORER_H

#include <vector>
#include "domain/HarmonizationVariant.h"
#include "domain/HarmonizationSettings.h"

class VariantScorer {
public:
    int score(const HarmonizationVariant& variant, const HarmonizationSettings& settings) const;
    void sortVariants(std::vector<HarmonizationVariant>& variants,
                      const HarmonizationSettings& settings) const;

private:
    int calculateSmoothnessScore(const HarmonizationVariant& variant) const;
    int calculateFunctionalScore(const HarmonizationVariant& variant) const;
};

#endif
