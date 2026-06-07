#ifndef HARM_APP_HARMONIZATIONENGINE_H
#define HARM_APP_HARMONIZATIONENGINE_H

#include "infrastructure/Results.h"
#include "infrastructure/ResultStorage.h"
#include "domain/HarmonizationJob.h"
#include "harmonization/MelodyHarmonizer.h"
#include "harmonization/BassHarmonizer.h"

class HarmonizationEngine {
public:
    JobResult harmonizeMelody(const HarmonizationJob& job);
    JobResult harmonizeBass(const HarmonizationJob& job);

private:
    ResultStorage resultStorage;
    MelodyHarmonizer melodyHarmonizer;
    BassHarmonizer bassHarmonizer;
};

#endif
