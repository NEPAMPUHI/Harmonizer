#ifndef HARM_RESULTS_H
#define HARM_RESULTS_H

#include <string>
#include <vector>
#include "checking/CheckError.h"
#include "infrastructure/FrontendScore.h"

struct HarmonizationResult {
    std::string variantId;
    int score = 0;
    std::string musicXmlPath; // internal path on disk — not exposed in JSON response
    std::string musicXml;     // full XML string embedded in response
    FrontendScore frontendScore;

    // Rhythm-plan metadata for debugging and future UI use.
    int              rhythmPlanIndex    = -1;
    int              rhythmPlanPriority =  0;
    std::vector<int> segmentDurations;   // durationSixteenths per harmonic position
};

struct JobError {
    std::string code;
    std::string message;
};

struct JobResult {
    std::string jobId;
    std::string status;
    std::string mode;   // set for check_solution; empty for harmonize modes
    std::vector<HarmonizationResult> results;
    std::vector<JobError>   errors;       // system-level errors (harmonize pipeline)
    std::vector<CheckError> checkErrors;  // diagnostic errors (check_solution mode)

    static JobResult success(const std::string& jobId);
    static JobResult success(const std::string& jobId,
                             const std::vector<HarmonizationResult>& results);
    static JobResult error(const std::string& jobId,
                           const std::string& code,
                           const std::string& message);
    static JobResult error(const std::string& code, const std::string& message);

    std::string toJson() const;
};

#endif
