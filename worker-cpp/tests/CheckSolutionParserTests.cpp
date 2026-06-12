#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "infrastructure/JobParser.h"
#include "application/TaskDispatcher.h"
#include "domain/HarmonizationJob.h"
#include "domain/CheckSolutionInput.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static const char* SETTINGS_FRAGMENT = R"(
  "settings": {
    "key": "C",
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": []
  })";

static std::string oneNotePerVoiceJson(int dur = 16) {
    return std::string(R"({"jobId":"cs_t1","mode":"check_solution",)") +
           SETTINGS_FRAGMENT + R"(,
  "input": {
    "measures": [{
      "soprano": [{ "name": "E", "octave": 5, "alter":  0, "durationSixteenths":)" +
           std::to_string(dur) + R"(, "isRest": false }],
      "alto":    [{ "name": "C", "octave": 5, "alter":  0, "durationSixteenths":)" +
           std::to_string(dur) + R"(, "isRest": false }],
      "tenor":   [{ "name": "G", "octave": 4, "alter":  0, "durationSixteenths":)" +
           std::to_string(dur) + R"(, "isRest": false }],
      "bass":    [{ "name": "C", "octave": 3, "alter":  0, "durationSixteenths":)" +
           std::to_string(dur) + R"(, "isRest": false }]
    }]
  }
})";
}

// ── parser: structure ─────────────────────────────────────────────────────────

TEST_CASE("CheckSolutionParser: mode parsed as CheckSolution", "[checkparser]") {
    JobParser parser;
    auto job = parser.parse(oneNotePerVoiceJson());
    CHECK(job.mode == HarmonizationMode::CheckSolution);
}

TEST_CASE("CheckSolutionParser: one measure — all four voice arrays have size 1", "[checkparser]") {
    JobParser parser;
    auto job = parser.parse(oneNotePerVoiceJson());

    REQUIRE(job.checkSolutionInput.measures.size() == 1);
    const auto& m = job.checkSolutionInput.measures[0];
    CHECK(m.soprano.size() == 1);
    CHECK(m.alto.size()    == 1);
    CHECK(m.tenor.size()   == 1);
    CHECK(m.bass.size()    == 1);
}

TEST_CASE("CheckSolutionParser: note fields correctly round-trip", "[checkparser]") {
    JobParser parser;
    auto job = parser.parse(oneNotePerVoiceJson(8));

    const auto& m = job.checkSolutionInput.measures[0];

    // soprano: E5, alter=0, dur=8, not rest
    CHECK(m.soprano[0].getName()              == NoteName::E);
    CHECK(m.soprano[0].getOctave()            == 5);
    CHECK(m.soprano[0].getAlter()             == 0);
    CHECK(m.soprano[0].getDurationSixteenths()== 8);
    CHECK(m.soprano[0].isRest()               == false);

    // bass: C3
    CHECK(m.bass[0].getName()  == NoteName::C);
    CHECK(m.bass[0].getOctave()== 3);
}

TEST_CASE("CheckSolutionParser: rest note — no pitch fields required", "[checkparser]") {
    const std::string json = std::string(R"({"jobId":"cs_rest","mode":"check_solution",)") +
        SETTINGS_FRAGMENT + R"(,
  "input": {
    "measures": [{
      "soprano": [{ "durationSixteenths": 4, "isRest": true }],
      "alto":    [{ "durationSixteenths": 8, "isRest": true }],
      "tenor":   [{ "durationSixteenths": 4, "isRest": true }],
      "bass":    [{ "durationSixteenths": 16, "isRest": true }]
    }]
  }
})";

    JobParser parser;
    REQUIRE_NOTHROW(parser.parse(json));
    auto job = parser.parse(json);

    const auto& m = job.checkSolutionInput.measures[0];
    CHECK(m.soprano[0].isRest()                == true);
    CHECK(m.soprano[0].getDurationSixteenths() == 4);
    CHECK(m.alto[0].getDurationSixteenths()    == 8);
    CHECK(m.bass[0].getDurationSixteenths()    == 16);
}

TEST_CASE("CheckSolutionParser: altered note (F#)", "[checkparser]") {
    const std::string json = std::string(R"({"jobId":"cs_alt","mode":"check_solution",)") +
        SETTINGS_FRAGMENT + R"(,
  "input": {
    "measures": [{
      "soprano": [{ "name": "F", "octave": 5, "alter": 1, "durationSixteenths": 4, "isRest": false }],
      "alto":    [{ "name": "C", "octave": 5, "alter": 0, "durationSixteenths": 4, "isRest": false }],
      "tenor":   [{ "name": "A", "octave": 4, "alter": 0, "durationSixteenths": 4, "isRest": false }],
      "bass":    [{ "name": "D", "octave": 3, "alter": 0, "durationSixteenths": 4, "isRest": false }]
    }]
  }
})";

    JobParser parser;
    auto job = parser.parse(json);
    const auto& m = job.checkSolutionInput.measures[0];
    CHECK(m.soprano[0].getName()  == NoteName::F);
    CHECK(m.soprano[0].getAlter() == 1);
}

TEST_CASE("CheckSolutionParser: two measures — counts correct per measure", "[checkparser]") {
    const std::string json = std::string(R"({"jobId":"cs_2m","mode":"check_solution",)") +
        SETTINGS_FRAGMENT + R"(,
  "input": {
    "measures": [
      {
        "soprano": [{ "name": "E", "octave": 5, "alter": 0, "durationSixteenths": 4, "isRest": false },
                    { "name": "D", "octave": 5, "alter": 0, "durationSixteenths": 4, "isRest": false }],
        "alto":    [{ "name": "C", "octave": 5, "alter": 0, "durationSixteenths": 8, "isRest": false }],
        "tenor":   [{ "name": "G", "octave": 4, "alter": 0, "durationSixteenths": 8, "isRest": false }],
        "bass":    [{ "name": "C", "octave": 3, "alter": 0, "durationSixteenths": 8, "isRest": false }]
      },
      {
        "soprano": [{ "name": "C", "octave": 5, "alter": 0, "durationSixteenths": 16, "isRest": false }],
        "alto":    [{ "name": "E", "octave": 4, "alter": 0, "durationSixteenths": 16, "isRest": false }],
        "tenor":   [{ "name": "G", "octave": 4, "alter": 0, "durationSixteenths": 16, "isRest": false }],
        "bass":    [{ "name": "C", "octave": 3, "alter": 0, "durationSixteenths": 16, "isRest": false }]
      }
    ]
  }
})";

    JobParser parser;
    auto job = parser.parse(json);

    REQUIRE(job.checkSolutionInput.measures.size() == 2);
    const auto& m0 = job.checkSolutionInput.measures[0];
    CHECK(m0.soprano.size() == 2);   // two soprano notes in measure 0
    CHECK(m0.alto.size()    == 1);
    CHECK(m0.tenor.size()   == 1);
    CHECK(m0.bass.size()    == 1);

    const auto& m1 = job.checkSolutionInput.measures[1];
    CHECK(m1.soprano.size() == 1);
    CHECK(m1.alto.size()    == 1);
}

TEST_CASE("CheckSolutionParser: empty voice arrays are valid", "[checkparser]") {
    const std::string json = std::string(R"({"jobId":"cs_empty","mode":"check_solution",)") +
        SETTINGS_FRAGMENT + R"(,
  "input": {
    "measures": [{
      "soprano": [],
      "alto":    [],
      "tenor":   [],
      "bass":    []
    }]
  }
})";

    JobParser parser;
    REQUIRE_NOTHROW(parser.parse(json));
    auto job = parser.parse(json);
    const auto& m = job.checkSolutionInput.measures[0];
    CHECK(m.soprano.empty());
    CHECK(m.bass.empty());
}

// ── parser: validation errors ─────────────────────────────────────────────────

TEST_CASE("CheckSolutionParser: missing input.measures throws", "[checkparser][validation]") {
    const std::string json = std::string(R"({"jobId":"cs_err","mode":"check_solution",)") +
        SETTINGS_FRAGMENT + R"(,
  "input": {}
})";

    JobParser parser;
    CHECK_THROWS_AS(parser.parse(json), std::runtime_error);
}

TEST_CASE("CheckSolutionParser: missing soprano voice throws", "[checkparser][validation]") {
    const std::string json = std::string(R"({"jobId":"cs_err2","mode":"check_solution",)") +
        SETTINGS_FRAGMENT + R"(,
  "input": {
    "measures": [{
      "alto":  [],
      "tenor": [],
      "bass":  []
    }]
  }
})";

    JobParser parser;
    CHECK_THROWS_AS(parser.parse(json), std::runtime_error);
}

TEST_CASE("CheckSolutionParser: durationSixteenths <= 0 throws", "[checkparser][validation]") {
    const std::string json = std::string(R"({"jobId":"cs_err3","mode":"check_solution",)") +
        SETTINGS_FRAGMENT + R"(,
  "input": {
    "measures": [{
      "soprano": [{ "durationSixteenths": 0, "isRest": true }],
      "alto":    [],
      "tenor":   [],
      "bass":    []
    }]
  }
})";

    JobParser parser;
    CHECK_THROWS_AS(parser.parse(json), std::runtime_error);
}

TEST_CASE("CheckSolutionParser: non-rest note missing `name` field throws", "[checkparser][validation]") {
    const std::string json = std::string(R"({"jobId":"cs_err4","mode":"check_solution",)") +
        SETTINGS_FRAGMENT + R"(,
  "input": {
    "measures": [{
      "soprano": [{ "octave": 5, "alter": 0, "durationSixteenths": 4, "isRest": false }],
      "alto":    [],
      "tenor":   [],
      "bass":    []
    }]
  }
})";

    JobParser parser;
    CHECK_THROWS_AS(parser.parse(json), std::runtime_error);
}

// ── dispatcher ────────────────────────────────────────────────────────────────

TEST_CASE("dispatchCheckSolution: returns success, mode=check_solution, errors empty",
          "[checkparser][dispatcher]") {
    JobParser parser;
    auto job = parser.parse(oneNotePerVoiceJson());

    TaskDispatcher dispatcher;
    JobResult result = dispatcher.process(job);

    CHECK(result.status  == "success");
    CHECK(result.mode    == "check_solution");
    CHECK(result.errors.empty());
    CHECK(result.jobId   == "cs_t1");
}

TEST_CASE("dispatchCheckSolution: JSON output has required fields", "[checkparser][dispatcher]") {
    JobParser parser;
    auto job = parser.parse(oneNotePerVoiceJson());

    TaskDispatcher dispatcher;
    auto j = nlohmann::json::parse(dispatcher.process(job).toJson());

    CHECK(j["status"]  == "success");
    CHECK(j["mode"]    == "check_solution");
    // errors is always an array; may be non-empty if test notes fall outside
    // valid SATB ranges (tenor G4 exceeds TENOR_RANGE max F4 → unrecognised chord)
    CHECK(j["errors"].is_array());
}
