#ifndef HARM_ACTIVERULESSET_H
#define HARM_ACTIVERULESSET_H

#include "domain/HarmonizationSettings.h"

// Parsed from HarmonizationSettings::forbiddenRules.
//
// Naming note: the frontend sends `forbiddenRules` as a list of rule IDs that
// ARE currently active (enforced). The initial state has all IDs present
// (all rules checked); unchecking a rule removes its ID from the list. An ID
// present in forbiddenRules means "enforce this rule"; an absent ID means
// "skip this rule". ActiveRuleSet converts that list into plain bool flags.
struct ActiveRuleSet {
    bool parallelFifths    = true;   // "parallel_fifths"
    bool parallelOctaves   = true;   // "parallel_octaves"     (octaves + unisons)
    bool parallelSeconds   = true;   // "parallel_seconds"     (seconds + sevenths)
    bool allVoicesSameDir  = true;   // "all_voices_same_dir"
    bool voiceCrossing     = true;   // "voice_crossing"
    bool chromaticTransfer = true;   // "chromatic_transfer"
    bool hiddenIntervals   = true;   // "hidden_octaves"       (octaves + fifths)
    bool bassLeapSequence  = true;   // "bass_leap_sequence"   (consecutive 4ths/5ths)
    bool largeIntervalSaAt = true;   // "large_interval_sa_at" (S-A, A-T > octave)

    // Functional progression — umbrella switch for the entire
    // checkGeneralProgressionRules() block plus all position-aware functional
    // checks: K6/4 beat rule, D9→D7 resolution, D→VI53 strong-beat requirement.
    // UI label: "S після D"; historically names only subdominant-after-dominant,
    // but covers all functional harmony rules that have no separate UI entry.
    bool functionalRules   = true;   // "s_after_d"

    static ActiveRuleSet fromSettings(const HarmonizationSettings& settings);
    static ActiveRuleSet allEnabled();
    static ActiveRuleSet allDisabled();
};

#endif
