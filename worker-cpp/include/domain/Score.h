#ifndef HARM_SCORE_H
#define HARM_SCORE_H

#include <vector>
#include "Chord.h"
#include "HarmonicPosition.h"

struct Score {
    std::vector<Chord> chords;
    std::vector<HarmonicPosition> positions;

    // 0 = soprano (melody mode), 3 = bass (bass mode), -1 = unset.
    // Used by serializers to place MusicXML ties on the correct voice.
    int fixedVoiceIndex = -1;

    bool empty() const;
};

#endif
