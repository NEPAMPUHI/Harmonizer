#ifndef HARM_CHECKERROR_H
#define HARM_CHECKERROR_H

#include <string>
#include <vector>

enum class CheckErrorCode {
    // Chord identity
    UnknownChord,
    // Single-chord structural
    VoiceRangeViolation,
    MoreThanOctaveBetweenAdjacentVoices,
    // Pairwise — voice crossing / direction
    VoiceCrossing,
    AllVoicesSameDirection,
    // Pairwise — chromatic
    ChromaticSemitoneTransfer,
    // Pairwise — hidden intervals (soprano+bass)
    HiddenOctaves,
    HiddenFifths,
    // Pairwise — parallel intervals (all voice pairs)
    ParallelFifths,
    ParallelOctaves,           // semitone-based legacy check
    ParallelOctavesOrUnisons,  // diatonic-based (mirrors HarmonyRules)
    ParallelSeconds,
    ParallelSevenths,
    // Pairwise — other voice-leading
    AugmentedIntervalInBass,
    VoiceLeapGreaterThanOctave,
    // Triple-chord — bass line
    ConsecutiveFourthsInBass,
    ConsecutiveFifthsInBass,
    // Functional progression
    FunctionalProgressionError
};

enum class CheckRenderType {
    ChordMarker,
    VerticalBracket,
    VerticalBracketPair,
    MotionLines,
    CrossingLines,
    HiddenInterval,
    ChromaticTransfer,
    BassLineMarker,
    FunctionalRelationMarker
};

enum class VoiceType { Soprano, Alto, Tenor, Bass };

struct CheckError {
    CheckErrorCode code = CheckErrorCode::UnknownChord;
    std::string    message;

    int positionIndex     = -1;
    int nextPositionIndex = -1;

    std::vector<VoiceType> voices;
    int interval = 0;

    CheckRenderType renderType = CheckRenderType::ChordMarker;
};

#endif
