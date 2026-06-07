#ifndef HARM_HARMONYRULES_H
#define HARM_HARMONYRULES_H

#include "Chord.h"

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
    static bool isValidConnection(const Chord& previous, const Chord& current);
    static bool isValidConnection(const Chord& prePrevious, const Chord& previous, const Chord& current);
    static bool isValidChord(const Chord& current);

private:
    static bool checkVoiceRangeRules(const Chord& current);
    static bool checkVoiceSpacingRules(const Chord& current);
    static bool checkVoiceLeadingRules(const Chord& previous, const Chord& current);
    static bool checkGeneralProgressionRules(const Chord& previous, const Chord& current);
    static bool checkAfterCadentialSixFour(const Chord& previous, const Chord& current);
    static bool checkBeforeCadentialSixFour(const Chord& previous, const Chord& current);
    static bool checkAfterDominant(const Chord& previous, const Chord& current);
    static bool checkAfterSubdominant(const Chord& previous, const Chord& current);
    static bool checkAfterSeventhChord(const Chord& previous, const Chord& current);
    static bool checkAfterDominantTwo(const Chord& previous, const Chord& current);
    static bool hasValidSecondDegreeTriad(const Chord& current);

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
};

#endif