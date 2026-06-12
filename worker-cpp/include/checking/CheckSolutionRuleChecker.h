#ifndef HARM_CHECKSOLUTIONRULECHECKER_H
#define HARM_CHECKSOLUTIONRULECHECKER_H

#include <vector>
#include "checking/CheckError.h"
#include "checking/IdentifiedCheckChord.h"
#include "domain/ActiveRuleSet.h"

// Diagnostic-layer checker for check_solution mode.
// Does NOT replace HarmonyRules — it collects all errors in a single pass
// and never stops early.
class CheckSolutionRuleChecker {
public:
    // Backward-compat: all rules enabled.
    std::vector<CheckError> check(
        const std::vector<IdentifiedCheckChord>& chords) const;

    // Rules-aware: only enforces the rules listed in the active set.
    std::vector<CheckError> check(
        const std::vector<IdentifiedCheckChord>& chords,
        const ActiveRuleSet& rules) const;
};

#endif
