#include "harmonization/NoteDegreesResolver.h"
#include "harmonization/ScaleDegreeCalculator.h"

void NoteDegreesResolver::resolveInPlace(std::vector<HarmonicPosition>& positions,
                                          const HarmonizationSettings& settings) const {
    ScaleDegreeCalculator calculator;
    for (auto& pos : positions) {
        DegreeInfo info = calculator.calculateDetailed(pos.fixedNote, settings);
        pos.fixedNote.setDegree(info.degree);
        pos.fixedNote.setScaleRelation(info.relation);
    }
}
