#ifndef HARM_IDENTIFIEDCHECKCORD_H
#define HARM_IDENTIFIEDCHECKCORD_H

#include "checking/CheckHarmonicPosition.h"
#include "domain/Chord.h"

struct IdentifiedCheckChord {
    CheckHarmonicPosition position;

    bool isKnownChord = false;

    // Only valid when isKnownChord == true
    Chord        chord;
    ChordTemplate matchedTemplate;
};

#endif
