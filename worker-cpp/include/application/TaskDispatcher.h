#ifndef HARM_TASKDISPATCHER_H
#define HARM_TASKDISPATCHER_H

#include "infrastructure/Results.h"
#include "domain/HarmonizationJob.h"
#include "application/HarmonizationEngine.h"

class TaskDispatcher {
public:
    JobResult process(const HarmonizationJob& job);

private:
    JobResult dispatchHarmonizeMelody(const HarmonizationJob& job);
    JobResult dispatchHarmonizeBass(const HarmonizationJob& job);
    JobResult dispatchCheckSolution(const HarmonizationJob& job);

    HarmonizationEngine harmonizationEngine;
};

#endif