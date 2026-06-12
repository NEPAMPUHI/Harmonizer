#ifndef HARM_CHECKSOLUTIONINPUT_H
#define HARM_CHECKSOLUTIONINPUT_H

#include <vector>
#include "domain/Note.h"

// Raw SATB voices for one measure, as received from the frontend.
// No timeline synchronisation — each voice is an independent sequence.
// The C++ engine is responsible for building harmonic positions from these.
struct CheckMeasureInput {
    std::vector<Note> soprano;
    std::vector<Note> alto;
    std::vector<Note> tenor;
    std::vector<Note> bass;
};

struct CheckSolutionInput {
    std::vector<CheckMeasureInput> measures;
};

#endif
