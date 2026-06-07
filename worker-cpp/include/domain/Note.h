#ifndef NOTE_H
#define NOTE_H

#include "domain/ScaleRelation.h"

enum class NoteName {
    C = 1, D, E, F, G, A, B
};

enum class IntervalQuality {
    Perfect,
    Augmented,
    Diminished,
    Major,
    Minor
};

struct Duration {
    int numerator = 1;
    int denominator = 16;
};

struct Interval {
    int number;
    IntervalQuality quality;

    Interval(IntervalQuality quality, int number);
};

class Note {
private:
    NoteName name = NoteName::C;
    int octave = 4;
    int alter = 0;   // -2, -1, 0, 1, 2
    int degree = 1;
    ScaleRelation scaleRelation = ScaleRelation::Natural;
    Duration duration;
    bool isStrongBeat = true;

public:
    Note();

    Note(NoteName name, int octave, int alter, int degree, Duration duration, bool isStrongBeat);

    NoteName getName() const;
    int getOctave() const;
    int getAlter() const;
    int getDegree() const;
    void setDegree(int d);
    ScaleRelation getScaleRelation() const;
    void setScaleRelation(ScaleRelation r);
    Duration getDuration() const;
    bool getBeat() const;

    int nameToInt() const;
    int getSemitone() const;
    int getDiatonicPosition() const;

    Interval buildInterval(int semitones, int number) const;
    Interval getInterval(const Note& other) const;
    Interval getSimpleInterval(const Note& other) const;

    bool operator<(const Note& other) const;
    bool operator>(const Note& other) const;
    bool operator<=(const Note& other) const;
    bool operator>=(const Note& other) const;
    bool operator==(const Note& other) const;
    bool operator!=(const Note& other) const;
};

#endif


