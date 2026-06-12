#ifndef HARM_HARMONICRHYTHMPLAN_H
#define HARM_HARMONICRHYTHMPLAN_H

#include <vector>
#include "domain/HarmonicSegment.h"

// One complete assignment of HarmonicSegments to the input notes.
// HarmonicRhythmPlanner may produce several HarmonicRhythmPlans per input
// (e.g. with different cadence split patterns); the harmonizer can then
// run the chord-search independently for each plan and rank the results.
struct HarmonicRhythmPlan {
    std::vector<HarmonicSegment> segments;
    // Accumulated penalty from getAllowedSplitPatterns pattern indices.
    // Pattern index k at a branching span adds k*10 to the score.
    // Lower score = higher priority (plan 0 wins over plan 1 etc.).
    int priorityScore = 0;
};

#endif
