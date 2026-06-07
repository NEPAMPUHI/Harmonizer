#ifndef HARM_SCALEDEGREECALCULATOR_H
#define HARM_SCALEDEGREECALCULATOR_H

#include <string>
#include "domain/Note.h"
#include "domain/HarmonizationSettings.h"
#include "domain/ScaleRelation.h"

struct DegreeInfo {
    int degree;
    ScaleRelation relation;
};

// Lightweight helper for resolving a note's scale degree within a key.
// Returns 1-7 for diatonic notes, -1 for chromatic notes, -1 if key unknown.
// Will become the core of ScaleContext in a later refactor.
class ScaleDegreeCalculator {
public:
    DegreeInfo calculateDetailed(const Note& note, const HarmonizationSettings& settings) const;

    // Convenience wrapper — returns only the degree from calculateDetailed().
    int calculate(const Note& note, const HarmonizationSettings& settings) const;

private:
    bool isMajorKey(const std::string& key) const;
    std::string normalizeKeyRoot(const std::string& key) const;
};

#endif
