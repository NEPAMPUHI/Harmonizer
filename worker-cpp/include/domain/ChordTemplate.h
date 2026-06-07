#ifndef HARM_CHORDTEMPLATE_H
#define HARM_CHORDTEMPLATE_H

#include <array>
#include <string>

enum class HarmonicFunction {
    T,
    S,
    D
};

enum class ChordType {
    // triad and inversions
    Triad,
    Six,
    SixFour,
    // seventh and inversions
    Seventh,
    SixFive,
    FourThree,
    Two,
    // others
    CadentialSixFour,
    Ninth
};

/*

enum class Inversion means chord tone in soprano.

e.g.: S53 in C major
- F - I
- A - III
- C - V
 */
enum class Inversion {
    I,
    III,
    V,
    VII,
    IX
};

enum class ChordPosition {
    Close,
    Wide,
    Mixed
};

struct ChordTemplate {
    HarmonicFunction function;
    int degree; // degree of the root chord: T=1, S=4, D=5, II=2, etc.
    ChordType type;
    Inversion inversion;
    ChordPosition position;

    // Degrees of soprano, alto, tenor, bass
    std::array<int, 4> degreesInSatbOrder;
};

#endif
