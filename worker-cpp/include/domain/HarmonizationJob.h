#ifndef HARM_HARMONIZATIONJOB_H
#define HARM_HARMONIZATIONJOB_H

#include "HarmonizationSettings.h"
#include "ScoreInput.h"
#include <string>

enum class HarmonizationMode {
    HarmonizeMelody,
    HarmonizeBass,
    CheckSolution
};

struct HarmonizationJob {
    std::string jobId;
    HarmonizationMode mode;
    HarmonizationSettings settings;
    ScoreInput input;
};

#endif
