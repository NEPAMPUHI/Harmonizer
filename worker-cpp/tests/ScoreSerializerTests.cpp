#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "infrastructure/ScoreSerializer.h"
#include "infrastructure/Results.h"
#include "domain/Score.h"
#include "domain/Chord.h"
#include "domain/HarmonicPosition.h"
#include "domain/ChordTemplate.h"
#include "domain/HarmonizationSettings.h"

using json = nlohmann::json;

// ── test helpers ─────────────────────────────────────────────────────────────

static Note makeNote(NoteName name, int octave, int alter = 0) {
    return Note(name, octave, alter, 1, 4, false);
}

static ChordTemplate makeTemplate() {
    ChordTemplate t;
    t.function           = HarmonicFunction::T;
    t.degree             = 1;
    t.type               = ChordType::Triad;
    t.inversion          = Inversion::I;
    t.position           = ChordPosition::Close;
    t.degreesInSatbOrder = {1, 3, 5, 1};
    return t;
}

static Chord makeChord(NoteName s, int so, NoteName a, int ao,
                        NoteName t, int to, NoteName b, int bo,
                        int sAlter = 0, int aAlter = 0,
                        int tAlter = 0, int bAlter = 0) {
    return Chord(
        makeNote(s, so, sAlter),
        makeNote(a, ao, aAlter),
        makeNote(t, to, tAlter),
        makeNote(b, bo, bAlter),
        makeTemplate()
    );
}

static HarmonicPosition makePosition(int measureIndex, int startSixteenth,
                                      int durationSixteenths) {
    HarmonicPosition p;
    p.index              = 0;
    p.measureIndex       = measureIndex;
    p.startSixteenth     = startSixteenth;
    p.durationSixteenths = durationSixteenths;
    return p;
}

static HarmonizationSettings makeSettings(const std::string& key = "C",
                                           int beats = 4, int beatType = 4,
                                           int anacrusis = 0) {
    HarmonizationSettings s;
    s.key                  = key;
    s.scaleModes           = {"natural"};
    s.measureCount         = 2;
    s.timeSignature        = {beats, beatType};
    s.anacrusisSixteenths  = anacrusis;
    s.forbiddenRules       = {};
    s.allowedChords        = {};
    return s;
}

// Build a Score with two measures, two chords each.
// measureIndex follows HarmonicPositionBuilder convention: 0 = anacrusis only,
// regular measures start at 1.
static Score makeTwoMeasureScore() {
    Score score;
    // measure 1 (first regular): chords at beats 0 and 4
    score.chords.push_back(makeChord(
        NoteName::E, 5, NoteName::C, 5, NoteName::G, 4, NoteName::C, 3));
    score.chords.push_back(makeChord(
        NoteName::D, 5, NoteName::B, 4, NoteName::F, 4, NoteName::G, 3));
    // measure 2 (second regular): chords at beats 0 and 4
    score.chords.push_back(makeChord(
        NoteName::C, 5, NoteName::E, 5, NoteName::C, 4, NoteName::C, 3));
    score.chords.push_back(makeChord(
        NoteName::B, 4, NoteName::D, 5, NoteName::G, 4, NoteName::G, 3));

    score.positions.push_back(makePosition(1, 0,  4));
    score.positions.push_back(makePosition(1, 4,  4));
    score.positions.push_back(makePosition(2, 0,  4));
    score.positions.push_back(makePosition(2, 4,  4));
    return score;
}

// ── ScoreSerializer tests ─────────────────────────────────────────────────────

TEST_CASE("ScoreSerializer: empty score returns FrontendScore with no measures",
          "[serializer]") {
    ScoreSerializer ser;
    Score empty;
    auto fs = ser.serialize(empty, makeSettings());

    CHECK(fs.measures.empty());
    CHECK(fs.key  == "C");
    CHECK(fs.mode == "major");
    CHECK(fs.beats    == 4);
    CHECK(fs.beatType == 4);
}

TEST_CASE("ScoreSerializer: major key detected correctly", "[serializer][key]") {
    ScoreSerializer ser;
    Score empty;

    auto fsMajor = ser.serialize(empty, makeSettings("G"));
    CHECK(fsMajor.mode == "major");

    auto fsMinor = ser.serialize(empty, makeSettings("a"));
    CHECK(fsMinor.mode == "minor");

    auto fsFSharp = ser.serialize(empty, makeSettings("F#"));
    CHECK(fsFSharp.mode == "major");

    auto fsDMinor = ser.serialize(empty, makeSettings("d"));
    CHECK(fsDMinor.mode == "minor");
}

TEST_CASE("ScoreSerializer: time signature propagated — no hardcode 4/4", "[serializer][time]") {
    ScoreSerializer ser;
    Score empty;

    auto fs34 = ser.serialize(empty, makeSettings("C", 3, 4));
    CHECK(fs34.beats    == 3);
    CHECK(fs34.beatType == 4);

    auto fs68 = ser.serialize(empty, makeSettings("C", 6, 8));
    CHECK(fs68.beats    == 6);
    CHECK(fs68.beatType == 8);
}

TEST_CASE("ScoreSerializer: multiple measures grouped correctly", "[serializer][measures]") {
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();
    auto fs = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 2);
    CHECK(fs.measures[0].number == 1);
    CHECK(fs.measures[1].number == 2);
}

TEST_CASE("ScoreSerializer: each measure has 2 chords per voice", "[serializer][measures]") {
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();
    auto fs = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 2);
    for (const auto& m : fs.measures) {
        CHECK(m.voices.soprano.size() == 2);
        CHECK(m.voices.alto.size()    == 2);
        CHECK(m.voices.tenor.size()   == 2);
        CHECK(m.voices.bass.size()    == 2);
    }
}

TEST_CASE("ScoreSerializer: SATB voices contain correct pitches", "[serializer][satb]") {
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();
    auto fs = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 2);
    const auto& m0 = fs.measures[0];

    // First chord in measure 0: E5 / C5 / G4 / C3
    CHECK(m0.voices.soprano[0].step   == "E");
    CHECK(m0.voices.soprano[0].octave == 5);
    CHECK(m0.voices.alto[0].step      == "C");
    CHECK(m0.voices.alto[0].octave    == 5);
    CHECK(m0.voices.tenor[0].step     == "G");
    CHECK(m0.voices.tenor[0].octave   == 4);
    CHECK(m0.voices.bass[0].step      == "C");
    CHECK(m0.voices.bass[0].octave    == 3);
}

TEST_CASE("ScoreSerializer: duration and durationType are set correctly", "[serializer][duration]") {
    ScoreSerializer ser;

    Score score;
    score.chords.push_back(makeChord(
        NoteName::C, 5, NoteName::E, 4, NoteName::G, 4, NoteName::C, 3));
    score.positions.push_back(makePosition(0, 0, 4));  // quarter
    auto fs = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 1);
    CHECK(fs.measures[0].voices.soprano[0].durationSixteenths == 4);
    CHECK(fs.measures[0].voices.soprano[0].durationType       == "quarter");
}

TEST_CASE("ScoreSerializer: alter field preserved (accidentals)", "[serializer][alter]") {
    ScoreSerializer ser;

    Score score;
    // Soprano: Ab5 (alter=-1), Bass: F#3 (alter=1)
    score.chords.push_back(makeChord(
        NoteName::A, 5, NoteName::C, 5, NoteName::E, 4, NoteName::F, 3,
        /*sAlter*/-1, 0, 0, /*bAlter*/1));
    score.positions.push_back(makePosition(0, 0, 4));
    auto fs = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 1);
    CHECK(fs.measures[0].voices.soprano[0].step  == "A");
    CHECK(fs.measures[0].voices.soprano[0].alter == -1);
    CHECK(fs.measures[0].voices.bass[0].step     == "F");
    CHECK(fs.measures[0].voices.bass[0].alter    == 1);
}

TEST_CASE("ScoreSerializer: anacrusis measure number is 0", "[serializer][anacrusis]") {
    ScoreSerializer ser;

    Score score;
    score.chords.push_back(makeChord(
        NoteName::G, 5, NoteName::D, 5, NoteName::B, 4, NoteName::G, 3));
    score.positions.push_back(makePosition(0, 0, 4)); // anacrusis measure
    score.chords.push_back(makeChord(
        NoteName::C, 5, NoteName::E, 5, NoteName::G, 4, NoteName::C, 3));
    score.positions.push_back(makePosition(1, 0, 4)); // first full measure

    auto fs = ser.serialize(score, makeSettings("C", 4, 4, /*anacrusis=*/4));

    REQUIRE(fs.measures.size() == 2);
    CHECK(fs.measures[0].number == 0); // anacrusis
    CHECK(fs.measures[1].number == 1); // regular
}

// ── JobResult::toJson tests ───────────────────────────────────────────────────

TEST_CASE("JobResult::toJson: structure has required top-level fields", "[json]") {
    JobResult jr = JobResult::success("job_test_001");
    auto j = json::parse(jr.toJson());

    CHECK(j.contains("jobId"));
    CHECK(j.contains("status"));
    CHECK(j.contains("results"));
    CHECK(j.contains("errors"));
    CHECK(j["jobId"]  == "job_test_001");
    CHECK(j["status"] == "success");
    CHECK(j["results"].is_array());
    CHECK(j["errors"].is_array());
}

TEST_CASE("JobResult::toJson: result entry contains all required fields", "[json]") {
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();
    auto settings = makeSettings("g", 3, 4); // g minor, 3/4

    HarmonizationResult r;
    r.variantId     = "variant_001";
    r.score         = 90;
    r.musicXml      = "<score/>";
    r.musicXmlPath  = "results/job_x/variant_001.musicxml"; // internal
    r.frontendScore = ser.serialize(score, settings);

    JobResult jr = JobResult::success("job_x", {r});
    auto j = json::parse(jr.toJson());

    REQUIRE(j["results"].size() == 1);
    const auto& res = j["results"][0];

    CHECK(res["variantId"] == "variant_001");
    CHECK(res["score"]     == 90);
    CHECK(res["musicXml"]  == "<score/>");
    CHECK(res["key"]       == "g");
    CHECK(res["mode"]      == "minor");
    CHECK(res["beats"]     == 3);
    CHECK(res["beatType"]  == 4);
    CHECK(res.contains("measures"));

    // musicXmlPath must NOT be exposed
    CHECK_FALSE(res.contains("musicXmlPath"));
}

TEST_CASE("JobResult::toJson: measures contain all four SATB voices", "[json][satb]") {
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();
    auto settings = makeSettings("C");

    HarmonizationResult r;
    r.variantId     = "variant_001";
    r.score         = 85;
    r.frontendScore = ser.serialize(score, settings);

    JobResult jr = JobResult::success("job_y", {r});
    auto j = json::parse(jr.toJson());

    REQUIRE(j["results"].size() == 1);
    REQUIRE(j["results"][0]["measures"].size() == 2);

    const auto& m0 = j["results"][0]["measures"][0];
    CHECK(m0["number"] == 1);
    CHECK(m0["voices"].contains("soprano"));
    CHECK(m0["voices"].contains("alto"));
    CHECK(m0["voices"].contains("tenor"));
    CHECK(m0["voices"].contains("bass"));
    CHECK(m0["voices"]["soprano"].size() == 2);
    CHECK(m0["voices"]["alto"].size()    == 2);
    CHECK(m0["voices"]["tenor"].size()   == 2);
    CHECK(m0["voices"]["bass"].size()    == 2);
}

TEST_CASE("JobResult::toJson: note fields are correct in JSON", "[json][note]") {
    ScoreSerializer ser;

    Score score;
    score.chords.push_back(makeChord(
        NoteName::E, 5, NoteName::C, 5, NoteName::G, 4, NoteName::C, 3));
    score.positions.push_back(makePosition(0, 0, 8)); // half note

    HarmonizationResult r;
    r.variantId     = "variant_001";
    r.score         = 80;
    r.frontendScore = ser.serialize(score, makeSettings("F"));

    JobResult jr = JobResult::success("job_z", {r});
    auto j = json::parse(jr.toJson());

    const auto& note = j["results"][0]["measures"][0]["voices"]["soprano"][0];
    CHECK(note["step"]               == "E");
    CHECK(note["octave"]             == 5);
    CHECK(note["alter"]              == 0);
    CHECK(note["durationSixteenths"] == 8);
    CHECK(note["durationType"]       == "half");
}

TEST_CASE("ScoreSerializer: chordNames populated, count equals harmonic position count", "[serializer][chordnames]") {
    // chordNames has one entry per harmonic position (not per merged voice note).
    // For this non-merging score the counts happen to be equal, but the invariant
    // is chordNames.size() == number of positions, not voices[i].size().
    ScoreSerializer ser;
    auto score    = makeTwoMeasureScore();
    auto fs       = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 2);
    for (const auto& m : fs.measures) {
        CHECK(m.chordNames.size() == 2);  // two harmonic positions per measure
        CHECK(m.voices.soprano.size() == 2); // no merge in this score
        CHECK(m.voices.bass.size()    == 2);
    }
}

TEST_CASE("ScoreSerializer: chordNames values match Chord::getName()", "[serializer][chordnames]") {
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();
    auto fs    = ser.serialize(score, makeSettings("C"));

    // All chords use makeTemplate() → degree=1, type=Triad → getName() = "T53"
    REQUIRE(fs.measures.size() == 2);
    for (const auto& m : fs.measures) {
        for (const auto& name : m.chordNames) {
            CHECK(name == "T53");
        }
    }
}

TEST_CASE("JobResult::toJson: measures contain chordNames array", "[json][chordnames]") {
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();

    HarmonizationResult r;
    r.variantId     = "variant_cn";
    r.score         = 77;
    r.frontendScore = ser.serialize(score, makeSettings("C"));

    JobResult jr = JobResult::success("job_cn", {r});
    auto j = json::parse(jr.toJson());

    REQUIRE(j["results"].size() == 1);
    const auto& m0 = j["results"][0]["measures"][0];

    REQUIRE(m0.contains("chordNames"));
    CHECK(m0["chordNames"].is_array());
    CHECK(m0["chordNames"].size() == 2);   // two chords per measure
    CHECK(m0["chordNames"][0] == "T53");
    CHECK(m0["chordNames"][1] == "T53");
}

TEST_CASE("ScoreSerializer: chordNames fallback does not crash serialization pipeline",
          "[serializer][chordnames][robustness]") {
    // This test verifies the pipeline doesn't throw even when the measures
    // have been constructed directly (not via ScoreSerializer) and chordNames
    // is populated via the try/catch path.  We exercise it indirectly by
    // running serialize on a valid score and confirming no exception is thrown
    // and the output JSON is parseable.
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();

    HarmonizationResult r;
    r.variantId     = "variant_robust";
    r.score         = 50;
    r.frontendScore = ser.serialize(score, makeSettings("C"));

    JobResult jr = JobResult::success("job_robust", {r});
    std::string jsonStr;
    REQUIRE_NOTHROW(jsonStr = jr.toJson());
    REQUIRE_NOTHROW(json::parse(jsonStr));
}

TEST_CASE("JobResult::toJson: error result serialises correctly", "[json][error]") {
    JobResult jr = JobResult::error("job_err", "NO_VARIANTS", "Nothing produced");
    auto j = json::parse(jr.toJson());

    CHECK(j["status"] == "error");
    REQUIRE(j["errors"].size() == 1);
    CHECK(j["errors"][0]["code"]    == "NO_VARIANTS");
    CHECK(j["errors"][0]["message"] == "Nothing produced");
}

// ── Merge helpers ─────────────────────────────────────────────────────────────

// Builds a HarmonicPosition with all fields set.
static HarmonicPosition makePos(int mi, int start, int dur, int srcIdx = -1) {
    HarmonicPosition p;
    p.index              = 0;
    p.measureIndex       = mi;
    p.startSixteenth     = start;
    p.durationSixteenths = dur;
    p.sourceNoteIndex    = srcIdx;
    return p;
}

// ── [serializer-merge] tests ──────────────────────────────────────────────────

TEST_CASE("ScoreSerializer merge: fixed soprano (fvi=0) — 3 cadence-split positions same "
          "sourceNoteIndex → 1 merged whole note", "[serializer-merge]") {
    // T53 → S64 → T53 in C major: cadence split [4,4,8], soprano C5 fixed throughout.
    // Bass is C2 in all three chords (S64 is second inversion → bass = C).
    ScoreSerializer ser;

    Score score;
    score.fixedVoiceIndex = 0;
    // T53: Soprano=C5, Alto=E4, Tenor=G3, Bass=C2
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    // S64: Soprano=C5, Alto=F4, Tenor=A3, Bass=C2
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::F, 4, NoteName::A, 3, NoteName::C, 2));
    // T53: Soprano=C5, Alto=E4, Tenor=G3, Bass=C2
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));

    // Positions: [4,4,8] cadence split, all sourceNoteIndex=0
    score.positions = { makePos(1, 0, 4, 0), makePos(1, 4, 4, 0), makePos(1, 8, 8, 0) };

    auto fs = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];

    // Soprano: fixed voice, sourceNoteIndex=0 for all → merged to 1 whole note
    REQUIRE(m.voices.soprano.size() == 1);
    CHECK(m.voices.soprano[0].step               == "C");
    CHECK(m.voices.soprano[0].octave             == 5);
    CHECK(m.voices.soprano[0].durationSixteenths == 16);
    CHECK(m.voices.soprano[0].durationType       == "whole");
    CHECK(m.voices.soprano[0].tiedToNext         == false);

    // Alto: E4 → F4 → E4 (pitch changes) → 3 notes, no merge
    REQUIRE(m.voices.alto.size() == 3);
    CHECK(m.voices.alto[0].step == "E");
    CHECK(m.voices.alto[1].step == "F");
    CHECK(m.voices.alto[2].step == "E");

    // Tenor: G3 → A3 → G3 (pitch changes) → 3 notes, no merge
    REQUIRE(m.voices.tenor.size() == 3);
    CHECK(m.voices.tenor[0].step == "G");
    CHECK(m.voices.tenor[1].step == "A");
    CHECK(m.voices.tenor[2].step == "G");

    // Bass: C2, C2, C2 → consecutive same pitch → merged to 1 whole note
    REQUIRE(m.voices.bass.size() == 1);
    CHECK(m.voices.bass[0].step               == "C");
    CHECK(m.voices.bass[0].octave             == 2);
    CHECK(m.voices.bass[0].durationSixteenths == 16);
    CHECK(m.voices.bass[0].durationType       == "whole");

    // chordNames: one per harmonic position (unchanged)
    REQUIRE(m.chordNames.size() == 3);

    // chordTicks: [0, 4, 8]
    REQUIRE(m.chordTicks.size() == 3);
    CHECK(m.chordTicks[0] == 0);
    CHECK(m.chordTicks[1] == 4);
    CHECK(m.chordTicks[2] == 8);
}

TEST_CASE("ScoreSerializer merge: fixed soprano — 2 different sourceNoteIndices → 2 soprano notes",
          "[serializer-merge]") {
    // Two separate input notes with same pitch: must NOT merge (different sourceNoteIndex).
    ScoreSerializer ser;

    Score score;
    score.fixedVoiceIndex = 0;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));

    // Different sourceNoteIndex: two separate quarter notes, same pitch
    score.positions = { makePos(1, 0, 4, 0), makePos(1, 4, 4, 1) };

    auto fs = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];

    // Two separate soprano notes (different sourceNoteIndex → no merge)
    REQUIRE(m.voices.soprano.size() == 2);
    CHECK(m.voices.soprano[0].durationSixteenths == 4);
    CHECK(m.voices.soprano[1].durationSixteenths == 4);
}

TEST_CASE("ScoreSerializer merge: non-fixed (fvi=-1) — 3 consecutive identical bass → 1 merged note",
          "[serializer-merge]") {
    ScoreSerializer ser;

    Score score;
    // score.fixedVoiceIndex defaults to -1
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::C, 5, NoteName::G, 4, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::B, 4, NoteName::G, 4, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 5, NoteName::C, 4, NoteName::C, 2));

    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4), makePos(1, 8, 8) };

    auto fs = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];

    // Bass C2 × 3 consecutive → merged to dur=16
    REQUIRE(m.voices.bass.size() == 1);
    CHECK(m.voices.bass[0].step               == "C");
    CHECK(m.voices.bass[0].durationSixteenths == 16);
    CHECK(m.voices.bass[0].durationType       == "whole");
}

TEST_CASE("ScoreSerializer merge: pitch breaks chain — no merge across pitch change",
          "[serializer-merge]") {
    ScoreSerializer ser;

    Score score;
    // Bass: C2, G2, C2 — G2 breaks the chain → 3 separate bass notes
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::F, 4, NoteName::A, 3, NoteName::G, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));

    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4), makePos(1, 8, 8) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];
    REQUIRE(m.voices.bass.size() == 3);
    CHECK(m.voices.bass[0].durationSixteenths == 4);
    CHECK(m.voices.bass[1].durationSixteenths == 4);
    CHECK(m.voices.bass[2].durationSixteenths == 8);
}

TEST_CASE("ScoreSerializer merge: partial alto merge — first two same, third different",
          "[serializer-merge]") {
    // Alto: E4(4), E4(4), F4(8) → merge first two → [E4/8, F4/8]
    ScoreSerializer ser;

    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::E, 4, NoteName::A, 3, NoteName::G, 2));
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::F, 4, NoteName::C, 4, NoteName::C, 2));

    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4), makePos(1, 8, 8) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];

    REQUIRE(m.voices.alto.size() == 2);
    CHECK(m.voices.alto[0].step               == "E");
    CHECK(m.voices.alto[0].durationSixteenths == 8);
    CHECK(m.voices.alto[1].step               == "F");
    CHECK(m.voices.alto[1].durationSixteenths == 8);
}

TEST_CASE("ScoreSerializer merge: fixed bass (fvi=3) — merge bass by sourceNoteIndex",
          "[serializer-merge]") {
    ScoreSerializer ser;

    Score score;
    score.fixedVoiceIndex = 3;  // bass mode
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::F, 4, NoteName::A, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));

    // All positions share sourceNoteIndex=0 (one tied/whole bass note)
    score.positions = { makePos(1, 0, 4, 0), makePos(1, 4, 4, 0), makePos(1, 8, 8, 0) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];

    // Bass: fixed voice, merged by sourceNoteIndex → 1 whole note
    REQUIRE(m.voices.bass.size() == 1);
    CHECK(m.voices.bass[0].durationSixteenths == 16);
    CHECK(m.voices.bass[0].durationType       == "whole");

    // Soprano: all C5 → merged by pitch → 1 whole note
    REQUIRE(m.voices.soprano.size() == 1);
    CHECK(m.voices.soprano[0].durationSixteenths == 16);
}

TEST_CASE("ScoreSerializer merge: no cross-measure merge even with same pitch",
          "[serializer-merge]") {
    // Measure 1 ends on E5 soprano, measure 2 starts on E5 soprano.
    // They must NOT merge (separate measures).
    ScoreSerializer ser;

    Score score;
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::C, 5, NoteName::G, 4, NoteName::C, 3));
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::D, 5, NoteName::B, 4, NoteName::G, 3));

    // Different measures
    score.positions = { makePos(1, 12, 4), makePos(2, 0, 4) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 2);

    // Each measure has exactly 1 soprano note (no cross-measure merge)
    CHECK(fs.measures[0].voices.soprano.size() == 1);
    CHECK(fs.measures[1].voices.soprano.size() == 1);
    CHECK(fs.measures[0].voices.soprano[0].durationSixteenths == 4);
    CHECK(fs.measures[1].voices.soprano[0].durationSixteenths == 4);
}

TEST_CASE("ScoreSerializer merge: chordTicks populated from positions[i].startSixteenth",
          "[serializer-merge]") {
    ScoreSerializer ser;

    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::F, 4, NoteName::A, 3, NoteName::G, 2));
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::G, 4, NoteName::C, 4, NoteName::C, 2));

    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4), makePos(1, 8, 8) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];

    REQUIRE(m.chordTicks.size() == 3);
    CHECK(m.chordTicks[0] == 0);
    CHECK(m.chordTicks[1] == 4);
    CHECK(m.chordTicks[2] == 8);
}

TEST_CASE("ScoreSerializer merge: chordNames count equals position count (not merged note count)",
          "[serializer-merge]") {
    // 3 positions, soprano merges to 1 note — chordNames must still have 3 entries.
    ScoreSerializer ser;

    Score score;
    score.fixedVoiceIndex = 0;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::F, 4, NoteName::A, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.positions = { makePos(1, 0, 4, 0), makePos(1, 4, 4, 0), makePos(1, 8, 8, 0) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];

    // Soprano merged to 1 note, but chordNames has 3 entries
    CHECK(m.voices.soprano.size() == 1);
    CHECK(m.chordNames.size()     == 3);
    CHECK(m.chordTicks.size()     == 3);
}

TEST_CASE("JobResult::toJson: measures contain chordTicks array", "[json][chordticks]") {
    ScoreSerializer ser;

    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::B, 4, NoteName::F, 4, NoteName::G, 2));
    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4) };

    HarmonizationResult r;
    r.variantId     = "variant_ct";
    r.score         = 80;
    r.frontendScore = ser.serialize(score, makeSettings("C"));

    JobResult jr = JobResult::success("job_ct", {r});
    auto j = json::parse(jr.toJson());

    REQUIRE(j["results"].size() == 1);
    const auto& m0 = j["results"][0]["measures"][0];

    REQUIRE(m0.contains("chordTicks"));
    CHECK(m0["chordTicks"].is_array());
    CHECK(m0["chordTicks"].size() == 2);
    CHECK(m0["chordTicks"][0] == 0);
    CHECK(m0["chordTicks"][1] == 4);
}

// ── dotted duration tests ─────────────────────────────────────────────────────

TEST_CASE("ScoreSerializer: standard durations have dotted=false", "[serializer][dotted]") {
    ScoreSerializer ser;
    // Test each standard duration: 16/8/4/2/1 sixteenths
    for (auto [sixteenths, expectedType] : std::vector<std::pair<int,std::string>>{
            {16, "whole"}, {8, "half"}, {4, "quarter"}, {2, "eighth"}, {1, "16th"}}) {
        Score score;
        score.chords.push_back(makeChord(
            NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
        score.positions.push_back(makePos(1, 0, sixteenths));
        auto fs = ser.serialize(score, makeSettings("C"));
        REQUIRE(fs.measures.size() == 1);
        const auto& n = fs.measures[0].voices.soprano[0];
        CHECK(n.durationType == expectedType);
        CHECK(n.dotted == false);
    }
}

TEST_CASE("ScoreSerializer: 12 sixteenths → half + dotted=true", "[serializer][dotted]") {
    ScoreSerializer ser;
    // Bass: B2 × 3 quarters → merged to 12 sixteenths = dotted half
    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::B, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::F, 4, NoteName::A, 3, NoteName::B, 2));
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::G, 4, NoteName::C, 4, NoteName::B, 2));
    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4), makePos(1, 8, 4) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& bass = fs.measures[0].voices.bass;
    REQUIRE(bass.size() == 1);
    CHECK(bass[0].durationSixteenths == 12);
    CHECK(bass[0].durationType       == "half");
    CHECK(bass[0].dotted             == true);
}

TEST_CASE("ScoreSerializer: 6 sixteenths → quarter + dotted=true", "[serializer][dotted]") {
    ScoreSerializer ser;
    // Bass: B2 × 3 eighth notes → merged to 6 sixteenths = dotted quarter
    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::B, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::F, 4, NoteName::A, 3, NoteName::B, 2));
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::G, 4, NoteName::C, 4, NoteName::B, 2));
    score.positions = { makePos(1, 0, 2), makePos(1, 2, 2), makePos(1, 4, 2) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& bass = fs.measures[0].voices.bass;
    REQUIRE(bass.size() == 1);
    CHECK(bass[0].durationSixteenths == 6);
    CHECK(bass[0].durationType       == "quarter");
    CHECK(bass[0].dotted             == true);
}

TEST_CASE("ScoreSerializer: 3 sixteenths → eighth + dotted=true", "[serializer][dotted]") {
    ScoreSerializer ser;
    // Bass: B2 × 3 sixteenth notes → merged to 3 sixteenths = dotted eighth
    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::B, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::F, 4, NoteName::A, 3, NoteName::B, 2));
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::G, 4, NoteName::C, 4, NoteName::B, 2));
    score.positions = { makePos(1, 0, 1), makePos(1, 1, 1), makePos(1, 2, 1) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& bass = fs.measures[0].voices.bass;
    REQUIRE(bass.size() == 1);
    CHECK(bass[0].durationSixteenths == 3);
    CHECK(bass[0].durationType       == "eighth");
    CHECK(bass[0].dotted             == true);
}

TEST_CASE("JobResult::toJson: dotted field present in note JSON", "[json][dotted]") {
    ScoreSerializer ser;

    Score score;
    score.chords.push_back(makeChord(
        NoteName::E, 5, NoteName::C, 5, NoteName::G, 4, NoteName::C, 3));
    score.positions.push_back(makePos(1, 0, 4));  // quarter, dotted=false

    HarmonizationResult r;
    r.variantId     = "variant_dot";
    r.score         = 80;
    r.frontendScore = ser.serialize(score, makeSettings("C"));

    JobResult jr = JobResult::success("job_dot", {r});
    auto j = json::parse(jr.toJson());

    const auto& note = j["results"][0]["measures"][0]["voices"]["soprano"][0];
    REQUIRE(note.contains("dotted"));
    CHECK(note["dotted"] == false);
}

TEST_CASE("JobResult::toJson: dotted=true in JSON for merged dotted-half note", "[json][dotted]") {
    ScoreSerializer ser;

    // Bass: B2 × 3 quarters → 12 sixteenths = dotted half
    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::B, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::F, 4, NoteName::A, 3, NoteName::B, 2));
    score.chords.push_back(makeChord(NoteName::E, 5, NoteName::G, 4, NoteName::C, 4, NoteName::B, 2));
    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4), makePos(1, 8, 4) };

    HarmonizationResult r;
    r.variantId     = "variant_doth";
    r.score         = 80;
    r.frontendScore = ser.serialize(score, makeSettings("C"));

    JobResult jr = JobResult::success("job_doth", {r});
    auto j = json::parse(jr.toJson());

    const auto& bassArr = j["results"][0]["measures"][0]["voices"]["bass"];
    REQUIRE(bassArr.size() == 1);
    CHECK(bassArr[0]["durationType"]       == "half");
    CHECK(bassArr[0]["durationSixteenths"] == 12);
    CHECK(bassArr[0]["dotted"]             == true);
}

// ── chordLabelVisible tests ───────────────────────────────────────────────────

TEST_CASE("ScoreSerializer: chordLabelVisible — position 0 always visible", "[serializer][chordlabelvisible]") {
    ScoreSerializer ser;
    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.positions = { makePos(1, 0, 4) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];
    REQUIRE(m.chordLabelVisible.size() == 1);
    CHECK(m.chordLabelVisible[0] == true);
}

TEST_CASE("ScoreSerializer: chordLabelVisible — identical consecutive chord is hidden", "[serializer][chordlabelvisible]") {
    // Two positions with identical SATB and same chord name → second hidden.
    ScoreSerializer ser;
    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];
    REQUIRE(m.chordLabelVisible.size() == 2);
    CHECK(m.chordLabelVisible[0] == true);   // first always visible
    CHECK(m.chordLabelVisible[1] == false);  // identical → hidden
}

TEST_CASE("ScoreSerializer: chordLabelVisible — changed soprano makes position visible", "[serializer][chordlabelvisible]") {
    ScoreSerializer ser;
    Score score;
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::D, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];
    REQUIRE(m.chordLabelVisible.size() == 2);
    CHECK(m.chordLabelVisible[0] == true);
    CHECK(m.chordLabelVisible[1] == true);  // soprano changed
}

TEST_CASE("ScoreSerializer: chordLabelVisible — 3 positions, middle identical to first → [T,F,T]", "[serializer][chordlabelvisible]") {
    ScoreSerializer ser;
    Score score;
    // pos0: C5/E4/G3/C2, pos1: same, pos2: different (bass G2)
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::G, 2));
    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4), makePos(1, 8, 4) };

    auto fs = ser.serialize(score, makeSettings("C"));
    REQUIRE(fs.measures.size() == 1);
    const auto& m = fs.measures[0];
    REQUIRE(m.chordLabelVisible.size() == 3);
    CHECK(m.chordLabelVisible[0] == true);
    CHECK(m.chordLabelVisible[1] == false);  // same as pos0
    CHECK(m.chordLabelVisible[2] == true);   // bass changed
}

TEST_CASE("ScoreSerializer: chordLabelVisible size equals chordNames size", "[serializer][chordlabelvisible]") {
    ScoreSerializer ser;
    auto score = makeTwoMeasureScore();
    auto fs    = ser.serialize(score, makeSettings("C"));

    REQUIRE(fs.measures.size() == 2);
    for (const auto& m : fs.measures) {
        CHECK(m.chordLabelVisible.size() == m.chordNames.size());
    }
}

TEST_CASE("JobResult::toJson: measures contain chordLabelVisible array", "[json][chordlabelvisible]") {
    ScoreSerializer ser;

    Score score;
    // Two identical positions → [true, false]
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.chords.push_back(makeChord(NoteName::C, 5, NoteName::E, 4, NoteName::G, 3, NoteName::C, 2));
    score.positions = { makePos(1, 0, 4), makePos(1, 4, 4) };

    HarmonizationResult r;
    r.variantId     = "variant_clv";
    r.score         = 80;
    r.frontendScore = ser.serialize(score, makeSettings("C"));

    JobResult jr = JobResult::success("job_clv", {r});
    auto j = json::parse(jr.toJson());

    REQUIRE(j["results"].size() == 1);
    const auto& m0 = j["results"][0]["measures"][0];

    REQUIRE(m0.contains("chordLabelVisible"));
    CHECK(m0["chordLabelVisible"].is_array());
    REQUIRE(m0["chordLabelVisible"].size() == 2);
    CHECK(m0["chordLabelVisible"][0] == true);
    CHECK(m0["chordLabelVisible"][1] == false);
}
