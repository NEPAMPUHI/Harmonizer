#ifndef HARM_HARMONICPOSITIONBUILDER_H
#define HARM_HARMONICPOSITIONBUILDER_H

#include <vector>
#include "HarmonicPosition.h"
#include "HarmonicSegment.h"
#include "HarmonizationSettings.h"
#include "HarmonizationJob.h"

class HarmonicPositionBuilder {
public:
    std::vector<HarmonicPosition> build(const std::vector<HarmonicSegment>& segments,
                                        const HarmonizationSettings& settings,
                                        HarmonizationMode mode);
};

#endif
