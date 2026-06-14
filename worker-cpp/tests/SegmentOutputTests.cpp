#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <numeric>
#include <string>

#include "domain/HarmonicPosition.h"
#include "domain/HarmonicSegment.h"
#include "domain/Score.h"
#include "domain/Chord.h"
#include "domain/ChordTemplate.h"
#include "domain/HarmonizationSettings.h"
#include "domain/ScoreInput.h"
#include "domain/HarmonicPositionBuilder.h"
#include "harmonization/HarmonicRhythmPlanner.h"
#include "harmonization/MelodyHarmonizer.h"
#include "harmonization/BassHarmonizer.h"
#include "infrastructure/ScoreSerializer.h"
#include "infrastructure/MusicXmlWriter.h"
#include "infrastructure/Results.h"

using json = nlohmann::json;

// ── Helpers ───────────────────────────────────────────────────────────────────

static Note mkNote(NoteName n, int oct, int dur = 4, bool rest = false) {
    return Note(n, oct, 0, 0, dur, /*isStrongBeat=*/true, /*tiedToNext=*/false, rest);
}

static ChordTemplate makeT53() {
    ChordTemplate t;
    t.function           = HarmonicFunction::T;
    t.degree             = 1;
    t.type               = ChordType::Triad;
    t.inversion          = Inversion::I;
    t.position           = ChordPosition::Close;
    t.degreesInSatbOrder = {1, 5, 3, 1};
    return t;
}

static Chord makeChord(NoteName s, int so, NoteName a, int ao,
                        NoteName t, int to, NoteName b, int bo) {
    auto tmpl = makeT53();
    return Chord(mkNote(s,so), mkNote(a,ao), mkNote(t,to), mkNote(b,bo), tmpl);
}

// Build a HarmonicPosition with all fields set.
static HarmonicPosition makePos(int srcIdx, int offset, int dur,
                                 int meas = 1, int start = 0, bool fixedRest = false) {
    HarmonicPosition p;
    p.index                       = srcIdx;
    p.measureIndex                = meas;
    p.startSixteenth              = start;
    p.durationSixteenths          = dur;
    p.sourceNoteIndex             = srcIdx;
    p.offsetInFixedNoteSixteenths = offset;
    Note fn = mkNote(NoteName::C, 5, dur, fixedRest);
    p.fixedNote                   = fn;
    return p;
}

static HarmonizationSettings makeSettings4_4(bool split = false) {
    HarmonizationSettings s;
    s.key = "C"; s.scaleModes = {"natural","harmonic","melodic"};
    s.allowedChords = {"T53","T6","S53","S6","D53","D6","D7"};
    s.timeSignature.beats = 4; s.timeSignature.beatType = 4;
    s.anacrusisSixteenths = 0; s.measureCount = 1;
    s.splitLongNotesByBasePulse = split;
    return s;
}

// ── HarmonicPositionBuilder: sourceNoteIndex & offsetInFixedNoteSixteenths ──

TEST_CASE("SegOut: single note positions carry sourceNoteIndex 0", "[segment-output]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings4_4(false);
    auto plans    = planner.buildPlans({ mkNote(NoteName::C, 5, 4) }, settings);
    REQUIRE(plans.size() == 1);

    HarmonicPositionBuilder builder;
    auto positions = builder.build(plans[0].segments, settings, HarmonizationMode::HarmonizeMelody);
    REQUIRE(positions.size() == 1);
    CHECK(positions[0].sourceNoteIndex == 0);
    CHECK(positions[0].offsetInFixedNoteSixteenths == 0);
}

TEST_CASE("SegOut: two notes have sourceNoteIndexes 0 and 1", "[segment-output]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings4_4(false);
    auto plans = planner.buildPlans(
        { mkNote(NoteName::C, 5, 4), mkNote(NoteName::E, 5, 4) }, settings);
    HarmonicPositionBuilder builder;
    auto positions = builder.build(plans[0].segments, settings, HarmonizationMode::HarmonizeMelody);
    REQUIRE(positions.size() == 2);
    CHECK(positions[0].sourceNoteIndex == 0);
    CHECK(positions[1].sourceNoteIndex == 1);
}

TEST_CASE("SegOut: split [4,4,8] segments share sourceNoteIndex 0", "[segment-output]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings4_4(true);
    auto plans    = planner.buildPlans({ mkNote(NoteName::C, 5, 16) }, settings);
    REQUIRE(plans.size() == 1);
    HarmonicPositionBuilder builder;
    auto positions = builder.build(plans[0].segments, settings, HarmonizationMode::HarmonizeMelody);
    REQUIRE(positions.size() == 3);  // cadence [4,4,8]
    CHECK(positions[0].sourceNoteIndex == 0);
    CHECK(positions[1].sourceNoteIndex == 0);
    CHECK(positions[2].sourceNoteIndex == 0);
}

TEST_CASE("SegOut: split [4,4,8] segment offsets are 0, 4, 8", "[segment-output]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings4_4(true);
    auto plans    = planner.buildPlans({ mkNote(NoteName::C, 5, 16) }, settings);
    REQUIRE(plans.size() == 1);
    HarmonicPositionBuilder builder;
    auto positions = builder.build(plans[0].segments, settings, HarmonizationMode::HarmonizeMelody);
    REQUIRE(positions.size() == 3);
    CHECK(positions[0].offsetInFixedNoteSixteenths == 0);
    CHECK(positions[1].offsetInFixedNoteSixteenths == 4);
    CHECK(positions[2].offsetInFixedNoteSixteenths == 8);
}

// ── Score.fixedVoiceIndex ─────────────────────────────────────────────────────

TEST_CASE("SegOut: Score default fixedVoiceIndex is -1", "[segment-output]") {
    Score s;
    CHECK(s.fixedVoiceIndex == -1);
}

TEST_CASE("SegOut: MelodyHarmonizer sets fixedVoiceIndex 0", "[segment-output]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 4), mkNote(NoteName::E, 5, 4),
                    mkNote(NoteName::G, 5, 4), mkNote(NoteName::C, 5, 4) };
    MelodyHarmonizer h;
    auto variants = h.harmonize(input, makeSettings4_4(false));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.musicScore.fixedVoiceIndex == 0);
}

TEST_CASE("SegOut: BassHarmonizer sets fixedVoiceIndex 3", "[segment-output]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 3, 4), mkNote(NoteName::G, 2, 4),
                    mkNote(NoteName::C, 3, 4), mkNote(NoteName::G, 2, 4) };
    BassHarmonizer h;
    auto variants = h.harmonize(input, makeSettings4_4(false));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.musicScore.fixedVoiceIndex == 3);
}

// ── ScoreSerializer: tiedToNext ───────────────────────────────────────────────

// Build a melody-mode Score with 3 positions that all belong to sourceNoteIndex=0.
static Score makeSplitMelodyScore() {
    Score score;
    score.fixedVoiceIndex = 0;
    // Three chords — same soprano C5 across segments [4,4,8].
    Chord c1 = makeChord(NoteName::C,5, NoteName::G,4, NoteName::E,4, NoteName::C,3);
    Chord c2 = makeChord(NoteName::C,5, NoteName::E,4, NoteName::G,3, NoteName::C,3);
    Chord c3 = makeChord(NoteName::C,5, NoteName::G,4, NoteName::E,4, NoteName::C,3);
    score.chords = {c1, c2, c3};

    score.positions.push_back(makePos(0,  0, 4, 1,  0));   // first  segment: offset 0
    score.positions.push_back(makePos(0,  4, 4, 1,  4));   // middle segment: offset 4
    score.positions.push_back(makePos(0,  8, 8, 1,  8));   // last   segment: offset 8
    return score;
}

TEST_CASE("SegOut: ScoreSerializer soprano merges to 1 whole note (3 segments, same sourceNoteIndex)",
          "[segment-output]") {
    // Cadence split [4,4,8] all belong to sourceNoteIndex=0 → mergeFixed collapses
    // them into a single 16-sixteenth whole note with tiedToNext=false.
    auto score    = makeSplitMelodyScore();
    ScoreSerializer ser;
    auto fs = ser.serialize(score, makeSettings4_4());
    REQUIRE_FALSE(fs.measures.empty());
    const auto& s = fs.measures[0].voices.soprano;
    REQUIRE(s.size() == 1);
    CHECK(s[0].durationSixteenths == 16);
    CHECK(s[0].durationType       == "whole");
    CHECK(s[0].tiedToNext         == false);
}

TEST_CASE("SegOut: ScoreSerializer soprano merged note has correct pitch", "[segment-output]") {
    auto score = makeSplitMelodyScore();
    ScoreSerializer ser;
    auto fs = ser.serialize(score, makeSettings4_4());
    const auto& s = fs.measures[0].voices.soprano;
    REQUIRE(s.size() == 1);
    CHECK(s[0].step   == "C");
    CHECK(s[0].octave == 5);
}

TEST_CASE("SegOut: ScoreSerializer chordNames kept (3 entries) after soprano merge to 1 note",
          "[segment-output]") {
    auto score = makeSplitMelodyScore();
    ScoreSerializer ser;
    auto fs = ser.serialize(score, makeSettings4_4());
    const auto& m = fs.measures[0];
    REQUIRE(m.voices.soprano.size() == 1);
    CHECK(m.chordNames.size()       == 3);
    CHECK(m.chordTicks.size()       == 3);
}

TEST_CASE("SegOut: ScoreSerializer inner voices never tiedToNext in melody mode", "[segment-output]") {
    auto score = makeSplitMelodyScore();
    ScoreSerializer ser;
    auto fs = ser.serialize(score, makeSettings4_4());
    const auto& m = fs.measures[0];
    for (const auto& n : m.voices.alto)   CHECK(n.tiedToNext == false);
    for (const auto& n : m.voices.tenor)  CHECK(n.tiedToNext == false);
    for (const auto& n : m.voices.bass)   CHECK(n.tiedToNext == false);
}

TEST_CASE("SegOut: ScoreSerializer no ties when fixedVoiceIndex=-1", "[segment-output]") {
    auto score = makeSplitMelodyScore();
    score.fixedVoiceIndex = -1;
    ScoreSerializer ser;
    auto fs = ser.serialize(score, makeSettings4_4());
    const auto& m = fs.measures[0];
    for (const auto& n : m.voices.soprano) CHECK(n.tiedToNext == false);
    for (const auto& n : m.voices.alto)    CHECK(n.tiedToNext == false);
}

TEST_CASE("SegOut: ScoreSerializer bass merges to 1 whole note in bass mode", "[segment-output]") {
    // Bass is the fixed voice (fvi=3); 3 positions share sourceNoteIndex=0 →
    // mergeFixed collapses them to a single 16-sixteenth whole note.
    Score score;
    score.fixedVoiceIndex = 3;
    Chord c = makeChord(NoteName::G,4, NoteName::E,4, NoteName::B,3, NoteName::G,2);
    score.chords = {c, c, c};
    score.positions.push_back(makePos(0, 0, 4, 1, 0));
    score.positions.push_back(makePos(0, 4, 4, 1, 4));
    score.positions.push_back(makePos(0, 8, 8, 1, 8));
    for (auto& p : score.positions) p.fixedNote = mkNote(NoteName::G, 2, 4);

    ScoreSerializer ser;
    auto fs = ser.serialize(score, makeSettings4_4());
    REQUIRE_FALSE(fs.measures.empty());
    const auto& b = fs.measures[0].voices.bass;
    REQUIRE(b.size() == 1);
    CHECK(b[0].durationSixteenths == 16);
    CHECK(b[0].durationType       == "whole");
    CHECK(b[0].tiedToNext         == false);
    for (const auto& n : fs.measures[0].voices.soprano) CHECK(n.tiedToNext == false);
}

TEST_CASE("SegOut: ScoreSerializer rest fixed positions merge to single rest note", "[segment-output]") {
    // Two rest positions with same sourceNoteIndex=0 → mergeFixed merges them.
    // Result: 1 half-note rest (dur=8), tiedToNext=false.
    Score score;
    score.fixedVoiceIndex = 0;
    Chord c = makeChord(NoteName::C,5, NoteName::G,4, NoteName::E,4, NoteName::C,3);
    score.chords = {c, c};
    score.positions.push_back(makePos(0, 0, 4, 1, 0, /*fixedRest=*/true));
    score.positions.push_back(makePos(0, 4, 4, 1, 4, /*fixedRest=*/true));

    ScoreSerializer ser;
    auto fs = ser.serialize(score, makeSettings4_4());
    const auto& s = fs.measures[0].voices.soprano;
    REQUIRE(s.size() == 1);
    CHECK(s[0].durationSixteenths == 8);
    CHECK(s[0].tiedToNext         == false);
    CHECK(s[0].isRest             == true);
}

// ── MusicXmlWriter: tie XML elements ─────────────────────────────────────────

TEST_CASE("SegOut: MusicXmlWriter no tie elements without segmentation", "[segment-output]") {
    Score score;
    score.fixedVoiceIndex = 0;
    score.chords.push_back(makeChord(NoteName::C,5, NoteName::G,4, NoteName::E,4, NoteName::C,3));
    score.chords.push_back(makeChord(NoteName::E,5, NoteName::C,5, NoteName::G,4, NoteName::G,3));
    score.positions.push_back(makePos(0, 0, 4, 1, 0));  // different sourceNoteIndex
    score.positions.push_back(makePos(1, 0, 4, 1, 4));

    MusicXmlWriter writer;
    HarmonizationSettings settings = makeSettings4_4();
    auto xml = writer.writeScoreToString(score, settings, "job1", "v1");

    CHECK(xml.find("<tie") == std::string::npos);
    CHECK(xml.find("<tied") == std::string::npos);
}

TEST_CASE("SegOut: MusicXmlWriter soprano has tie start on first segment", "[segment-output]") {
    auto score = makeSplitMelodyScore();
    MusicXmlWriter writer;
    auto xml = writer.writeScoreToString(score, makeSettings4_4(), "job1", "v1");
    CHECK(xml.find("<tie type=\"start\"/>") != std::string::npos);
}

TEST_CASE("SegOut: MusicXmlWriter soprano has tie stop on subsequent segments", "[segment-output]") {
    auto score = makeSplitMelodyScore();
    MusicXmlWriter writer;
    auto xml = writer.writeScoreToString(score, makeSettings4_4(), "job1", "v1");
    CHECK(xml.find("<tie type=\"stop\"/>") != std::string::npos);
}

TEST_CASE("SegOut: MusicXmlWriter notations contain tied elements", "[segment-output]") {
    auto score = makeSplitMelodyScore();
    MusicXmlWriter writer;
    auto xml = writer.writeScoreToString(score, makeSettings4_4(), "job1", "v1");
    CHECK(xml.find("<notations>") != std::string::npos);
    CHECK(xml.find("<tied type=\"start\"/>") != std::string::npos);
    CHECK(xml.find("<tied type=\"stop\"/>")  != std::string::npos);
}

TEST_CASE("SegOut: MusicXmlWriter inner voices have no tie elements (melody mode)", "[segment-output]") {
    // Build a score where only soprano (voice 1 of P1) should be tied.
    // Check that P2 (tenor/bass) has no tie elements.
    auto score = makeSplitMelodyScore();
    MusicXmlWriter writer;
    auto xml = writer.writeScoreToString(score, makeSettings4_4(), "job1", "v1");

    // P2 starts after </part> of P1. Find P2 content.
    const auto p2Start = xml.find("<part id=\"P2\">");
    REQUIRE(p2Start != std::string::npos);
    const auto p2Content = xml.substr(p2Start);
    CHECK(p2Content.find("<tie") == std::string::npos);
}

// ── HarmonizationResult / toJson metadata ────────────────────────────────────

static HarmonizationResult makeResult(int planIdx, int planPri,
                                       std::vector<int> durs) {
    HarmonizationResult r;
    r.variantId          = "variant_001";
    r.score              = 50;
    r.rhythmPlanIndex    = planIdx;
    r.rhythmPlanPriority = planPri;
    r.segmentDurations   = std::move(durs);
    return r;
}

TEST_CASE("SegOut: HarmonizationResult carries rhythmPlanIndex", "[segment-output]") {
    auto r = makeResult(1, 10, {4,4,8});
    CHECK(r.rhythmPlanIndex == 1);
}

TEST_CASE("SegOut: HarmonizationResult carries rhythmPlanPriority", "[segment-output]") {
    auto r = makeResult(1, 10, {4,4,8});
    CHECK(r.rhythmPlanPriority == 10);
}

TEST_CASE("SegOut: HarmonizationResult carries segmentDurations", "[segment-output]") {
    auto r = makeResult(0, 0, {4,4,8});
    REQUIRE(r.segmentDurations.size() == 3);
    CHECK(r.segmentDurations[0] == 4);
    CHECK(r.segmentDurations[1] == 4);
    CHECK(r.segmentDurations[2] == 8);
}

TEST_CASE("SegOut: toJson includes rhythmPlanIndex", "[segment-output]") {
    JobResult jr;
    jr.jobId  = "j1";
    jr.status = "success";
    jr.results.push_back(makeResult(2, 20, {4,4,4,4}));
    auto j = json::parse(jr.toJson());
    CHECK(j["results"][0]["rhythmPlanIndex"] == 2);
}

TEST_CASE("SegOut: toJson includes rhythmPlanPriority", "[segment-output]") {
    JobResult jr;
    jr.jobId = "j1"; jr.status = "success";
    jr.results.push_back(makeResult(1, 10, {4,4,8}));
    auto j = json::parse(jr.toJson());
    CHECK(j["results"][0]["rhythmPlanPriority"] == 10);
}

TEST_CASE("SegOut: toJson includes segmentDurations array", "[segment-output]") {
    JobResult jr;
    jr.jobId = "j1"; jr.status = "success";
    jr.results.push_back(makeResult(0, 0, {4,4,8}));
    auto j = json::parse(jr.toJson());
    auto durs = j["results"][0]["segmentDurations"];
    REQUIRE(durs.size() == 3);
    CHECK(durs[0] == 4);
    CHECK(durs[1] == 4);
    CHECK(durs[2] == 8);
}

TEST_CASE("SegOut: toJson FrontendNote includes tiedToNext field", "[segment-output]") {
    // Build a minimal result with a note that has tiedToNext=true.
    FrontendNote n;
    n.step = "C"; n.octave = 5; n.alter = 0;
    n.durationSixteenths = 4; n.durationType = "quarter";
    n.tiedToNext = true; n.isRest = false;

    FrontendMeasure m; m.number = 1;
    m.voices.soprano.push_back(n);

    FrontendScore fs;
    fs.key = "C"; fs.mode = "major"; fs.beats = 4; fs.beatType = 4;
    fs.measures.push_back(m);

    HarmonizationResult r;
    r.variantId = "variant_001"; r.score = 50;
    r.frontendScore = fs;

    JobResult jr;
    jr.jobId = "j1"; jr.status = "success";
    jr.results.push_back(r);

    auto j = json::parse(jr.toJson());
    auto sop = j["results"][0]["measures"][0]["voices"]["soprano"];
    REQUIRE_FALSE(sop.empty());
    CHECK(sop[0]["tiedToNext"] == true);
    CHECK(sop[0]["isRest"]     == false);
}

// ── Duration integrity ────────────────────────────────────────────────────────

TEST_CASE("SegOut: segment durations sum equals source note duration", "[segment-output]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings4_4(true);
    auto plans    = planner.buildPlans({ mkNote(NoteName::C, 5, 16) }, settings);

    for (const auto& plan : plans) {
        int sum = 0;
        for (const auto& seg : plan.segments) sum += seg.durationSixteenths;
        CHECK(sum == 16);
    }
}

TEST_CASE("SegOut: position durations in score match segment durations", "[segment-output]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 16) };
    MelodyHarmonizer h;
    auto variants = h.harmonize(input, makeSettings4_4(true));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants) {
        int sum = 0;
        for (const auto& pos : v.musicScore.positions) sum += pos.durationSixteenths;
        CHECK(sum == 16);
    }
}
