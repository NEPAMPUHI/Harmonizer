#include <catch2/catch_test_macros.hpp>
#include "harmonization/HarmonicRhythmPlanner.h"
#include "harmonization/HarmonicRhythmPlan.h"

static Note makeNote(NoteName name, int dur, bool tiedToNext = false) {
    return Note(name, 4, 0, 1, dur, false, tiedToNext, false);
}

static Note makeRest(int dur) {
    return Note(NoteName::C, 4, 0, 0, dur, false, false, true);
}

static HarmonizationSettings makeSettings44(bool split = false) {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural"};
    s.allowedChords = {"T53"};
    s.timeSignature.beats    = 4;
    s.timeSignature.beatType = 4;
    s.anacrusisSixteenths    = 0;
    s.measureCount           = 2;
    s.splitLongNotesByBasePulse = split;
    return s;
}

// ── buildPlans: API contract ──────────────────────────────────────────────────

TEST_CASE("buildPlans: empty notes → 1 plan with 0 segments", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({}, makeSettings44());
    REQUIRE(plans.size() == 1);
    CHECK(plans[0].segments.empty());
}

TEST_CASE("buildPlans: always returns at least 1 plan", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeNote(NoteName::C, 4) }, makeSettings44());
    CHECK(plans.size() >= 1);
}

TEST_CASE("buildPlans: always returns exactly 1 plan", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeNote(NoteName::C, 4) }, makeSettings44());
    CHECK(plans.size() == 1);
}

// ── Consistency with buildSegments ───────────────────────────────────────────

TEST_CASE("buildPlans: plan[0].segments matches buildSegments for simple notes", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4),
        makeNote(NoteName::D, 8),
        makeNote(NoteName::E, 2),
    };
    auto settings = makeSettings44();
    auto segs  = planner.buildSegments(notes, settings);
    auto plans = planner.buildPlans(notes, settings);

    REQUIRE(plans.size() == 1);
    REQUIRE(plans[0].segments.size() == segs.size());
    for (size_t i = 0; i < segs.size(); ++i) {
        CHECK(plans[0].segments[i].durationSixteenths          == segs[i].durationSixteenths);
        CHECK(plans[0].segments[i].offsetInFixedNoteSixteenths == segs[i].offsetInFixedNoteSixteenths);
        CHECK(plans[0].segments[i].sourceNoteIndex             == segs[i].sourceNoteIndex);
        CHECK(plans[0].segments[i].fixedNote.getName()         == segs[i].fixedNote.getName());
    }
}

TEST_CASE("buildPlans: plan[0].segments matches buildSegments with split enabled", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = { makeNote(NoteName::G, 8) };
    auto settings = makeSettings44(/*split=*/true);
    auto segs  = planner.buildSegments(notes, settings);
    auto plans = planner.buildPlans(notes, settings);

    REQUIRE(plans.size() == 1);
    REQUIRE(plans[0].segments.size() == segs.size());
    for (size_t i = 0; i < segs.size(); ++i)
        CHECK(plans[0].segments[i].durationSixteenths == segs[i].durationSixteenths);
}

// ── Segment fields accessible through HarmonicRhythmPlan ─────────────────────

TEST_CASE("buildPlans: segment fields accessible — fixedNote, duration, offset, index", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeNote(NoteName::E, 8) }, makeSettings44());

    REQUIRE(plans[0].segments.size() == 1);
    const auto& seg = plans[0].segments[0];
    CHECK(seg.fixedNote.getName()         == NoteName::E);
    CHECK(seg.durationSixteenths          == 8);
    CHECK(seg.offsetInFixedNoteSixteenths == 0);
    CHECK(seg.sourceNoteIndex             == 0);
    CHECK(seg.isRest                      == false);
}

TEST_CASE("buildPlans: rest segment isRest=true visible through plan", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeRest(4) }, makeSettings44());

    REQUIRE(plans[0].segments.size() == 1);
    CHECK(plans[0].segments[0].isRest == true);
}

// ── Cadence split pattern: whole note in 4/4 always uses [4,4,8] ─────────────

TEST_CASE("buildPlans: 4/4 whole note at boundary → exactly 1 plan with cadence [4,4,8]", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeNote(NoteName::C, 16) }, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 1);
    REQUIRE(plans[0].segments.size() == 3);
    CHECK(plans[0].segments[0].durationSixteenths == 4);
    CHECK(plans[0].segments[1].durationSixteenths == 4);
    CHECK(plans[0].segments[2].durationSixteenths == 8);
}

TEST_CASE("buildPlans: 4/4 whole note — cadence pattern also without split flag", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeNote(NoteName::C, 16) }, makeSettings44(/*split=*/false));
    REQUIRE(plans.size() == 1);
    REQUIRE(plans[0].segments.size() == 3);
    CHECK(plans[0].segments[0].durationSixteenths == 4);
    CHECK(plans[0].segments[2].durationSixteenths == 8);
}

// ── Tied notes visible through buildPlans ────────────────────────────────────

TEST_CASE("buildPlans: tied notes merged in the single plan", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4, /*tiedToNext=*/true),
        makeNote(NoteName::C, 4),
    };
    auto plans = planner.buildPlans(notes, makeSettings44());

    REQUIRE(plans.size() == 1);
    REQUIRE(plans[0].segments.size() == 1);
    CHECK(plans[0].segments[0].durationSixteenths == 8);
    CHECK(plans[0].segments[0].sourceNoteIndex    == 0);
}

// ── Always exactly 1 plan regardless of note count ───────────────────────────

TEST_CASE("buildPlans: 2 whole notes in 4/4 → always 1 plan", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),
        makeNote(NoteName::G, 16),
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 1);
    // Both whole notes get cadence pattern [4,4,8] = 3 segs each → 6 total
    CHECK(plans[0].segments.size() == 6);
    CHECK(plans[0].segments[2].durationSixteenths == 8);  // cadence half of note 0
    CHECK(plans[0].segments[5].durationSixteenths == 8);  // cadence half of note 1
}

TEST_CASE("buildPlans: sourceNoteIndex correct for 2 whole notes", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),  // note 0
        makeNote(NoteName::G, 16),  // note 1
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 1);
    // First 3 segments belong to note 0, last 3 to note 1
    for (int i = 0; i < 3; ++i)
        CHECK(plans[0].segments[i].sourceNoteIndex == 0);
    for (int i = 3; i < 6; ++i)
        CHECK(plans[0].segments[i].sourceNoteIndex == 1);
}

TEST_CASE("buildPlans: non-branching notes produce 1 plan", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4),
        makeNote(NoteName::D, 4),
        makeNote(NoteName::E, 4),
        makeNote(NoteName::F, 4),
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    CHECK(plans.size() == 1);
}

TEST_CASE("buildPlans: mixed notes (whole at boundary + quarter) → 1 plan", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    // whole at pos 0 → cadence [4,4,8] + quarter → 1 seg → total 4 segs
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),
        makeNote(NoteName::G, 4),
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 1);
    CHECK(plans[0].segments.size() == 4);
    CHECK(plans[0].segments.back().fixedNote.getName() == NoteName::G);
    CHECK(plans[0].segments.back().durationSixteenths  == 4);
}

// ── buildSegments remains consistent with buildPlans.front() ─────────────────

TEST_CASE("buildSegments: wrapper returns plan[0].segments (cadence pattern preferred)", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings44(/*split=*/true);
    std::vector<Note> notes = { makeNote(NoteName::C, 16) };

    auto segs  = planner.buildSegments(notes, settings);
    auto plans = planner.buildPlans(notes, settings);

    REQUIRE(!segs.empty());
    REQUIRE(segs.size() == plans[0].segments.size());
    for (size_t i = 0; i < segs.size(); ++i)
        CHECK(segs[i].durationSixteenths == plans[0].segments[i].durationSixteenths);
}
