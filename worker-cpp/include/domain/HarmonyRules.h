#ifndef HARM_HARMONYRULES_H
#define HARM_HARMONYRULES_H

#include "Chord.h"
#include "domain/HarmonicPosition.h"
#include "domain/ActiveRuleSet.h"

struct VoiceRange {
    Note min;
    Note max;

    bool contains(const Note& note) const;
};

// Standard SATB voice ranges — defined in HarmonyRules.cpp, used by ChordBuilder.
extern const VoiceRange SOPRANO_RANGE;
extern const VoiceRange ALTO_RANGE;
extern const VoiceRange TENOR_RANGE;
extern const VoiceRange BASS_RANGE;

class HarmonyRules {
public:
    // ── Rules-unaware overloads (backward compat, call rules-aware with allEnabled) ──
    static bool isValidConnection(const Chord& previous, const Chord& current);
    static bool isValidConnection(const Chord& previous, const Chord& current, const HarmonicPosition& previousPosition);
    static bool isValidConnection(const Chord& previous, const Chord& current,
                                  const HarmonicPosition& previousPosition, const HarmonicPosition& currentPosition);
    static bool isValidConnection(const Chord& prePrevious, const Chord& previous, const Chord& current);
    static bool isValidChord(const Chord& current);
    static bool isValidChord(const Chord& current, const HarmonicPosition& position);

    // ── Rules-aware overloads (used by HarmonyGraph and CheckSolutionRuleChecker) ──
    static bool isValidChord(const Chord& current, const ActiveRuleSet& rules);
    static bool isValidChord(const Chord& current, const HarmonicPosition& position, const ActiveRuleSet& rules);
    static bool isValidConnection(const Chord& previous, const Chord& current, const ActiveRuleSet& rules);
    static bool isValidConnection(const Chord& previous, const Chord& current,
                                  const HarmonicPosition& previousPosition, const ActiveRuleSet& rules);
    static bool isValidConnection(const Chord& previous, const Chord& current,
                                  const HarmonicPosition& previousPosition,
                                  const HarmonicPosition& currentPosition, const ActiveRuleSet& rules);
    static bool isValidConnection(const Chord& prePrevious, const Chord& previous,
                                  const Chord& current, const ActiveRuleSet& rules);

    // Diagnostic helper: checks only functional-progression rules (not voice-leading).
    static bool isValidFunctionalProgression(const Chord& previous, const Chord& current);
    static bool isValidFunctionalProgression(const Chord& previous, const Chord& current, const ActiveRuleSet& rules);

    static bool checkFinalChordByFixedNote(const Chord& current, const Note& fixedNote,
                                           const HarmonicPosition& position, int lastPositionIndex);
    static bool checkInitialChordByFixedNote(const Chord& current, const Note& fixedNote,
                                             const HarmonicPosition& position, int firstPositionIndex);
    static bool checkFinalChordByBass(const Chord& current, const Note& fixedNote,
                                      const HarmonicPosition& position, int lastPositionIndex);
    static bool checkInitialChordByBass(const Chord& current, const Note& fixedNote,
                                        const HarmonicPosition& position, int firstPositionIndex);

    // First position of the last measure: constrains chord choice based on the
    // fixed voice degree (soprano or bass) at that position.
    static bool checkFirstChordOfLastMeasureByFixedNote(const Chord& current, const Note& fixedNote,
                                                         const HarmonicPosition& position,
                                                         int firstPositionOfLastMeasureIndex);
    static bool checkFirstChordOfLastMeasureByBass(const Chord& current, const Note& fixedNote,
                                                    const HarmonicPosition& position,
                                                    int firstPositionOfLastMeasureIndex);

private:
    static bool checkVoiceRangeRules(const Chord& current);
    static bool checkVoiceSpacingRules(const Chord& current);
    static bool checkVoiceSpacingRules(const Chord& current, const ActiveRuleSet& rules);
    static bool checkVoiceLeadingRules(const Chord& previous, const Chord& current);
    static bool checkVoiceLeadingRules(const Chord& previous, const Chord& current, const ActiveRuleSet& rules);
    static bool checkGeneralProgressionRules(const Chord& previous, const Chord& current);
    static bool checkGeneralProgressionRules(const Chord& previous, const Chord& current, const ActiveRuleSet& rules);
    static bool checkAfterCadentialSixFour(const Chord& previous, const Chord& current);
    static bool checkBeforeCadentialSixFour(const Chord& previous, const Chord& current);
    static bool checkAfterDominant(const Chord& previous, const Chord& current);
    static bool checkAfterSubdominant(const Chord& previous, const Chord& current);
    static bool checkAfterSeventhChord(const Chord& previous, const Chord& current);
    static bool checkAfterDominantTwo(const Chord& previous, const Chord& current);
    static bool hasValidSecondDegreeTriad(const Chord& current);
    static bool checkSixFourBeatRule(const Chord& current, const HarmonicPosition& position);
    static bool checkSixFourBeatRule(const Chord& current, const HarmonicPosition& position, const ActiveRuleSet& rules);
    static bool checkSixFourBassMotion(const Chord& previous, const Chord& current);
    static bool checkSixFourTripleRules(const Chord& prePrevious, const Chord& previous, const Chord& current);
    static bool checkDominantNinthToDominantSeventh(const Chord& previous, const Chord& current,
                                                    const HarmonicPosition& previousPosition);
    static bool checkDominantNinthToDominantSeventh(const Chord& previous, const Chord& current,
                                                    const HarmonicPosition& previousPosition,
                                                    const ActiveRuleSet& rules);
    static bool checkDominantToVI53(const Chord& previous, const Chord& current, const HarmonicPosition& currentPosition);
    static bool checkDominantToVI53(const Chord& previous, const Chord& current,
                                    const HarmonicPosition& currentPosition, const ActiveRuleSet& rules);

    static bool hasValidVoiceRanges(const Chord& chord);
    static bool hasNoMoreThanOctave(const Chord& current);
    static bool hasNoVoiceCrossing(const Chord& previous, const Chord& current);
    static bool hasNotAllVoicesInSameDirection(const Chord& previous, const Chord& current);
    static bool hasNoChromaticSemitoneTransfer(const Chord& previous, const Chord& current);
    static bool hasNo2ConsecutiveFoursInBass(const Chord& prePrevious, const Chord& previous, const Chord& current);
    static bool hasNo2ConsecutiveFifthsInBass(const Chord& prePrevious, const Chord& previous, const Chord& current);
    static bool hasNoHiddenOctaves(const Chord& previous, const Chord& current);
    static bool hasNoHiddenFifths(const Chord& previous, const Chord& current);
    static bool hasNoParallelFifths(const Chord& previous, const Chord& current);
    static bool hasNoParallelOctavesOrUnisons(const Chord& previous, const Chord& current);
    static bool hasNoParallelSeconds(const Chord& previous, const Chord& current);
    static bool hasNoParallelSeventh(const Chord& previous, const Chord& current);
    static bool hasNoAugmentedInBass(const Chord& previous, const Chord& current);
    static bool hasNoVoiceLeapGreaterThanCan(const Chord& previous, const Chord& current);
    static bool isLeapGreaterThanOctave(const Note& a, const Note& b);
    static bool isLeapGreaterThanFourth(const Note& a, const Note& b);
    static bool checkDoubledThirdInSixChord(const Chord& previous, const Chord& current);
    static bool containsDoubledThird(const Chord& chord);
    static int normalizeDegree(int degree);
    static bool checkDominantSeventhFourthStaysInSameVoice(const Chord& previous, const Chord& current);
    static bool isDominantSeventhFamilyChord(const Chord& chord);
    static int findVoiceWithDegree(const Chord& chord, int targetDegree);
};

#endif