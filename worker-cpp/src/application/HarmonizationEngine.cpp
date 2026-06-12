#include "application/HarmonizationEngine.h"

JobResult HarmonizationEngine::harmonizeMelody(const HarmonizationJob& job) {
    auto variants = melodyHarmonizer.harmonize(job.input, job.settings);
    if (variants.empty())
        return JobResult::error(job.jobId, "NO_VARIANTS", "No harmonization variants generated");
    auto results = resultStorage.saveVariants(job.jobId, variants, job.settings);
    return JobResult::success(job.jobId, results);
}

JobResult HarmonizationEngine::harmonizeBass(const HarmonizationJob& job) {
    auto variants = bassHarmonizer.harmonize(job.input, job.settings);
    if (variants.empty())
        return JobResult::error(job.jobId, "NO_VARIANTS", "No harmonization variants generated");
    auto results = resultStorage.saveVariants(job.jobId, variants, job.settings);
    return JobResult::success(job.jobId, results);
}
