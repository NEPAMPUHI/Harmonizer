#ifndef HARM_RESULTS_H
#define HARM_RESULTS_H

#include <string>
#include <vector>

struct StoredResultFile {
    std::string variantId;
    std::string path;
    int score = 0;
};

struct JobError {
    std::string code;
    std::string message;
};

struct JobResult {
    std::string jobId;
    std::string status;
    std::vector<StoredResultFile> files;
    std::vector<JobError> errors;

    static JobResult success(const std::string& jobId);
    static JobResult success(const std::string& jobId, const std::vector<StoredResultFile>& files);
    static JobResult error(const std::string& jobId, const std::string& code, const std::string& message);
    // overload used by Worker when jobId is not yet known
    static JobResult error(const std::string& code, const std::string& message);

    std::string toJson() const;
};

#endif
