#include "application/TaskDispatcher.h"
#include "checking/CheckHarmonicPositionBuilder.h"
#include "checking/CheckChordIdentifier.h"
#include "checking/CheckSolutionRuleChecker.h"
#include "domain/ActiveRuleSet.h"
#include <iostream>
#include <stdexcept>

JobResult TaskDispatcher::process(const HarmonizationJob& job) {
    switch (job.mode) {
        case HarmonizationMode::HarmonizeMelody:
            return dispatchHarmonizeMelody(job);
        case HarmonizationMode::HarmonizeBass:
            return dispatchHarmonizeBass(job);
        case HarmonizationMode::CheckSolution:
            return dispatchCheckSolution(job);
        default:
            throw std::runtime_error("Unsupported harmonization mode");
    }
}

JobResult TaskDispatcher::dispatchHarmonizeMelody(const HarmonizationJob& job) {
    return harmonizationEngine.harmonizeMelody(job);
}

JobResult TaskDispatcher::dispatchHarmonizeBass(const HarmonizationJob& job) {
    return harmonizationEngine.harmonizeBass(job);
}

JobResult TaskDispatcher::dispatchCheckSolution(const HarmonizationJob& job) {
    const auto positions   = CheckHarmonicPositionBuilder{}.build(job.checkSolutionInput);
    const auto identified  = CheckChordIdentifier{}.identify(positions, job.settings);
    const auto rules       = ActiveRuleSet::allEnabled();
    const auto checkErrors = CheckSolutionRuleChecker{}.check(identified, rules);

    // Debug — first 5 positions
    const std::size_t limit = std::min(identified.size(), std::size_t{5});
    for (std::size_t i = 0; i < limit; ++i) {
        const auto& r = identified[i];
        const auto& p = r.position;
        std::cerr << "[check_solution] pos[" << i << "] m=" << p.measureIndex
                  << " start=" << p.positionInMeasureSixteenths
                  << " dur="   << p.durationSixteenths;
        if (r.isKnownChord)
            std::cerr << " chord=" << r.chord.getName();
        else
            std::cerr << " UNKNOWN S=" << p.hasSoprano << " A=" << p.hasAlto
                      << " T=" << p.hasTenor << " B=" << p.hasBass;
        std::cerr << '\n';
    }
    std::cerr << "[check_solution] " << checkErrors.size() << " diagnostic error(s)\n";

    auto result         = JobResult::success(job.jobId);
    result.mode         = "check_solution";
    result.checkErrors  = checkErrors;
    return result;
}
