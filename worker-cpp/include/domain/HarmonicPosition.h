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
    Note fixedNote; // нота, від якої будуємо акорд: для мелодії — сопрано, для басу — бас

    // Harmonic-segmentation metadata — populated by HarmonicPositionBuilder.
    // sourceNoteIndex: index in the original input note array; positions that
    //   share the same value belong to the same input note (tie candidates).
    // offsetInFixedNoteSixteenths: how many sixteenths into the source note
    //   this segment starts; 0 means the first (or only) segment.
    int sourceNoteIndex             = -1;
    int offsetInFixedNoteSixteenths =  0;
};

#endif