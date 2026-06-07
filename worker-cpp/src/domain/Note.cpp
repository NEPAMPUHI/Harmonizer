#include "Note.h"
#include <cstdlib>
#include <stdexcept>

Interval::Interval(IntervalQuality quality, int number) : quality(quality), number(number) {}

Note::Note() = default;

Note::Note(NoteName name, int octave, int alter, int degree, Duration duration, bool isStrongBeat)
    : name(name),
      octave(octave),
      alter(alter),
      degree(degree),
      duration(duration),
      isStrongBeat(isStrongBeat) {}

NoteName Note::getName() const {
    return name;
}

int Note::getOctave() const {
    return octave;
}

int Note::getAlter() const {
    return alter;
}

int Note::getDegree() const {
    return degree;
}

void Note::setDegree(int d) {
    degree = d;
}

ScaleRelation Note::getScaleRelation() const {
    return scaleRelation;
}

void Note::setScaleRelation(ScaleRelation r) {
    scaleRelation = r;
}

Duration Note::getDuration() const {
    return duration;
}

bool Note::getBeat() const {
    return isStrongBeat;
}

int Note::getSemitone() const {
    int base = 0;

    switch (name) {
        case NoteName::C: base = 0; break;
        case NoteName::D: base = 2; break;
        case NoteName::E: base = 4; break;
        case NoteName::F: base = 5; break;
        case NoteName::G: base = 7; break;
        case NoteName::A: base = 9; break;
        case NoteName::B: base = 11; break;
    }

    return octave * 12 + base + alter;
}

int Note::nameToInt() const {
    return static_cast<int>(name);
}

int Note::getDiatonicPosition() const {
    return octave * 7 + nameToInt();
}

Interval Note::buildInterval(int semitones, int number) const {
    switch (number) {
        case 1:
            if (semitones == 0)  return Interval(IntervalQuality::Perfect,   number);
            if (semitones == 1)  return Interval(IntervalQuality::Augmented,  number);
            if (semitones == 11) return Interval(IntervalQuality::Diminished, number);
            break;

        case 2:
            if (semitones == 0) return Interval(IntervalQuality::Diminished, number);
            if (semitones == 1) return Interval(IntervalQuality::Minor, number);
            if (semitones == 2) return Interval(IntervalQuality::Major, number);
            if (semitones == 3) return Interval(IntervalQuality::Augmented, number);
            break;

        case 3:
            if (semitones == 2) return Interval(IntervalQuality::Diminished, number);
            if (semitones == 3) return Interval(IntervalQuality::Minor, number);
            if (semitones == 4) return Interval(IntervalQuality::Major, number);
            if (semitones == 5) return Interval(IntervalQuality::Augmented, number);
            break;

        case 4:
            if (semitones == 4) return Interval(IntervalQuality::Diminished, number);
            if (semitones == 5) return Interval(IntervalQuality::Perfect, number);
            if (semitones == 6) return Interval(IntervalQuality::Augmented, number);
            break;

        case 5:
            if (semitones == 6) return Interval(IntervalQuality::Diminished, number);
            if (semitones == 7) return Interval(IntervalQuality::Perfect, number);
            if (semitones == 8) return Interval(IntervalQuality::Augmented, number);
            break;

        case 6:
            if (semitones == 7) return Interval(IntervalQuality::Diminished, number);
            if (semitones == 8) return Interval(IntervalQuality::Minor, number);
            if (semitones == 9) return Interval(IntervalQuality::Major, number);
            if (semitones == 10) return Interval(IntervalQuality::Augmented, number);
            break;

        case 7:
            if (semitones == 9) return Interval(IntervalQuality::Diminished, number);
            if (semitones == 10) return Interval(IntervalQuality::Minor, number);
            if (semitones == 11) return Interval(IntervalQuality::Major, number);
            if (semitones == 0) return Interval(IntervalQuality::Augmented, number);
            break;
    }

    throw std::invalid_argument("Unsupported interval");
}

Interval Note::getInterval(const Note& other) const {
    int semitones = std::abs(getSemitone() - other.getSemitone());
    int number = std::abs(getDiatonicPosition() - other.getDiatonicPosition()) + 1;

    int simpleSemitones = semitones % 12;
    int simpleNumber = ((number - 1) % 7) + 1;

    Interval interval = buildInterval(simpleSemitones, simpleNumber);
    return Interval(interval.quality, number);
}

Interval Note::getSimpleInterval(const Note& other) const {
    int semitones = std::abs(getSemitone() - other.getSemitone()) % 12;
    int number = std::abs(getDiatonicPosition() - other.getDiatonicPosition()) % 7 + 1;

    return buildInterval(semitones, number);
}

bool Note::operator<(const Note& other) const {
    return getSemitone() < other.getSemitone();
}

bool Note::operator>(const Note& other) const {
    return other < *this;
}

bool Note::operator<=(const Note& other) const {
    return !(*this > other);
}

bool Note::operator>=(const Note& other) const {
    return !(*this < other);
}

bool Note::operator==(const Note& other) const {
    return getSemitone() == other.getSemitone();
}

bool Note::operator!=(const Note& other) const {
    return !(*this == other);
}