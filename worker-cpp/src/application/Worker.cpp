#include "Worker.h"

JobResult Worker::process(const std::string& jobJson) {
    std::string jobId;
    try {
        HarmonizationJob job = jobParser.parse(jobJson);
        jobId = job.jobId;

        jobValidator.validate(job);

        return taskDispatcher.process(job);
    }
    catch (const std::exception& e) {
        return handleError(e, jobId);
    }
}

JobResult Worker::handleError(const std::exception& e, const std::string& jobId) {
    if (jobId.empty())
        return JobResult::error("WORKER_ERROR", e.what());
    return JobResult::error(jobId, "WORKER_ERROR", e.what());
}