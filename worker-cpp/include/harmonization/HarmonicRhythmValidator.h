#ifndef HARM_HARMONICRHYTHMVALIDATOR_H
#define HARM_HARMONICRHYTHMVALIDATOR_H

#include <string>
#include <vector>
#include "harmonization/HarmonicRhythmPlan.h"
#include "domain/HarmonizationSettings.h"
#include "domain/Note.h"

struct RhythmValidationResult {
    bool        ok      = true;
    std::string message;

    static RhythmValidationResult valid()              { return {true, {}}; }
    static RhythmValidationResult invalid(std::string msg) { return {false, std::move(msg)}; }
};

class HarmonicRhythmValidator {
public:
    RhythmValidationResult validate(
        const HarmonicRhythmPlan&    plan,
        const std::vector<Note>&     notes,
        const HarmonizationSettings& settings) const;
};

#endif
