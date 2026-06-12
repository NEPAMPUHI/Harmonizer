#ifndef HARM_CHECKHARMONICPOSITION_H
#define HARM_CHECKHARMONICPOSITION_H

#include "domain/Note.h"

// One vertical slice of the check_solution timeline.
// A segment [positionInMeasureSixteenths, +durationSixteenths) where every
// active voice contributes its sounding note (or rest).
// hasX == false means the voice had no note covering this segment at all
// (neither a pitch nor a rest — the voice simply left a gap).
struct CheckHarmonicPosition {
    int measureIndex                = 0;
    int positionInMeasureSixteenths = 0;
    int durationSixteenths          = 0;

    Note soprano;
    Note alto;
    Note tenor;
    Note bass;

    bool hasSoprano = false;
    bool hasAlto    = false;
    bool hasTenor   = false;
    bool hasBass    = false;
};

#endif
