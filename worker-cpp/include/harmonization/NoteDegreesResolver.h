#ifndef HARM_NOTEDEGREESRESOLVER_H
#define HARM_NOTEDEGREESRESOLVER_H

#include <vector>
#include "domain/HarmonicPosition.h"
#include "domain/HarmonizationSettings.h"

// Pipeline step that resolves Note::degree for every fixedNote in the given positions
// using ScaleDegreeCalculator.
class NoteDegreesResolver {
public:
    void resolveInPlace(std::vector<HarmonicPosition>& positions,
                        const HarmonizationSettings& settings) const;
};

#endif
