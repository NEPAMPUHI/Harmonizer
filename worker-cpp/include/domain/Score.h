#ifndef HARM_SCORE_H
#define HARM_SCORE_H

#include <vector>
#include "Chord.h"
#include "HarmonicPosition.h"

struct Score {
    std::vector<Chord> chords;
    std::vector<HarmonicPosition> positions;

    bool empty() const;
};

#endif
