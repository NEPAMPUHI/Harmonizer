#ifndef HARM_HARMONICPOSITIONBUILDER_H
#define HARM_HARMONICPOSITIONBUILDER_H

#include <vector>
#include "HarmonicPosition.h"
#include "ScoreInput.h"
#include "HarmonizationSettings.h"
#include "HarmonizationJob.h"

class HarmonicPositionBuilder {
public:
    std::vector<HarmonicPosition> build(const ScoreInput& input, const HarmonizationSettings& settings, HarmonizationMode mode);
};

#endif