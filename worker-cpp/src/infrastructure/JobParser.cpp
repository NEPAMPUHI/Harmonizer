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

ScoreInput JobParser::parseInput(const json& inputJson, HarmonizationMode /*mode*/) const {
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

    for (const char* field : {"step", "octave", "alter", "durationSixteenths"})
        if (!noteJson.contains(field))
            throw std::runtime_error(std::string("Missing field: note.") + field);

    NoteName name   = stepToNoteName(noteJson["step"].get<std::string>());
    int octave      = noteJson["octave"].get<int>();
    int alter       = noteJson["alter"].get<int>();
    int sixteenths  = noteJson["durationSixteenths"].get<int>();

    // degree is key-dependent; will be assigned by the harmonization engine
    int degree = 0;
    Duration duration{sixteenths, 16};

    return Note(name, octave, alter, degree, duration, false);
}
