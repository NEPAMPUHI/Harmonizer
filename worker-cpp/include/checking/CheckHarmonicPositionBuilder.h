#ifndef HARM_CHECKHARMONICPOSITIONBUILDER_H
#define HARM_CHECKHARMONICPOSITIONBUILDER_H

#include <vector>
#include "checking/CheckHarmonicPosition.h"
#include "domain/CheckSolutionInput.h"

class CheckHarmonicPositionBuilder {
public:
    std::vector<CheckHarmonicPosition> build(const CheckSolutionInput& input) const;

private:
    std::vector<CheckHarmonicPosition> buildMeasure(
        const CheckMeasureInput& measure, int measureIndex) const;
};

#endif
