#include "infrastructure/ResultStorage.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

std::vector<HarmonizationResult> ResultStorage::saveVariants(
    const std::string& jobId,
    const std::vector<HarmonizationVariant>& variants,
    const HarmonizationSettings& settings)
{
    std::vector<HarmonizationResult> results;
    results.reserve(variants.size());
    for (int i = 0; i < static_cast<int>(variants.size()); ++i)
        results.push_back(saveVariant(jobId, variants[i], i, settings));
    return results;
}

HarmonizationResult ResultStorage::saveVariant(
    const std::string& jobId,
    const HarmonizationVariant& variant,
    int index,
    const HarmonizationSettings& settings)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "variant_%03d", index + 1);
    const std::string variantId = buf;

    createJobDirectory(jobId);
    const std::string path = buildVariantPath(jobId, variantId);

    const std::string xmlStr =
        musicXmlWriter.writeScoreToString(variant.musicScore, settings, jobId, variantId);

    std::ofstream file(path);
    if (!file)
        throw std::runtime_error("Cannot write result file: " + path);
    file << xmlStr;

    HarmonizationResult r;
    r.variantId          = variantId;
    r.score              = (variant.score != 0) ? variant.score : (100 - index);
    r.musicXmlPath       = path;
    r.musicXml           = xmlStr;
    r.frontendScore      = scoreSerializer.serialize(variant.musicScore, settings);
    r.rhythmPlanIndex    = variant.rhythmPlanIndex;
    r.rhythmPlanPriority = variant.rhythmPlanPriority;
    for (const auto& pos : variant.musicScore.positions)
        r.segmentDurations.push_back(pos.durationSixteenths);
    return r;
}

std::vector<HarmonizationResult> ResultStorage::savePlaceholderVariants(
    const std::string& jobId, int count)
{
    std::vector<HarmonizationResult> results;
    results.reserve(count);
    for (int i = 0; i < count; ++i)
        results.push_back(savePlaceholderVariant(jobId, i));
    return results;
}

HarmonizationResult ResultStorage::savePlaceholderVariant(
    const std::string& jobId, int index)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "variant_%03d", index + 1);
    const std::string variantId = buf;

    createJobDirectory(jobId);
    const std::string path = buildVariantPath(jobId, variantId);

    const std::string xmlStr =
        musicXmlWriter.writePlaceholderScoreToString(jobId, variantId);

    std::ofstream file(path);
    if (!file)
        throw std::runtime_error("Cannot write result file: " + path);
    file << xmlStr;

    HarmonizationResult r;
    r.variantId    = variantId;
    r.score        = 100 - index;
    r.musicXmlPath = path;
    r.musicXml     = xmlStr;
    // frontendScore remains default (empty) — placeholder has no notes
    return r;
}

std::vector<HarmonizationResult> ResultStorage::saveInputMelodyVariants(
    const HarmonizationJob& job, int count)
{
    std::vector<HarmonizationResult> results;
    results.reserve(count);
    for (int i = 0; i < count; ++i)
        results.push_back(saveInputMelodyVariant(job, i));
    return results;
}

HarmonizationResult ResultStorage::saveInputMelodyVariant(
    const HarmonizationJob& job, int index)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "variant_%03d", index + 1);
    const std::string variantId = buf;

    createJobDirectory(job.jobId);
    const std::string path = buildVariantPath(job.jobId, variantId);

    const std::string xmlStr =
        musicXmlWriter.writeInputMelodyToString(job, variantId);

    std::ofstream file(path);
    if (!file)
        throw std::runtime_error("Cannot write result file: " + path);
    file << xmlStr;

    HarmonizationResult r;
    r.variantId    = variantId;
    r.score        = 100 - index;
    r.musicXmlPath = path;
    r.musicXml     = xmlStr;
    // frontendScore remains default — input melody has no SATB harmonization yet
    return r;
}

std::string ResultStorage::createJobDirectory(const std::string& jobId) {
    std::string dirPath = "results/" + jobId;
    std::filesystem::create_directories(dirPath);
    return dirPath;
}

std::string ResultStorage::buildVariantPath(const std::string& jobId,
                                             const std::string& variantId) const {
    return "results/" + jobId + "/" + variantId + ".musicxml";
}
