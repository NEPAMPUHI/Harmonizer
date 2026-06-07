#ifndef HARM_WORKER_H
#define HARM_WORKER_H

#include <string>
#include "infrastructure/Results.h"
#include "infrastructure/JobParser.h"
#include "application/JobValidator.h"
#include "application/TaskDispatcher.h"

class Worker {
public:
    JobResult process(const std::string& jobJson);

private:
    JobResult handleError(const std::exception& e, const std::string& jobId = "");

    JobParser jobParser;
    JobValidator jobValidator;
    TaskDispatcher taskDispatcher;
};

#endif

