#include "Chord.h"
#include <stdexcept>

Chord::Chord() = default;

Chord::Chord(
    const Note& soprano,
    const Note& alto,
    const Note& tenor,
    const Note& bass,
    const ChordTemplate& chordTemplate
)
    : soprano(soprano),
      alto(alto),
      tenor(tenor),
      bass(bass),
      chordTemplate(chordTemplate)
{
}

Note Chord::getSoprano() const {
    return soprano;
}

Note Chord::getAlto() const {
    return alto;
}

Note Chord::getTenor() const {
    return tenor;
}

Note Chord::getBass() const {
    return bass;
}

ChordTemplate Chord::getChordTemplate() const {
    return chordTemplate;
}

HarmonicFunction Chord::getFunction() const {
    return chordTemplate.function;
}

int Chord::getDegree() const {
    return chordTemplate.degree;
}

ChordType Chord::getType() const {
    return chordTemplate.type;
}

Inversion Chord::getInversion() const {
    return chordTemplate.inversion;
}

ChordPosition Chord::getPosition() const {
    return chordTemplate.position;
}

std::string Chord::getName() const {
    if (getType() == ChordType::CadentialSixFour) {
        return "K64";
    }

    if (getDegree() == 1) {
        return "T" + getTypeString();
    }

    if (getDegree() == 2) {
        return "II" + getTypeString();
    }

    if (getDegree() == 3) {
        return "III" + getTypeString();
    }

    if (getDegree() == 4) {
        return "S" + getTypeString();
    }

    if (getDegree() == 5) {
        return "D" + getTypeString();
    }

    if (getDegree() == 6) {
        return "VI" + getTypeString();
    }

    if (getDegree() == 7) {
        return "VII" + getTypeString();
    }

    throw std::runtime_error("Unsupported chord type");
}

std::string Chord::getTypeString() const {
    switch (getType()) {
        case ChordType::Triad:
            return "53";
        case ChordType::Six:
            return "6";
        case ChordType::SixFour:
            return "64";
        case ChordType::Seventh:
            return "7";
        case ChordType::SixFive:
            return "65";
        case ChordType::FourThree:
            return "43";
        case ChordType::Two:
            return "2";
        case ChordType::CadentialSixFour:
            return "K64";
        case ChordType::Ninth:
            return "9";
        default:
            throw std::runtime_error("Unsupported chord type");
    }
}

int Chord::getSopranoDegree() const {
    return chordTemplate.degreesInSatbOrder[0];
}

int Chord::getAltoDegree() const {
    return chordTemplate.degreesInSatbOrder[1];
}

int Chord::getTenorDegree() const {
    return chordTemplate.degreesInSatbOrder[2];
}

int Chord::getBassDegree() const {
    return chordTemplate.degreesInSatbOrder[3];
}