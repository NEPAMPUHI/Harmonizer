#include "domain/HarmonyRules.h"

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
    Note(NoteName::C, 4, 0, 1, Duration{}, false),
    Note(NoteName::C, 6, 0, 1, Duration{}, false)
};

const VoiceRange ALTO_RANGE = {
    Note(NoteName::G, 3, 0, 1, Duration{}, false),
    Note(NoteName::D, 5, 0, 1, Duration{}, false)
};

const VoiceRange TENOR_RANGE = {
    Note(NoteName::C, 3, 0, 1, Duration{}, false),
    Note(NoteName::F, 4, 0, 1, Duration{}, false)
};

const VoiceRange BASS_RANGE = {
    Note(NoteName::E, 2, 0, 1, Duration{}, false),
    Note(NoteName::C, 4, 0, 1, Duration{}, false)
};

bool VoiceRange::contains(const Note& note) const {
    return note >= min && note <= max;
}

bool HarmonyRules::isValidChord(const Chord& current) {
    return checkVoiceRangeRules(current) && checkVoiceSpacingRules(current) && hasValidSecondDegreeTriad(current);
}

bool HarmonyRules::isValidConnection(const Chord& previous, const Chord& current) {
    return isValidChord(current) &&
           checkVoiceLeadingRules(previous, current) &&
           checkGeneralProgressionRules(previous, current);
}

bool HarmonyRules::isValidConnection(const Chord& prePrevious, const Chord& previous, const Chord& current) {
    return hasNo2ConsecutiveFoursInBass(prePrevious, previous, current) &&
           hasNo2ConsecutiveFifthsInBass(prePrevious, previous, current);
}

bool HarmonyRules::checkVoiceRangeRules(const Chord& current) {
    return hasValidVoiceRanges(current);
}

bool HarmonyRules::checkVoiceSpacingRules(const Chord& current) {
    return hasNoMoreThanOctave(current);
}

bool HarmonyRules::checkVoiceLeadingRules(const Chord& previous, const Chord& current) {
    return hasNoVoiceCrossing(previous, current) &&
           hasNotAllVoicesInSameDirection(previous, current) &&
           hasNoChromaticSemitoneTransfer(previous, current) &&
           hasNoHiddenOctaves(previous, current) &&
           hasNoHiddenFifths(previous, current) &&
           hasNoParallelFifths(previous, current) &&
           hasNoParallelOctavesOrUnisons(previous, current) &&
           hasNoParallelSeconds(previous, current) &&
           hasNoParallelSeventh(previous, current) &&
           hasNoAugmentedInBass(previous, current);
}

bool HarmonyRules::checkGeneralProgressionRules(const Chord& previous, const Chord& current) {
    if (previous.getType() == ChordType::CadentialSixFour && !checkAfterCadentialSixFour(previous, current)) return false;
    if (current.getType() == ChordType::CadentialSixFour && !checkBeforeCadentialSixFour(previous, current)) return false;
    if (previous.getFunction() == HarmonicFunction::D && !checkAfterDominant(previous, current)) return false;
    if (previous.getFunction() == HarmonicFunction::S && !checkAfterSubdominant(previous, current)) return false;
    if (isSeventhChordType(previous.getType()) && !checkAfterSeventhChord(previous, current)) return false;
    if (!hasValidSecondDegreeTriad(current)) return false;

    return true;
}

bool HarmonyRules::checkAfterCadentialSixFour(const Chord& previous, const Chord& current) {
    return current.getFunction() != HarmonicFunction::S &&
           current.getFunction() != HarmonicFunction::T;
}

bool HarmonyRules::checkBeforeCadentialSixFour(const Chord& previous, const Chord& current) {
    return previous.getFunction() != HarmonicFunction::D;
}

bool HarmonyRules::checkAfterDominant(const Chord& previous, const Chord& current) {
    if (current.getFunction() == HarmonicFunction::S) return false;
    if (!checkAfterDominantTwo(previous, current)) return false;
    return true;
}

bool HarmonyRules::checkAfterSubdominant(const Chord& previous, const Chord& current) {
    if (current.getFunction() == HarmonicFunction::T && previous.getDegree() == 4 && current.getDegree() == 6) return false;
    if (current.getFunction() == HarmonicFunction::S && previous.getDegree() == 2 && current.getDegree() == 4) return false;

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