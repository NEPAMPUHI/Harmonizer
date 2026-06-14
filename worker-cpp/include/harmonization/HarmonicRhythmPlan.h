#ifndef HARM_HARMONICRHYTHMPLAN_H
#define HARM_HARMONICRHYTHMPLAN_H

#include <vector>
#include "domain/HarmonicSegment.h"

// One complete assignment of HarmonicSegments to the input notes.
// HarmonicRhythmPlanner always produces exactly one plan using the
// highest-priority split pattern at each position.
struct HarmonicRhythmPlan {
    std::vector<HarmonicSegment> segments;
};

#endif
