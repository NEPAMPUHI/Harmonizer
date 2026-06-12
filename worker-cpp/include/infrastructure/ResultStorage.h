#ifndef HARM_RESULTSTORAGE_H
#define HARM_RESULTSTORAGE_H

#include <string>
#include <vector>
#include "infrastructure/Results.h"
#include "infrastructure/MusicXmlWriter.h"
#include "infrastructure/ScoreSerializer.h"
#include "domain/HarmonizationJob.h"
#include "domain/HarmonizationVariant.h"

class ResultStorage {
public:
    std::vector<HarmonizationResult> saveVariants(
        const std::string& jobId,
        const std::vector<HarmonizationVariant>& variants,
        const HarmonizationSettings& settings);

    HarmonizationResult saveVariant(
        const std::string& jobId,
        const HarmonizationVariant& variant,
        int index,
        const HarmonizationSettings& settings);

    std::vector<HarmonizationResult> savePlaceholderVariants(
        const std::string& jobId, int count);

    HarmonizationResult savePlaceholderVariant(
        const std::string& jobId, int index);

    std::vector<HarmonizationResult> saveInputMelodyVariants(
        const HarmonizationJob& job, int count);

    HarmonizationResult saveInputMelodyVariant(
        const HarmonizationJob& job, int index);

    std::string createJobDirectory(const std::string& jobId);
    std::string buildVariantPath(const std::string& jobId,
                                 const std::string& variantId) const;

private:
    MusicXmlWriter musicXmlWriter;
    ScoreSerializer scoreSerializer;
};

#endif
