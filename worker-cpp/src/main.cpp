#include <string>

#include "infrastructure/InOut.h"
#include "application/Worker.h"
#include "infrastructure/Results.h"

int main() {
    InOut io;
    Worker worker;

    while (true) {
        std::string jobJson = io.readNextJob();

        if (jobJson == "shutdown") {
            break;
        }

        JobResult result = worker.process(jobJson);
        io.writeResult(result.toJson());
    }

    return 0;
}