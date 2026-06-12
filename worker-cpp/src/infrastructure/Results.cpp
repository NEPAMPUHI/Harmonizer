#include "infrastructure/Results.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ── CheckError serialization helpers ─────────────────────────────────────────

static const char* errorCodeStr(CheckErrorCode c) {
    switch (c) {
        case CheckErrorCode::UnknownChord:                        return "UnknownChord";
        case CheckErrorCode::VoiceRangeViolation:                 return "VoiceRangeViolation";
        case CheckErrorCode::MoreThanOctaveBetweenAdjacentVoices: return "MoreThanOctaveBetweenAdjacentVoices";
        case CheckErrorCode::VoiceCrossing:                       return "VoiceCrossing";
        case CheckErrorCode::AllVoicesSameDirection:              return "AllVoicesSameDirection";
        case CheckErrorCode::ChromaticSemitoneTransfer:           return "ChromaticSemitoneTransfer";
        case CheckErrorCode::HiddenOctaves:                       return "HiddenOctaves";
        case CheckErrorCode::HiddenFifths:                        return "HiddenFifths";
        case CheckErrorCode::ParallelFifths:                      return "ParallelFifths";
        case CheckErrorCode::ParallelOctaves:                     return "ParallelOctaves";
        case CheckErrorCode::ParallelOctavesOrUnisons:            return "ParallelOctavesOrUnisons";
        case CheckErrorCode::ParallelSeconds:                     return "ParallelSeconds";
        case CheckErrorCode::ParallelSevenths:                    return "ParallelSevenths";
        case CheckErrorCode::AugmentedIntervalInBass:             return "AugmentedIntervalInBass";
        case CheckErrorCode::VoiceLeapGreaterThanOctave:          return "VoiceLeapGreaterThanOctave";
        case CheckErrorCode::ConsecutiveFourthsInBass:            return "ConsecutiveFourthsInBass";
        case CheckErrorCode::ConsecutiveFifthsInBass:             return "ConsecutiveFifthsInBass";
        case CheckErrorCode::FunctionalProgressionError:          return "FunctionalProgressionError";
    }
    return "Unknown";
}

static const char* renderTypeStr(CheckRenderType r) {
    switch (r) {
        case CheckRenderType::ChordMarker:            return "ChordMarker";
        case CheckRenderType::VerticalBracket:        return "VerticalBracket";
        case CheckRenderType::VerticalBracketPair:    return "VerticalBracketPair";
        case CheckRenderType::MotionLines:            return "MotionLines";
        case CheckRenderType::CrossingLines:          return "CrossingLines";
        case CheckRenderType::HiddenInterval:         return "HiddenInterval";
        case CheckRenderType::ChromaticTransfer:      return "ChromaticTransfer";
        case CheckRenderType::BassLineMarker:         return "BassLineMarker";
        case CheckRenderType::FunctionalRelationMarker: return "FunctionalRelationMarker";
    }
    return "Unknown";
}

static const char* voiceTypeStr(VoiceType v) {
    switch (v) {
        case VoiceType::Soprano: return "soprano";
        case VoiceType::Alto:    return "alto";
        case VoiceType::Tenor:   return "tenor";
        case VoiceType::Bass:    return "bass";
    }
    return "unknown";
}

static json checkErrorToJson(const CheckError& e) {
    json j;
    j["code"]       = errorCodeStr(e.code);
    j["renderType"] = renderTypeStr(e.renderType);
    if (!e.message.empty())
        j["message"] = e.message;
    if (e.positionIndex >= 0)
        j["positionIndex"] = e.positionIndex;
    if (e.nextPositionIndex >= 0)
        j["nextPositionIndex"] = e.nextPositionIndex;
    if (!e.voices.empty()) {
        json arr = json::array();
        for (VoiceType v : e.voices) arr.push_back(voiceTypeStr(v));
        j["voices"] = arr;
    }
    if (e.interval != 0)
        j["interval"] = e.interval;
    return j;
}

JobResult JobResult::success(const std::string& jobId) {
    return success(jobId, {});
}

JobResult JobResult::success(const std::string& jobId,
                              const std::vector<HarmonizationResult>& results) {
    JobResult r;
    r.jobId   = jobId;
    r.status  = "success";
    r.results = results;
    return r;
}

JobResult JobResult::error(const std::string& jobId,
                            const std::string& code,
                            const std::string& message) {
    JobResult r;
    r.jobId  = jobId;
    r.status = "error";
    r.errors.push_back({code, message});
    return r;
}

JobResult JobResult::error(const std::string& code, const std::string& message) {
    return error("", code, message);
}

// Serialises a FrontendNote to a json object.
static json noteToJson(const FrontendNote& n) {
    return {
        {"step",               n.step},
        {"octave",             n.octave},
        {"alter",              n.alter},
        {"durationSixteenths", n.durationSixteenths},
        {"durationType",       n.durationType},
        {"dotted",             n.dotted},
        {"tiedToNext",         n.tiedToNext},
        {"isRest",             n.isRest},
    };
}

// Serialises a vector of FrontendNotes to a json array.
static json voiceToJson(const std::vector<FrontendNote>& notes) {
    json arr = json::array();
    for (const auto& n : notes)
        arr.push_back(noteToJson(n));
    return arr;
}

std::string JobResult::toJson() const {
    json j;
    j["jobId"]  = jobId;
    j["status"] = status;
    if (!mode.empty())
        j["mode"] = mode;

    json resultsArr = json::array();
    for (const auto& r : results) {
        const FrontendScore& fs = r.frontendScore;

        json measuresArr = json::array();
        for (const auto& m : fs.measures) {
            json chordNamesArr = json::array();
            for (const auto& name : m.chordNames)
                chordNamesArr.push_back(name);

            json chordTicksArr = json::array();
            for (int t : m.chordTicks)
                chordTicksArr.push_back(t);

            json chordLabelVisibleArr = json::array();
            for (bool v : m.chordLabelVisible)
                chordLabelVisibleArr.push_back(v);

            measuresArr.push_back({
                {"number", m.number},
                {"voices", {
                    {"soprano", voiceToJson(m.voices.soprano)},
                    {"alto",    voiceToJson(m.voices.alto)},
                    {"tenor",   voiceToJson(m.voices.tenor)},
                    {"bass",    voiceToJson(m.voices.bass)},
                }},
                {"chordNames",        chordNamesArr},
                {"chordTicks",        chordTicksArr},
                {"chordLabelVisible", chordLabelVisibleArr},
            });
        }

        json segDurArr = json::array();
        for (int d : r.segmentDurations) segDurArr.push_back(d);

        resultsArr.push_back({
            {"variantId",          r.variantId},
            {"score",              r.score},
            {"rhythmPlanIndex",    r.rhythmPlanIndex},
            {"rhythmPlanPriority", r.rhythmPlanPriority},
            {"segmentDurations",   segDurArr},
            {"musicXml",           r.musicXml},
            // musicXmlPath is intentionally omitted — internal server path
            {"key",                fs.key},
            {"mode",               fs.mode},
            {"beats",              fs.beats},
            {"beatType",           fs.beatType},
            {"anacrusisSixteenths",fs.anacrusisSixteenths},
            {"measures",           measuresArr},
        });
    }
    j["results"] = resultsArr;

    // check_solution diagnostic errors take precedence over generic JobErrors.
    json errorsArr = json::array();
    if (!checkErrors.empty()) {
        for (const auto& e : checkErrors)
            errorsArr.push_back(checkErrorToJson(e));
    } else {
        for (const auto& e : errors)
            errorsArr.push_back({{"code", e.code}, {"message", e.message}});
    }
    j["errors"] = errorsArr;

    return j.dump();
}
