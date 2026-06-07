#ifndef HARM_CHORD_H
#define HARM_CHORD_H

#include "Note.h"
#include "ChordTemplate.h"

class Chord {
private:
    Note soprano;
    Note alto;
    Note tenor;
    Note bass;

    ChordTemplate chordTemplate;

public:
    Chord();

    Chord(
        const Note& soprano,
        const Note& alto,
        const Note& tenor,
        const Note& bass,
        const ChordTemplate& chordTemplate
    );

    Note getSoprano() const;
    Note getAlto() const;
    Note getTenor() const;
    Note getBass() const;

    ChordTemplate getChordTemplate() const;

    HarmonicFunction getFunction() const;
    int getDegree() const;
    ChordType getType() const;
    Inversion getInversion() const;
    ChordPosition getPosition() const;
    std::string getName() const;

    int getSopranoDegree() const;
    int getAltoDegree() const;
    int getTenorDegree() const;
    int getBassDegree() const;

private: 
    std::string getTypeString() const;
};

#endif