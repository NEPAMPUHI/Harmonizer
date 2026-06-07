#include "application/TaskDispatcher.h"
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
    // TODO: delegate to SolutionCheckingEngine once it is wired up
    return JobResult::success(job.jobId);
}
