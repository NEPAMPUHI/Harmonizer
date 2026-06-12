#ifndef HARM_HARMONIZATIONVARIANT_H
#define HARM_HARMONIZATIONVARIANT_H

#include "Score.h"

struct HarmonizationVariant {
    Score musicScore;
    int   score              = 0;
    int   rhythmPlanIndex    = -1;  // index into HarmonicRhythmPlanner::buildPlans() output
    int   rhythmPlanPriority = 0;   // carried from HarmonicRhythmPlan::priorityScore; lower = better
};

#endif
