#include "infrastructure/JobParser.h"
#include <stdexcept>

using json = nlohmann::json;

static void requireField(const json& obj, const std::string& field) {
    if (!obj.contains(field))
        throw std::runtime_error("Missing field: " + field);
}

HarmonizationJob JobParser::parse(const std::string& jsonText) const {
    json j;
    try {
        j = json::parse(jsonText);
    } catch (const json::parse_error&) {
        throw std::runtime_error("Invalid JSON");
    }

    requireField(j, "jobId");
    requireField(j, "mode");
    requireField(j, "settings");
    requireField(j, "input");

    HarmonizationJob job;
    job.jobId    = j["jobId"].get<std::string>();
    job.mode     = parseMode(j["mode"].get<std::string>());
    job.settings = parseSettings(j["settings"], job.mode);
    job.input    = parseInput(j["input"], job.mode);
    if (job.mode == HarmonizationMode::CheckSolution)
        job.checkSolutionInput = parseCheckSolutionInput(j["input"]);
    return job;
}

HarmonizationMode JobParser::parseMode(const std::string& mode) const {
    if (mode == "harmonize_melody") return HarmonizationMode::HarmonizeMelody;
    if (mode == "harmonize_bass")   return HarmonizationMode::HarmonizeBass;
    if (mode == "check_solution")   return HarmonizationMode::CheckSolution;
    throw std::runtime_error("Unknown job mode: " + mode);
}

HarmonizationSettings JobParser::parseSettings(const json& s, HarmonizationMode mode) const {
    requireField(s, "key");
    requireField(s, "measureCount");
    requireField(s, "timeSignature");
    requireField(s, "anacrusisSixteenths");
    requireField(s, "forbiddenRules");
    requireField(s, "allowedChords");

    HarmonizationSettings settings;
    settings.key                = s["key"].get<std::string>();
    settings.measureCount       = s["measureCount"].get<int>();
    settings.timeSignature      = parseTimeSignature(s["timeSignature"]);
    settings.anacrusisSixteenths = s["anacrusisSixteenths"].get<int>();

    // scaleMode is required for harmonization modes; optional for check_solution.
    if (mode != HarmonizationMode::CheckSolution) {
        requireField(s, "scaleMode");
        settings.scaleModes = s["scaleMode"].get<std::vector<std::string>>();
    } else if (s.contains("scaleMode") && s["scaleMode"].is_array()) {
        settings.scaleModes = s["scaleMode"].get<std::vector<std::string>>();
    }

    for (const auto& r : s["forbiddenRules"])
        settings.forbiddenRules.push_back(r.get<std::string>());
    for (const auto& c : s["allowedChords"])
        settings.allowedChords.push_back(c.get<std::string>());

    settings.splitLongNotesByBasePulse = s.value("splitLongNotesByBasePulse", false);

    return settings;
}

TimeSignature JobParser::parseTimeSignature(const json& t) const {
    requireField(t, "beats");
    requireField(t, "beatType");

    TimeSignature ts;
    ts.beats    = t["beats"].get<int>();
    ts.beatType = t["beatType"].get<int>();
    return ts;
}

ScoreInput JobParser::parseInput(const json& inputJson, HarmonizationMode mode) const {
    if (mode == HarmonizationMode::CheckSolution)
        return {};  // notes not required for check_solution

    if (!inputJson.contains("notes") || !inputJson["notes"].is_array())
        throw std::runtime_error("Missing field: input.notes");

    ScoreInput input;
    for (const auto& noteJson : inputJson["notes"])
        input.notes.push_back(parseNote(noteJson));
    return input;
}

static NoteName stepToNoteName(const std::string& step) {
    if (step == "C") return NoteName::C;
    if (step == "D") return NoteName::D;
    if (step == "E") return NoteName::E;
    if (step == "F") return NoteName::F;
    if (step == "G") return NoteName::G;
    if (step == "A") return NoteName::A;
    if (step == "B") return NoteName::B;
    throw std::runtime_error("Invalid note step: " + step);
}

Note JobParser::parseNote(const json& noteJson) const {
    if (!noteJson.is_object())
        throw std::runtime_error("Note must be a JSON object");

    if (!noteJson.contains("durationSixteenths"))
        throw std::runtime_error("Missing field: note.durationSixteenths");

    const bool isRest       = noteJson.value("isRest", false);
    const bool tiedToNext   = noteJson.value("tiedToNext", false);
    const int  sixteenths   = noteJson["durationSixteenths"].get<int>();

    NoteName name  = NoteName::C;
    int      octave = 4;
    int      alter  = 0;

    if (!isRest) {
        for (const char* field : {"step", "octave", "alter"})
            if (!noteJson.contains(field))
                throw std::runtime_error(std::string("Missing field: note.") + field);
        name   = stepToNoteName(noteJson["step"].get<std::string>());
        octave = noteJson["octave"].get<int>();
        alter  = noteJson["alter"].get<int>();
    }

    return Note(name, octave, alter, 0, sixteenths, false, tiedToNext, isRest);
}

// ── check_solution input ──────────────────────────────────────────────────────

Note JobParser::parseCheckNote(const json& noteJson) const {
    if (!noteJson.is_object())
        throw std::runtime_error("Check note must be a JSON object");
    if (!noteJson.contains("durationSixteenths"))
        throw std::runtime_error("Missing field: note.durationSixteenths");

    const bool isRest     = noteJson.value("isRest", false);
    const int  sixteenths = noteJson["durationSixteenths"].get<int>();
    if (sixteenths <= 0)
        throw std::runtime_error("note.durationSixteenths must be > 0");

    if (isRest)
        return Note(NoteName::C, 4, 0, 0, sixteenths, false, false, true);

    for (const char* field : {"name", "octave", "alter"})
        if (!noteJson.contains(field))
            throw std::runtime_error(std::string("Missing field: note.") + field);

    const NoteName name   = stepToNoteName(noteJson["name"].get<std::string>());
    const int      octave = noteJson["octave"].get<int>();
    const int      alter  = noteJson["alter"].get<int>();

    return Note(name, octave, alter, 0, sixteenths, false);
}

CheckSolutionInput JobParser::parseCheckSolutionInput(const json& inputJson) const {
    if (!inputJson.contains("measures") || !inputJson["measures"].is_array())
        throw std::runtime_error("Missing field: input.measures");

    CheckSolutionInput result;
    for (const auto& measureJson : inputJson["measures"]) {
        if (!measureJson.is_object())
            throw std::runtime_error("Each measure must be a JSON object");
        for (const char* voice : {"soprano", "alto", "tenor", "bass"})
            if (!measureJson.contains(voice) || !measureJson[voice].is_array())
                throw std::runtime_error(std::string("Missing voice array in measure: ") + voice);

        CheckMeasureInput measure;
        for (const auto& n : measureJson["soprano"]) measure.soprano.push_back(parseCheckNote(n));
        for (const auto& n : measureJson["alto"])    measure.alto.push_back(parseCheckNote(n));
        for (const auto& n : measureJson["tenor"])   measure.tenor.push_back(parseCheckNote(n));
        for (const auto& n : measureJson["bass"])    measure.bass.push_back(parseCheckNote(n));
        result.measures.push_back(std::move(measure));
    }
    return result;
}
