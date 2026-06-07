#ifndef HARM_JOBPARSER_H
#define HARM_JOBPARSER_H

#include <string>
#include "domain/HarmonizationJob.h"
#include "domain/HarmonizationSettings.h"
#include "domain/ScoreInput.h"
#include <nlohmann/json.hpp>

class JobParser {
public:
    HarmonizationJob parse(const std::string& jsonText) const;
    HarmonizationMode parseMode(const std::string& mode) const;

private:
    HarmonizationSettings parseSettings(const nlohmann::json& settingsJson, HarmonizationMode mode) const;
    TimeSignature parseTimeSignature(const nlohmann::json& timeSignatureJson) const;
    ScoreInput parseInput(const nlohmann::json& inputJson, HarmonizationMode mode) const;
    Note parseNote(const nlohmann::json& noteJson) const;
};

#endif
