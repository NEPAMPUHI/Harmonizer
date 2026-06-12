#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "checking/CheckSolutionRuleChecker.h"
#include "checking/CheckHarmonicPosition.h"
#include "checking/IdentifiedCheckChord.h"
#include "infrastructure/Results.h"
#include "infrastructure/JobParser.h"
#include "application/TaskDispatcher.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static Note pitched(NoteName n, int oct) {
    return Note(n, oct, 0, 0, 4, false);
}
static Note restNote() {
    return Note(NoteName::C, 4, 0, 0, 4, false, false, true);
}

// Build an IdentifiedCheckChord with all 4 voices present and pitched.
// isKnown=true → treated as a valid chord; isKnown=false → will trigger UnknownChord.
//
// When isKnown=true the ic.chord is set to a close-position T-1 triad whose
// notes satisfy all SATB range and spacing constraints, so no additional
// diagnostic errors fire for known chords.
static IdentifiedCheckChord makeIC(bool isKnown, int posIdx = 0) {
    IdentifiedCheckChord ic;
    auto& pos = ic.position;
    pos.measureIndex                = 1;
    pos.positionInMeasureSixteenths = posIdx * 4;
    pos.durationSixteenths          = 4;
    pos.soprano = pitched(NoteName::C, 5); pos.hasSoprano = true;
    pos.alto    = pitched(NoteName::G, 4); pos.hasAlto    = true;
    pos.tenor   = pitched(NoteName::E, 3); pos.hasTenor   = true;
    pos.bass    = pitched(NoteName::C, 3); pos.hasBass    = true;
    ic.isKnownChord = isKnown;

    if (isKnown) {
        ChordTemplate tmpl;
        tmpl.function           = HarmonicFunction::T;
        tmpl.degree             = 1;
        tmpl.type               = ChordType::Triad;
        tmpl.inversion          = Inversion::I;
        tmpl.position           = ChordPosition::Close;
        tmpl.degreesInSatbOrder = {1, 3, 5, 1};
        // Close-position C-major: S=C5 A=E4 T=C4 B=C3 — all voices in range,
        // no adjacent-voice gap > octave, no parallel or hidden intervals.
        ic.chord           = Chord(pitched(NoteName::C, 5),
                                   pitched(NoteName::E, 4),
                                   pitched(NoteName::C, 4),
                                   pitched(NoteName::C, 3), tmpl);
        ic.matchedTemplate = tmpl;
    }
    return ic;
}

// Build an IdentifiedCheckChord where one voice is a rest.
static IdentifiedCheckChord makeICWithRest(int posIdx = 0) {
    auto ic = makeIC(false, posIdx);
    ic.position.bass = restNote();  // bass is a rest
    return ic;
}

// Build an IdentifiedCheckChord where one voice is absent (hasBass=false).
static IdentifiedCheckChord makeICAbsent(int posIdx = 0) {
    auto ic = makeIC(false, posIdx);
    ic.position.hasBass = false;
    return ic;
}

// ── Test 1: all known → no errors ────────────────────────────────────────────

TEST_CASE("CheckSolutionRuleChecker: all known chords → empty error list",
          "[rulechecker]") {
    CheckSolutionRuleChecker checker;
    const auto errors = checker.check({makeIC(true, 0), makeIC(true, 1), makeIC(true, 2)});
    CHECK(errors.empty());
}

// ── Test 2: one unknown → one UnknownChord error ──────────────────────────────

TEST_CASE("CheckSolutionRuleChecker: single unknown chord → 1 error with correct fields",
          "[rulechecker]") {
    CheckSolutionRuleChecker checker;
    const auto errors = checker.check({makeIC(false, 0)});

    REQUIRE(errors.size() == 1);
    CHECK(errors[0].code          == CheckErrorCode::UnknownChord);
    CHECK(errors[0].positionIndex == 0);
    CHECK(errors[0].renderType    == CheckRenderType::ChordMarker);
}

// ── Test 3: mixed list → errors at correct indices ────────────────────────────

TEST_CASE("CheckSolutionRuleChecker: known-unknown-known-unknown → 2 errors at idx 1 and 3",
          "[rulechecker]") {
    CheckSolutionRuleChecker checker;
    const auto errors = checker.check({
        makeIC(true,  0),
        makeIC(false, 1),
        makeIC(true,  2),
        makeIC(false, 3),
    });

    REQUIRE(errors.size() == 2);
    CHECK(errors[0].positionIndex == 1);
    CHECK(errors[1].positionIndex == 3);
    CHECK(errors[0].code == CheckErrorCode::UnknownChord);
    CHECK(errors[1].code == CheckErrorCode::UnknownChord);
}

// ── Test 4: rest in a voice → silently skipped (no error) ─────────────────────

TEST_CASE("CheckSolutionRuleChecker: position with rest voice → no error",
          "[rulechecker]") {
    CheckSolutionRuleChecker checker;

    // rest in one voice
    const auto errorsRest = checker.check({makeICWithRest(0)});
    CHECK(errorsRest.empty());

    // absent voice (hasX = false)
    const auto errorsAbsent = checker.check({makeICAbsent(0)});
    CHECK(errorsAbsent.empty());
}

// ── Test 5: JSON serialization round-trip ─────────────────────────────────────

TEST_CASE("CheckSolutionRuleChecker: errors appear in toJson() output", "[rulechecker]") {
    JobResult result = JobResult::success("job-json-test");
    result.mode = "check_solution";

    CheckError err;
    err.code          = CheckErrorCode::UnknownChord;
    err.message       = "Unknown chord";
    err.positionIndex = 3;
    err.renderType    = CheckRenderType::ChordMarker;
    result.checkErrors.push_back(err);

    const auto j = nlohmann::json::parse(result.toJson());

    REQUIRE(j["errors"].is_array());
    REQUIRE(j["errors"].size() == 1);
    CHECK(j["errors"][0]["code"]         == "UnknownChord");
    CHECK(j["errors"][0]["renderType"]   == "ChordMarker");
    CHECK(j["errors"][0]["positionIndex"] == 3);
    CHECK(j["mode"]                       == "check_solution");
}

// ── Test 6: dispatcher end-to-end — unknown chord appears in JSON ─────────────

TEST_CASE("dispatchCheckSolution: chromatic cluster produces UnknownChord in response JSON",
          "[rulechecker][dispatcher]") {
    // Build a job where 1 measure has 4 chromatic notes (not a valid chord in C major)
    static const char* jsonStr = R"({
      "jobId": "rc_e2e",
      "mode": "check_solution",
      "settings": {
        "key": "C",
        "measureCount": 1,
        "timeSignature": { "beats": 4, "beatType": 4 },
        "anacrusisSixteenths": 0,
        "forbiddenRules": [],
        "allowedChords": []
      },
      "input": {
        "measures": [{
          "soprano": [{ "name": "D", "octave": 5, "alter": 1, "durationSixteenths": 4, "isRest": false }],
          "alto":    [{ "name": "C", "octave": 5, "alter": 1, "durationSixteenths": 4, "isRest": false }],
          "tenor":   [{ "name": "A", "octave": 4, "alter": 1, "durationSixteenths": 4, "isRest": false }],
          "bass":    [{ "name": "F", "octave": 3, "alter": 1, "durationSixteenths": 4, "isRest": false }]
        }]
      }
    })";

    JobParser parser;
    const auto job = parser.parse(jsonStr);

    TaskDispatcher dispatcher;
    const auto result = dispatcher.process(job);
    const auto j = nlohmann::json::parse(result.toJson());

    CHECK(j["status"] == "success");
    CHECK(j["mode"]   == "check_solution");

    REQUIRE(j["errors"].is_array());
    REQUIRE(j["errors"].size() >= 1);
    CHECK(j["errors"][0]["code"]      == "UnknownChord");
    CHECK(j["errors"][0]["renderType"] == "ChordMarker");
    CHECK(j["errors"][0].contains("positionIndex"));
}
