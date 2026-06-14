#ifndef HARM_HARMONICRHYTHMPLANNER_H
#define HARM_HARMONICRHYTHMPLANNER_H

#include <vector>
#include "domain/Note.h"
#include "domain/HarmonicSegment.h"
#include "domain/HarmonizationSettings.h"
#include "harmonization/HarmonicSplitPattern.h"
#include "harmonization/HarmonicRhythmPlan.h"

class HarmonicRhythmPlanner {
public:
    // Returns one segment list derived from notes using the highest-priority
    // split pattern at each position.  Backward-compatible entry point used
    // by the current harmonizer pipeline.
    std::vector<HarmonicSegment> buildSegments(
        const std::vector<Note>& notes,
        const HarmonizationSettings& settings);

    // Returns exactly one HarmonicRhythmPlan: each note (or tied-note span)
    // maps to one or more segments using the highest-priority split pattern.
    // For a whole note in 4/4 at a measure boundary this is [4,4,8];
    // for all other notes the segment equals the note duration.
    std::vector<HarmonicRhythmPlan> buildPlans(
        const std::vector<Note>& notes,
        const HarmonizationSettings& settings);

    // Returns the ordered list of allowed split patterns for a note of the given
    // duration starting at absoluteStartSixteenths.  The first entry is the most
    // preferred pattern; callers that produce a single plan use patterns[0].
    static std::vector<HarmonicSplitPattern> getAllowedSplitPatterns(
        int durationSixteenths,
        int absoluteStartSixteenths,
        const HarmonizationSettings& settings);
};

#endif
