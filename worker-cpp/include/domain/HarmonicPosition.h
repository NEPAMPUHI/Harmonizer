#ifndef HARM_HARMONICPOSITION_H
#define HARM_HARMONICPOSITION_H

#include <vector>
#include "Note.h"

struct HarmonicPosition {
    int index;

    int measureIndex;
    int startSixteenth;
    int durationSixteenths;

    bool isStrongBeat = false;
    bool isMediumBeat = false;
    bool isWeakBeat = false;

    bool isBeginningZone = false;
    bool isMiddleZone = false;
    bool isCadentialZone = false;
    bool isEndingZone = false;

    std::vector<Note> melodyNotes;
    Note fixedNote; //це нота, від якої будуємо акорд: для гармонізації мелодії це сопрано, для гармонізації басу — бас
};

#endif