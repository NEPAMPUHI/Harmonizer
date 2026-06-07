#ifndef HARM_RESULTSTORAGE_H
#define HARM_RESULTSTORAGE_H

#include <string>
#include <vector>
#include "infrastructure/Results.h"
#include "infrastructure/MusicXmlWriter.h"
#include "domain/HarmonizationJob.h"
#include "domain/HarmonizationVariant.h"

class ResultStorage {
public:
    std::vector<StoredResultFile> saveVariants(const std::string& jobId,
                                               const std::vector<HarmonizationVariant>& variants);
    StoredResultFile saveVariant(const std::string& jobId,
                                 const HarmonizationVariant& variant,
                                 int index);

    std::vector<StoredResultFile> savePlaceholderVariants(const std::string& jobId, int count);
    StoredResultFile savePlaceholderVariant(const std::string& jobId, int index);

    std::vector<StoredResultFile> saveInputMelodyVariants(const HarmonizationJob& job, int count);
    StoredResultFile saveInputMelodyVariant(const HarmonizationJob& job, int index);

    std::string createJobDirectory(const std::string& jobId);
    std::string buildVariantPath(const std::string& jobId, const std::string& variantId) const;

private:
    MusicXmlWriter musicXmlWriter;
};

#endif
