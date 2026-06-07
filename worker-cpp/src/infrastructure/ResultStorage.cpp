#include "infrastructure/ResultStorage.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

std::vector<StoredResultFile> ResultStorage::saveVariants(
    const std::string& jobId,
    const std::vector<HarmonizationVariant>& variants)
{
    std::vector<StoredResultFile> files;
    for (int i = 0; i < static_cast<int>(variants.size()); ++i)
        files.push_back(saveVariant(jobId, variants[i], i));
    return files;
}

StoredResultFile ResultStorage::saveVariant(const std::string& jobId,
                                             const HarmonizationVariant& variant,
                                             int index)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "variant_%03d", index + 1);
    std::string variantId = buf;

    createJobDirectory(jobId);
    std::string path = buildVariantPath(jobId, variantId);

    std::ofstream file(path);
    if (!file)
        throw std::runtime_error("Cannot write result file: " + path);
    file << musicXmlWriter.writeScoreToString(variant.musicScore, jobId, variantId);

    return StoredResultFile{variantId, path, variant.score != 0 ? variant.score : 100 - index};
}

std::vector<StoredResultFile> ResultStorage::savePlaceholderVariants(const std::string& jobId, int count) {
    std::vector<StoredResultFile> files;
    for (int i = 0; i < count; ++i)
        files.push_back(savePlaceholderVariant(jobId, i));
    return files;
}

StoredResultFile ResultStorage::savePlaceholderVariant(const std::string& jobId, int index) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "variant_%03d", index + 1);
    std::string variantId = buf;

    createJobDirectory(jobId);
    std::string path = buildVariantPath(jobId, variantId);

    std::ofstream file(path);
    if (!file)
        throw std::runtime_error("Cannot write result file: " + path);
    file << musicXmlWriter.writePlaceholderScoreToString(jobId, variantId);

    return StoredResultFile{variantId, path, 100 - index};
}

std::string ResultStorage::createJobDirectory(const std::string& jobId) {
    std::string dirPath = "results/" + jobId;
    std::filesystem::create_directories(dirPath);
    return dirPath;
}

std::string ResultStorage::buildVariantPath(const std::string& jobId, const std::string& variantId) const {
    return "results/" + jobId + "/" + variantId + ".musicxml";
}

std::vector<StoredResultFile> ResultStorage::saveInputMelodyVariants(const HarmonizationJob& job, int count) {
    std::vector<StoredResultFile> files;
    for (int i = 0; i < count; ++i)
        files.push_back(saveInputMelodyVariant(job, i));
    return files;
}

StoredResultFile ResultStorage::saveInputMelodyVariant(const HarmonizationJob& job, int index) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "variant_%03d", index + 1);
    std::string variantId = buf;

    createJobDirectory(job.jobId);
    std::string path = buildVariantPath(job.jobId, variantId);

    std::ofstream file(path);
    if (!file)
        throw std::runtime_error("Cannot write result file: " + path);
    file << musicXmlWriter.writeInputMelodyToString(job, variantId);

    return StoredResultFile{variantId, path, 100 - index};
}
