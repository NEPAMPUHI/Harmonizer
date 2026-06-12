#ifndef HARM_CHECKCHORDIDENTIFIER_H
#define HARM_CHECKCHORDIDENTIFIER_H

#include <vector>
#include "checking/CheckHarmonicPosition.h"
#include "checking/IdentifiedCheckChord.h"
#include "domain/HarmonizationSettings.h"

class CheckChordIdentifier {
public:
    std::vector<IdentifiedCheckChord> identify(
        const std::vector<CheckHarmonicPosition>& positions,
        const HarmonizationSettings& settings) const;
};

#endif
