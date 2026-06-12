#ifndef HARM_HARMONICSEGMENT_H
#define HARM_HARMONICSEGMENT_H

#include "Note.h"

struct HarmonicSegment {
    Note fixedNote;
    int  durationSixteenths          = 4;
    int  offsetInFixedNoteSixteenths = 0;
    int  sourceNoteIndex             = 0;
    bool isRest                      = false;
};

#endif
