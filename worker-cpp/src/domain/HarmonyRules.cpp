#include "domain/HarmonyRules.h"
#include "domain/ActiveRuleSet.h"
#include <cstdlib>

namespace {
bool isChromaticTransfer(const Note& previous, const Note& current) {
    return previous.getName() == current.getName() && previous.getAlter() != current.getAlter();
}

bool isStrictlyAscending(const Note& first, const Note& second, const Note& third) {
    return first < second && second < third;
}

bool isStrictlyDescending(const Note& first, const Note& second, const Note& third) {
    return first > second && second > third;
}

bool isSameDirection(const Note& previousFirst, const Note& currentFirst, const Note& previousSecond, const Note& currentSecond) {
    return (currentFirst > previousFirst && currentSecond > previousSecond) ||
           (currentFirst < previousFirst && currentSecond < previousSecond);
}

bool isBassAndSopranoSameDirection(const Chord& previous, const Chord& current) {
    return isSameDirection(previous.getSoprano(), current.getSoprano(), previous.getBass(), current.getBass());
}

bool hasIntervalNumber(const Note& first, const Note& second, int number) {
    return first.getInterval(second).number == number;
}

bool hasSimpleIntervalNumber(const Note& first, const Note& second, int number) {
    return first.getSimpleInterval(second).number == number;
}

int bassAbsoluteInterval(const Chord& a, const Chord& b) {
    return std::abs(a.getBass().getSemitone() - b.getBass().getSemitone());
}

bool isBassMotionNoMoreThanMajorSecond(const Chord& a, const Chord& b) {
    return bassAbsoluteInterval(a, b) <= 2;
}

bool isBassMotionUnison(const Chord& a, const Chord& b) {
    return bassAbsoluteInterval(a, b) == 0;
}

bool isBassMotionSecond(const Chord& a, const Chord& b) {
    int i = bassAbsoluteInterval(a, b);
    return i == 1 || i == 2;
}

bool isSeventhChordType(ChordType type) {
    return type == ChordType::Seventh ||
           type == ChordType::SixFive ||
           type == ChordType::FourThree ||
           type == ChordType::Two;
}

bool isTriadOrInversionType(ChordType type) {
    return type == ChordType::Triad ||
           type == ChordType::Six ||
           type == ChordType::SixFour;
}

bool isPerfectToDiminishedFifth(const Interval& previousInterval, const Interval& currentInterval) {
    return previousInterval.number == 5 &&
           currentInterval.number == 5 &&
           previousInterval.quality == IntervalQuality::Perfect &&
           currentInterval.quality == IntervalQuality::Diminished;
}

bool hasParallelSimpleIntervalPair(const Note& previousFirst, const Note& previousSecond, const Note& currentFirst, const Note& currentSecond, int intervalNumber) {
    Interval previousInterval = previousFirst.getSimpleInterval(previousSecond);
    Interval currentInterval = currentFirst.getSimpleInterval(currentSecond);

    if (previousInterval.number != intervalNumber) return false;
    if (currentInterval.number != intervalNumber) return false;

    bool firstMovesUp = currentFirst > previousFirst;
    bool firstMovesDown = currentFirst < previousFirst;
    bool secondMovesUp = currentSecond > previousSecond;
    bool secondMovesDown = currentSecond < previousSecond;

    bool bothMoveUp = firstMovesUp && secondMovesUp;
    bool bothMoveDown = firstMovesDown && secondMovesDown;

    if (!bothMoveUp && !bothMoveDown) return false;

    if (intervalNumber == 5 && isPerfectToDiminishedFifth(previousInterval, currentInterval)) {
        return false;
    }

    return true;
}

bool hasParallelSimpleInterval(const Chord& previous, const Chord& current, int intervalNumber) {
    Note previousNotes[] = {
        previous.getSoprano(),
        previous.getAlto(),
        previous.getTenor(),
        previous.getBass()
    };

    Note currentNotes[] = {
        current.getSoprano(),
        current.getAlto(),
        current.getTenor(),
        current.getBass()
    };

    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            if (hasParallelSimpleIntervalPair(previousNotes[i], previousNotes[j], currentNotes[i], currentNotes[j], intervalNumber)) {
                return true;
            }
        }
    }

    return false;
}
}

const VoiceRange SOPRANO_RANGE = {
    Note(NoteName::C, 4, 0, 1, 4, false),
    Note(NoteName::C, 6, 0, 1, 4, false)
};

const VoiceRange ALTO_RANGE = {
    Note(NoteName::G, 3, 0, 1, 4, false),
    Note(NoteName::D, 5, 0, 1, 4, false)
};

const VoiceRange TENOR_RANGE = {
    Note(NoteName::C, 3, 0, 1, 4, false),
    Note(NoteName::F, 4, 0, 1, 4, false)
};

const VoiceRange BASS_RANGE = {
    Note(NoteName::E, 2, 0, 1, 4, false),
    Note(NoteName::C, 4, 0, 1, 4, false)
};

bool VoiceRange::contains(const Note& note) const {
    return note >= min && note <= max;
}

// ── Rules-unaware public overloads (backward compat) ─────────────────────────

bool HarmonyRules::isValidChord(const Chord& current) {
    return isValidChord(current, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::isValidChord(const Chord& current, const HarmonicPosition& position) {
    return isValidChord(current, position, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::isValidConnection(const Chord& previous, const Chord& current) {
    return isValidConnection(previous, current, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::isValidConnection(const Chord& prePrevious, const Chord& previous, const Chord& current) {
    return isValidConnection(prePrevious, previous, current, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::isValidFunctionalProgression(const Chord& previous, const Chord& current) {
    return true;
    //return checkGeneralProgressionRules(previous, current, ActiveRuleSet::allEnabled());
}

// ── Rules-aware public overloads ──────────────────────────────────────────────

bool HarmonyRules::isValidChord(const Chord& current, const ActiveRuleSet& rules) {
    if (rules.voiceRanges && !hasValidVoiceRanges(current)) return false;
    if (!checkVoiceSpacingRules(current, rules))            return false;
    if (!hasValidSecondDegreeTriad(current))                return false;
    return true;
}

bool HarmonyRules::isValidChord(const Chord& current, const HarmonicPosition& position,
                                 const ActiveRuleSet& rules) {
    return isValidChord(current, rules) && checkSixFourBeatRule(current, position, rules);
}

bool HarmonyRules::isValidConnection(const Chord& previous, const Chord& current,
                                      const ActiveRuleSet& rules) {
    return isValidChord(current, rules)
        && checkVoiceLeadingRules(previous, current, rules)
        && checkGeneralProgressionRules(previous, current, rules);
}

bool HarmonyRules::isValidConnection(const Chord& prePrevious, const Chord& previous,
                                      const Chord& current, const ActiveRuleSet& rules) {
    if (rules.bassLeapSequence) {
        if (!hasNo2ConsecutiveFoursInBass(prePrevious, previous, current))  return false;
        if (!hasNo2ConsecutiveFifthsInBass(prePrevious, previous, current)) return false;
    }
    if (rules.functionalRules) {
        if (!checkSixFourTripleRules(prePrevious, previous, current)) return false;
    }
    return true;
}

bool HarmonyRules::isValidFunctionalProgression(const Chord& previous, const Chord& current,
                                                  const ActiveRuleSet& rules) {
    return checkGeneralProgressionRules(previous, current, rules);
}

bool HarmonyRules::checkVoiceRangeRules(const Chord& current) {
    return hasValidVoiceRanges(current);
}

bool HarmonyRules::checkVoiceSpacingRules(const Chord& current) {
    return checkVoiceSpacingRules(current, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::checkVoiceSpacingRules(const Chord& current, const ActiveRuleSet& rules) {
    if (rules.largeIntervalSaAt) {
        if (current.getSoprano().getInterval(current.getAlto()).number  > 8)  return false;
        if (current.getAlto()   .getInterval(current.getTenor()).number > 8)  return false;
    }
    if (rules.voiceSpacingOctave) {
        if (current.getTenor().getInterval(current.getBass()).number > 15) return false;
    }
    return true;
}

bool HarmonyRules::checkVoiceLeadingRules(const Chord& previous, const Chord& current) {
    return checkVoiceLeadingRules(previous, current, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::checkVoiceLeadingRules(const Chord& previous, const Chord& current,
                                           const ActiveRuleSet& rules) {
    if (rules.voiceCrossing     && !hasNoVoiceCrossing(previous, current))              return false;
    if (rules.allVoicesSameDir  && !hasNotAllVoicesInSameDirection(previous, current))  return false;
    if (rules.chromaticTransfer && !hasNoChromaticSemitoneTransfer(previous, current))  return false;
    if (rules.hiddenIntervals   && !hasNoHiddenOctaves(previous, current))              return false;
    if (rules.hiddenIntervals   && !hasNoHiddenFifths(previous, current))               return false;
    if (rules.parallelFifths    && !hasNoParallelFifths(previous, current))             return false;
    if (rules.parallelOctaves   && !hasNoParallelOctavesOrUnisons(previous, current))   return false;
    if (rules.parallelSeconds   && !hasNoParallelSeconds(previous, current))            return false;
    if (rules.parallelSeconds   && !hasNoParallelSeventh(previous, current))            return false;
    if (rules.augmentedBass   && !hasNoAugmentedInBass(previous, current))         return false;
    if (rules.voiceLeapLimits && !hasNoVoiceLeapGreaterThanCan(previous, current)) return false;
    return true;
}

bool HarmonyRules::checkGeneralProgressionRules(const Chord& previous, const Chord& current) {
    return checkGeneralProgressionRules(previous, current, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::checkGeneralProgressionRules(const Chord& previous, const Chord& current,
                                                  const ActiveRuleSet& rules) {
    if (!rules.functionalRules) return true;

    if (previous.getType() == ChordType::CadentialSixFour && !checkAfterCadentialSixFour(previous, current)) return false;
    if (current.getType() == ChordType::CadentialSixFour && !checkBeforeCadentialSixFour(previous, current)) return false;
    if (previous.getFunction() == HarmonicFunction::D && !checkAfterDominant(previous, current)) return false;
    if (previous.getFunction() == HarmonicFunction::S && !checkAfterSubdominant(previous, current)) return false;
    if (isSeventhChordType(previous.getType()) && !checkAfterSeventhChord(previous, current)) return false;
    if (!hasValidSecondDegreeTriad(current)) return false;
    if (!checkDoubledThirdInSixChord(previous, current)) return false;
    if (!checkDominantSeventhFourthStaysInSameVoice(previous, current)) return false;
    if (!checkSixFourBassMotion(previous, current)) return false;

    return true;
}

bool HarmonyRules::checkAfterCadentialSixFour(const Chord& previous, const Chord& current) {
    if (current.getFunction() == HarmonicFunction::S) return false;
    if (current.getFunction() == HarmonicFunction::T) return false;
    if (current.getDegree() != 5) return false;
    if (current.getType() != ChordType::Triad
        && current.getType() != ChordType::Seventh
        && current.getType() != ChordType::Ninth) return false;
    return true;
}

bool HarmonyRules::checkBeforeCadentialSixFour(const Chord& previous, const Chord& current) {
    return previous.getFunction() == HarmonicFunction::S;
}

bool HarmonyRules::checkAfterDominant(const Chord& previous, const Chord& current) {
    if (current.getFunction() == HarmonicFunction::S) return false;
    if (current.getDegree() == 3) return false;
    if (!checkAfterDominantTwo(previous, current)) return false;
    return true;
}

bool HarmonyRules::checkAfterSubdominant(const Chord& previous, const Chord& current) {
    if ((previous.getDegree() == 4 || previous.getDegree() == 2) && current.getDegree() == 6) return false;
    if (previous.getDegree() == 2 && current.getDegree() == 4) return false;
    return true;
}

bool HarmonyRules::checkAfterSeventhChord(const Chord& previous, const Chord& current) {
    return !isTriadOrInversionType(current.getType());
}

bool HarmonyRules::checkAfterDominantTwo(const Chord& previous, const Chord& current) {
    bool previousIsD2 = previous.getFunction() == HarmonicFunction::D &&
                        previous.getType() == ChordType::Two;

    if (!previousIsD2) return true;

    bool currentIsD2 = current.getFunction() == HarmonicFunction::D &&
                       current.getType() == ChordType::Two;

    bool currentIsT6 = current.getFunction() == HarmonicFunction::T &&
                       current.getType() == ChordType::Six;

    return currentIsD2 || currentIsT6;
}

bool HarmonyRules::hasValidSecondDegreeTriad(const Chord& current) {
    bool currentIsII53 = current.getDegree() == 2 &&
                         current.getType() == ChordType::Triad;

    if (!currentIsII53) return true;

    Note bass = current.getBass();

    Note upperVoices[] = {
        current.getSoprano(),
        current.getAlto(),
        current.getTenor()
    };

    for (const Note& note : upperVoices) {
        Interval interval = bass.getSimpleInterval(note);

        if (interval.number == 5 &&
            interval.quality == IntervalQuality::Diminished) {
            return false;
        }
    }

    return true;
}

bool HarmonyRules::hasValidVoiceRanges(const Chord& chord) {
    return SOPRANO_RANGE.contains(chord.getSoprano()) &&
           ALTO_RANGE.contains(chord.getAlto()) &&
           TENOR_RANGE.contains(chord.getTenor()) &&
           BASS_RANGE.contains(chord.getBass());
}

bool HarmonyRules::hasNoMoreThanOctave(const Chord& current) {
    int sopranoAlto = current.getSoprano().getInterval(current.getAlto()).number;
    int altoTenor = current.getAlto().getInterval(current.getTenor()).number;
    int tenorBass = current.getTenor().getInterval(current.getBass()).number;

    if (sopranoAlto > 8) return false;
    if (altoTenor > 8) return false;
    if (tenorBass > 15) return false;

    return true;
}

bool HarmonyRules::hasNoVoiceCrossing(const Chord& previous, const Chord& current) {
    if (current.getSoprano() < previous.getAlto()) return false;
    if (current.getAlto() < previous.getTenor()) return false;
    if (current.getTenor() < previous.getBass()) return false;
    if (previous.getSoprano() < current.getAlto()) return false;
    if (previous.getAlto() < current.getTenor()) return false;
    if (previous.getTenor() < current.getBass()) return false;

    return true;
}

bool HarmonyRules::hasNotAllVoicesInSameDirection(const Chord& previous, const Chord& current) {
    bool allUp = current.getSoprano() > previous.getSoprano() &&
                 current.getAlto() > previous.getAlto() &&
                 current.getTenor() > previous.getTenor() &&
                 current.getBass() > previous.getBass();

    bool allDown = current.getSoprano() < previous.getSoprano() &&
                   current.getAlto() < previous.getAlto() &&
                   current.getTenor() < previous.getTenor() &&
                   current.getBass() < previous.getBass();

    return !(allUp || allDown);
}

bool HarmonyRules::hasNoChromaticSemitoneTransfer(const Chord& previous, const Chord& current) {
    Note previousNotes[] = { previous.getSoprano(), previous.getAlto(), previous.getTenor(), previous.getBass() };
    Note currentNotes[] = { current.getSoprano(), current.getAlto(), current.getTenor(), current.getBass() };

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i != j && isChromaticTransfer(previousNotes[i], currentNotes[j])) {
                return false;
            }
        }
    }

    return true;
}

bool HarmonyRules::hasNo2ConsecutiveFoursInBass(const Chord& prePrevious, const Chord& previous, const Chord& current) {
    bool firstIsFourth = hasIntervalNumber(prePrevious.getBass(), previous.getBass(), 4);
    bool secondIsFourth = hasIntervalNumber(previous.getBass(), current.getBass(), 4);

    if (!firstIsFourth || !secondIsFourth) return true;

    return !isStrictlyAscending(prePrevious.getBass(), previous.getBass(), current.getBass()) &&
           !isStrictlyDescending(prePrevious.getBass(), previous.getBass(), current.getBass());
}

bool HarmonyRules::hasNo2ConsecutiveFifthsInBass(const Chord& prePrevious, const Chord& previous, const Chord& current) {
    bool firstIsFifth = hasIntervalNumber(prePrevious.getBass(), previous.getBass(), 5);
    bool secondIsFifth = hasIntervalNumber(previous.getBass(), current.getBass(), 5);

    if (!firstIsFifth || !secondIsFifth) return true;

    return !isStrictlyAscending(prePrevious.getBass(), previous.getBass(), current.getBass()) &&
           !isStrictlyDescending(prePrevious.getBass(), previous.getBass(), current.getBass());
}

bool HarmonyRules::hasNoHiddenOctaves(const Chord& previous, const Chord& current) {
    int intervalNumber = current.getBass().getSimpleInterval(current.getSoprano()).number;

    if (intervalNumber == 1 && isBassAndSopranoSameDirection(previous, current)) {
        return false;
    }

    return true;
}

bool HarmonyRules::hasNoHiddenFifths(const Chord& previous, const Chord& current) {
    int intervalNumber = current.getBass().getSimpleInterval(current.getSoprano()).number;

    if (intervalNumber == 5 && isBassAndSopranoSameDirection(previous, current)) {
        return false;
    }

    return true;
}

bool HarmonyRules::hasNoParallelFifths(const Chord& previous, const Chord& current) {
    return !hasParallelSimpleInterval(previous, current, 5);
}

bool HarmonyRules::hasNoParallelOctavesOrUnisons(const Chord& previous, const Chord& current) {
    return !hasParallelSimpleInterval(previous, current, 1);
}

bool HarmonyRules::hasNoParallelSeconds(const Chord& previous, const Chord& current) {
    return !hasParallelSimpleInterval(previous, current, 2);
}

bool HarmonyRules::hasNoParallelSeventh(const Chord& previous, const Chord& current) {
    return !hasParallelSimpleInterval(previous, current, 7);
}

bool HarmonyRules::hasNoAugmentedInBass(const Chord& previous, const Chord& current) {
    Interval interval = previous.getBass().getInterval(current.getBass());

    return interval.quality != IntervalQuality::Augmented;
}

int HarmonyRules::normalizeDegree(int degree) {
    while (degree > 7) degree -= 7;
    while (degree < 1) degree += 7;
    return degree;
}

bool HarmonyRules::containsDoubledThird(const Chord& chord) {
    int thirdDegree = normalizeDegree(chord.getDegree() + 2);
    int count = 0;
    for (int d : chord.getChordTemplate().degreesInSatbOrder) {
        if (normalizeDegree(d) == thirdDegree) ++count;
    }
    return count == 2;
}

bool HarmonyRules::checkDoubledThirdInSixChord(const Chord& previous, const Chord& current) {
    if (current.getType() != ChordType::Six) return true;
    if (!containsDoubledThird(current)) return true;

    // Exception 1: two six-chords in a row on a held bass
    if (previous.getType() == ChordType::Six
        && previous.getBass() == current.getBass()) {
        return true;
    }

    // Exception 2: held triad with bass moving to the third
    const bool sameChordFunction =
        previous.getFunction() == current.getFunction()
        && previous.getDegree() == current.getDegree();

    const bool triadToSix =
        previous.getType() == ChordType::Triad
        && current.getType() == ChordType::Six;

    const bool upperVoicesHeld =
        previous.getSoprano() == current.getSoprano()
        && previous.getAlto() == current.getAlto()
        && previous.getTenor() == current.getTenor();

    const bool onlyBassChanged =
        previous.getBass() != current.getBass();

    const bool bassMovedToThird =
        normalizeDegree(current.getBass().getDegree())
        == normalizeDegree(current.getDegree() + 2);

    if (sameChordFunction && triadToSix && upperVoicesHeld && onlyBassChanged && bassMovedToThird) {
        return true;
    }

    return false;
}

bool HarmonyRules::isDominantSeventhFamilyChord(const Chord& chord) {
    return chord.getFunction() == HarmonicFunction::D
        && chord.getDegree() == 5
        && (chord.getType() == ChordType::Seventh
            || chord.getType() == ChordType::SixFive
            || chord.getType() == ChordType::FourThree
            || chord.getType() == ChordType::Two);
}

int HarmonyRules::findVoiceWithDegree(const Chord& chord, int targetDegree) {
    const int target = normalizeDegree(targetDegree);

    if (normalizeDegree(chord.getSoprano().getDegree()) == target) return 0;
    if (normalizeDegree(chord.getAlto().getDegree())    == target) return 1;
    if (normalizeDegree(chord.getTenor().getDegree())   == target) return 2;
    if (normalizeDegree(chord.getBass().getDegree())    == target) return 3;

    return -1;
}

bool HarmonyRules::checkDominantSeventhFourthStaysInSameVoice(const Chord& previous, const Chord& current) {
    if (!isDominantSeventhFamilyChord(previous) || !isDominantSeventhFamilyChord(current)) {
        return true;
    }

    const int fourthDegree = 4;

    const int prevVoice = findVoiceWithDegree(previous, fourthDegree);
    const int currVoice = findVoiceWithDegree(current,  fourthDegree);

    if (prevVoice == -1) return true;
    if (currVoice == -1) return false;

    return prevVoice == currVoice;
}

bool HarmonyRules::isValidConnection(const Chord& previous, const Chord& current,
                                      const HarmonicPosition& previousPosition) {
    return isValidConnection(previous, current, previousPosition, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::isValidConnection(const Chord& previous, const Chord& current,
                                      const HarmonicPosition& previousPosition,
                                      const ActiveRuleSet& rules) {
    if (!isValidConnection(previous, current, rules)) return false;
    if (!checkDominantNinthToDominantSeventh(previous, current, previousPosition, rules)) return false;
    return true;
}

bool HarmonyRules::isValidConnection(const Chord& previous, const Chord& current,
                                      const HarmonicPosition& previousPosition,
                                      const HarmonicPosition& currentPosition) {
    return isValidConnection(previous, current, previousPosition, currentPosition, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::isValidConnection(const Chord& previous, const Chord& current,
                                      const HarmonicPosition& previousPosition,
                                      const HarmonicPosition& currentPosition,
                                      const ActiveRuleSet& rules) {
    if (!isValidConnection(previous, current, previousPosition, rules)) return false;
    if (!checkDominantToVI53(previous, current, currentPosition, rules)) return false;
    return true;
}

bool HarmonyRules::checkDominantToVI53(const Chord& previous, const Chord& current,
                                        const HarmonicPosition& currentPosition,
                                        const ActiveRuleSet& rules) {
    if (!rules.functionalRules) return true;
    return checkDominantToVI53(previous, current, currentPosition);
}

bool HarmonyRules::checkDominantToVI53(const Chord& previous, const Chord& current,
                                        const HarmonicPosition& currentPosition) {
    if (normalizeDegree(current.getDegree()) != 6) return true;
    if (current.getType() != ChordType::Triad) return true;
    if (previous.getFunction() != HarmonicFunction::D) return true;

    if (previous.getDegree() != 5) return false;
    if (previous.getType() != ChordType::Triad && previous.getType() != ChordType::Seventh) return false;
    if (!currentPosition.isStrongBeat) return false;
    return true;
}

bool HarmonyRules::checkDominantNinthToDominantSeventh(const Chord& previous, const Chord& current,
                                                        const HarmonicPosition& previousPosition,
                                                        const ActiveRuleSet& rules) {
    if (!rules.functionalRules) return true;
    return checkDominantNinthToDominantSeventh(previous, current, previousPosition);
}

bool HarmonyRules::checkDominantNinthToDominantSeventh(const Chord& previous, const Chord& current,
                                                        const HarmonicPosition& previousPosition) {
    if (previous.getType()     != ChordType::Ninth    ) return true;
    if (current.getType()      != ChordType::Seventh  ) return true;
    if (previous.getFunction() != HarmonicFunction::D ) return true;
    if (current.getFunction()  != HarmonicFunction::D ) return true;

    // 3.1 — D9 must be on a strong beat
    if (!previousPosition.isStrongBeat) return false;

    // 3.2 — at least two voices must stay on the same note
    int heldVoiceCount = 0;
    if (previous.getSoprano() == current.getSoprano()) ++heldVoiceCount;
    if (previous.getAlto()    == current.getAlto()   ) ++heldVoiceCount;
    if (previous.getTenor()   == current.getTenor()  ) ++heldVoiceCount;
    if (previous.getBass()    == current.getBass()   ) ++heldVoiceCount;
    if (heldVoiceCount < 2) return false;

    // 3.3 — the ninth (VI degree) must resolve down by step to V degree
    bool ninthResolved = false;
    if (normalizeDegree(previous.getSoprano().getDegree()) == 6
        && normalizeDegree(current.getSoprano().getDegree()) == 5) ninthResolved = true;
    if (normalizeDegree(previous.getAlto().getDegree())    == 6
        && normalizeDegree(current.getAlto().getDegree())   == 5) ninthResolved = true;
    if (normalizeDegree(previous.getTenor().getDegree())   == 6
        && normalizeDegree(current.getTenor().getDegree())  == 5) ninthResolved = true;
    if (normalizeDegree(previous.getBass().getDegree())    == 6
        && normalizeDegree(current.getBass().getDegree())   == 5) ninthResolved = true;
    if (!ninthResolved) return false;

    return true;
}

bool HarmonyRules::checkSixFourBassMotion(const Chord& previous, const Chord& current) {
    if (current.getType() == ChordType::SixFour
        && !isBassMotionNoMoreThanMajorSecond(previous, current)) return false;
    if (previous.getType() == ChordType::SixFour
        && !isBassMotionNoMoreThanMajorSecond(previous, current)) return false;
    return true;
}

bool HarmonyRules::checkSixFourTripleRules(const Chord& prePrevious, const Chord& previous, const Chord& current) {
    if (previous.getType() != ChordType::SixFour) return true;

    if (isBassMotionSecond(prePrevious, previous) && isBassMotionSecond(previous, current)) {
        bool ascending  = prePrevious.getBass().getSemitone() < previous.getBass().getSemitone()
                       && previous.getBass().getSemitone()    < current.getBass().getSemitone();
        bool descending = prePrevious.getBass().getSemitone() > previous.getBass().getSemitone()
                       && previous.getBass().getSemitone()    > current.getBass().getSemitone();
        if (!ascending && !descending) return false;
    }

    if ((isBassMotionUnison(prePrevious, previous) && isBassMotionSecond(previous, current))
        || (isBassMotionSecond(prePrevious, previous) && isBassMotionUnison(previous, current))) {
        return false;
    }

    return true;
}

bool HarmonyRules::checkFinalChordByFixedNote(const Chord& current, const Note& fixedNote,
                                               const HarmonicPosition& position, int lastPositionIndex) {
    if (position.index != lastPositionIndex) return true;

    const int fixedDegree = normalizeDegree(fixedNote.getDegree());

    if (fixedDegree == 1 || fixedDegree == 3 || fixedDegree == 5) {
        return current.getFunction() == HarmonicFunction::T
            && current.getDegree() == 1
            && current.getType() == ChordType::Triad;
    }

    if (fixedDegree == 6) {
        return current.getFunction() == HarmonicFunction::T
            && current.getDegree() == 6
            && current.getType() == ChordType::Triad;
    }

    return true;
}

bool HarmonyRules::checkInitialChordByFixedNote(const Chord& current, const Note& fixedNote,
                                                const HarmonicPosition& position, int firstPositionIndex) {
    if (position.index != firstPositionIndex) return true;

    const int fixedDegree = normalizeDegree(fixedNote.getDegree());

    if (fixedDegree == 1 || fixedDegree == 3 || fixedDegree == 5) {
        return current.getFunction() == HarmonicFunction::T
            && current.getDegree() == 1
            && current.getType() == ChordType::Triad;
    }

    return true;
}

bool HarmonyRules::checkInitialChordByBass(const Chord& current, const Note& fixedNote,
                                            const HarmonicPosition& position, int firstPositionIndex) {
    if (position.index != firstPositionIndex) return true;
    if (normalizeDegree(fixedNote.getDegree()) != 1) return true;
    return current.getFunction() == HarmonicFunction::T
        && current.getDegree() == 1
        && current.getType() == ChordType::Triad;
}

bool HarmonyRules::checkFinalChordByBass(const Chord& current, const Note& fixedNote,
                                          const HarmonicPosition& position, int lastPositionIndex) {
    if (position.index != lastPositionIndex) return true;
    if (normalizeDegree(fixedNote.getDegree()) != 1) return true;
    return current.getFunction() == HarmonicFunction::T
        && current.getDegree() == 1
        && current.getType() == ChordType::Triad;
}

bool HarmonyRules::checkSixFourBeatRule(const Chord& current, const HarmonicPosition& position) {
    return checkSixFourBeatRule(current, position, ActiveRuleSet::allEnabled());
}

bool HarmonyRules::checkSixFourBeatRule(const Chord& current, const HarmonicPosition& position,
                                         const ActiveRuleSet& rules) {
    if (!rules.functionalRules) return true;
    if (current.getType() == ChordType::SixFour
        && (position.isStrongBeat || position.isMediumBeat)) {
        return false;
    }
    if (current.getType() == ChordType::CadentialSixFour
        && position.isWeakBeat) {
        return false;
    }
    return true;
}

bool HarmonyRules::isLeapGreaterThanOctave(const Note& a, const Note& b) {
    return std::abs(a.getSemitone() - b.getSemitone()) > 12;
}

bool HarmonyRules::isLeapGreaterThanFourth(const Note& a, const Note& b) {
    return std::abs(a.getSemitone() - b.getSemitone()) > 5;
}

bool HarmonyRules::hasNoVoiceLeapGreaterThanCan(const Chord& previous, const Chord& current) {
    return !isLeapGreaterThanOctave(previous.getSoprano(), current.getSoprano())
        && !isLeapGreaterThanFourth(previous.getAlto(), current.getAlto())
        && !isLeapGreaterThanFourth(previous.getTenor(), current.getTenor())
        && !isLeapGreaterThanOctave(previous.getBass(), current.getBass());
}