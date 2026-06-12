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

TEST_CASE("buildPlans: currently returns exactly 1 plan", "[rhythm-plan]") {
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

// ── Cadence split pattern: 2 plans from 1 whole note ─────────────────────────

TEST_CASE("buildPlans: 4/4 whole note at boundary → 2 plans", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeNote(NoteName::C, 16) }, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 2);
}

TEST_CASE("buildPlans: 4/4 whole note — plan[0] uses cadence pattern [4,4,8]", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeNote(NoteName::C, 16) }, makeSettings44(/*split=*/true));
    REQUIRE(plans[0].segments.size() == 3);
    CHECK(plans[0].segments[0].durationSixteenths == 4);
    CHECK(plans[0].segments[1].durationSixteenths == 4);
    CHECK(plans[0].segments[2].durationSixteenths == 8);
}

TEST_CASE("buildPlans: 4/4 whole note — plan[1] uses equal split [4,4,4,4]", "[rhythm-plan]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ makeNote(NoteName::C, 16) }, makeSettings44(/*split=*/true));
    REQUIRE(plans[1].segments.size() == 4);
    for (const auto& seg : plans[1].segments)
        CHECK(seg.durationSixteenths == 4);
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

// ── Combinatorial branching ───────────────────────────────────────────────────

TEST_CASE("buildPlans: 2 whole notes in 4/4 → 4 plans (2×2 cartesian product)", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),
        makeNote(NoteName::G, 16),
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 4);
}

TEST_CASE("buildPlans: 2 whole notes — plan ordering is depth-first by priority", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),
        makeNote(NoteName::G, 16),
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 4);

    // plan[0]: [4,4,8] + [4,4,8]   → 6 segments
    CHECK(plans[0].segments.size() == 6);
    CHECK(plans[0].segments[2].durationSixteenths == 8);  // last seg of note 0
    CHECK(plans[0].segments[5].durationSixteenths == 8);  // last seg of note 1

    // plan[1]: [4,4,8] + [4,4,4,4] → 7 segments
    CHECK(plans[1].segments.size() == 7);
    CHECK(plans[1].segments[2].durationSixteenths == 8);  // cadence seg of note 0
    CHECK(plans[1].segments[6].durationSixteenths == 4);  // equal-split seg of note 1

    // plan[2]: [4,4,4,4] + [4,4,8] → 7 segments
    CHECK(plans[2].segments.size() == 7);
    CHECK(plans[2].segments[3].durationSixteenths == 4);  // equal-split seg of note 0
    CHECK(plans[2].segments[6].durationSixteenths == 8);  // cadence seg of note 1

    // plan[3]: [4,4,4,4] + [4,4,4,4] → 8 segments
    CHECK(plans[3].segments.size() == 8);
}

TEST_CASE("buildPlans: sourceNoteIndex correct across all plans", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),  // note 0
        makeNote(NoteName::G, 16),  // note 1
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 4);
    for (const auto& plan : plans) {
        for (const auto& seg : plan.segments)
            CHECK(seg.sourceNoteIndex < 2);
        // First note's segments always come before second note's segments.
        bool seenNote1 = false;
        for (const auto& seg : plan.segments) {
            if (seg.sourceNoteIndex == 1) seenNote1 = true;
            if (seenNote1)
                CHECK(seg.sourceNoteIndex == 1);
        }
    }
}

TEST_CASE("buildPlans: non-branching notes don't multiply plans", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    // Quarter notes never split → 1 pattern each → always 1 plan.
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4),
        makeNote(NoteName::D, 4),
        makeNote(NoteName::E, 4),
        makeNote(NoteName::F, 4),
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    CHECK(plans.size() == 1);
}

TEST_CASE("buildPlans: mixed branching and non-branching — only whole note branches", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    // quarter (no split) + whole (2 patterns) + quarter (no split) → 2 plans
    std::vector<Note> notes = {
        makeNote(NoteName::E, 4),
        makeNote(NoteName::C, 16),
        makeNote(NoteName::G, 4),
    };
    // Whole note starts at absoluteStart=4 → NOT at measure boundary → 1 pattern.
    // So all 3 notes produce 1 pattern each → 1 plan total.
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    CHECK(plans.size() == 1);
}

TEST_CASE("buildPlans: whole note at boundary in mixed sequence → 2 plans", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    // whole (2 patterns at pos 0) + quarter (1 pattern) → 2 plans
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),
        makeNote(NoteName::G, 4),
    };
    auto plans = planner.buildPlans(notes, makeSettings44(/*split=*/true));
    REQUIRE(plans.size() == 2);
    // Both plans have the quarter note as their last segment.
    CHECK(plans[0].segments.back().fixedNote.getName() == NoteName::G);
    CHECK(plans[1].segments.back().fixedNote.getName() == NoteName::G);
    CHECK(plans[0].segments.back().durationSixteenths  == 4);
    CHECK(plans[1].segments.back().durationSixteenths  == 4);
}

// ── maxHarmonicRhythmPlans cap ────────────────────────────────────────────────

TEST_CASE("buildPlans: maxHarmonicRhythmPlans=1 caps output to 1 plan", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings44(/*split=*/true);
    settings.maxHarmonicRhythmPlans = 1;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),
        makeNote(NoteName::G, 16),
    };
    auto plans = planner.buildPlans(notes, settings);
    CHECK(plans.size() == 1);
    // The one kept plan uses the highest-priority (cadence) pattern everywhere.
    CHECK(plans[0].segments.size() == 6);  // [4,4,8] + [4,4,8]
}

TEST_CASE("buildPlans: maxHarmonicRhythmPlans=2 caps 4-way product to 2 plans", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings44(/*split=*/true);
    settings.maxHarmonicRhythmPlans = 2;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),
        makeNote(NoteName::G, 16),
    };
    auto plans = planner.buildPlans(notes, settings);
    CHECK(plans.size() == 2);
}

TEST_CASE("buildPlans: maxHarmonicRhythmPlans=0 treated as minimum 1", "[rhythm-plan][branching]") {
    HarmonicRhythmPlanner planner;
    auto settings = makeSettings44(/*split=*/true);
    settings.maxHarmonicRhythmPlans = 0;
    auto plans = planner.buildPlans({ makeNote(NoteName::C, 16) }, settings);
    CHECK(plans.size() >= 1);
}

// ── buildSegments remains consistent with buildPlans.front() ─────────────────

TEST_CASE("buildSegments: wrapper returns plan[0].segments (cadence pattern preferred)", "[rhythm-plan][branching]") {
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
