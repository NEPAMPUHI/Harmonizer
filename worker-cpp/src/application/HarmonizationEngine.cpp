#include "application/HarmonizationEngine.h"

JobResult HarmonizationEngine::harmonizeMelody(const HarmonizationJob& job) {
    auto variants = melodyHarmonizer.harmonize(job.input, job.settings);
    if (variants.empty())
        return JobResult::error(job.jobId, "NO_VARIANTS", "No harmonization variants generated");
    auto files = resultStorage.saveVariants(job.jobId, variants);
    return JobResult::success(job.jobId, files);
}

JobResult HarmonizationEngine::harmonizeBass(const HarmonizationJob& job) {
    auto variants = bassHarmonizer.harmonize(job.input, job.settings);
    if (variants.empty())
        return JobResult::error(job.jobId, "NO_VARIANTS", "No harmonization variants generated");
    auto files = resultStorage.saveVariants(job.jobId, variants);
    return JobResult::success(job.jobId, files);
}
