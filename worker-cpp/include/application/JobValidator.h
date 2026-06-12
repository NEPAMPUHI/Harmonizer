#ifndef HARM_JOBVALIDATOR_H
#define HARM_JOBVALIDATOR_H

#include <string>
#include <vector>
#include "domain/HarmonizationJob.h"
#include "domain/HarmonizationSettings.h"
#include "domain/ScoreInput.h"

class JobValidator {
public:
    void validate(const HarmonizationJob& job) const;

private:
    void validateCommonFields(const HarmonizationJob& job) const;
    void validateSettings(const HarmonizationSettings& settings, HarmonizationMode mode) const;
    void validateKeyAndScaleModes(const HarmonizationSettings& settings, HarmonizationMode mode) const;
    bool isSupportedMajorKey(const std::string& key) const;
    bool isSupportedMinorKey(const std::string& key) const;
    void validateInput(const HarmonizationJob& job) const;
    void validateNotes(const ScoreInput& input) const;
    void validateAllowedChords(const std::vector<std::string>& allowedChords) const;
};

#endif
