#include <catch2/catch_test_macros.hpp>
#include "harmonization/HarmonicRhythmPlanner.h"

static Note makeNote(NoteName name, int dur, bool tiedToNext = false) {
    return Note(name, 4, 0, 1, dur, false, tiedToNext, false);
}

static Note makeRest(int dur) {
    return Note(NoteName::C, 4, 0, 0, dur, false, false, true);
}

static HarmonizationSettings makeSettings44(bool split = true) {
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

static HarmonizationSettings makeSettings68(bool split = true) {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural"};
    s.allowedChords = {"T53"};
    s.timeSignature.beats    = 6;
    s.timeSignature.beatType = 8;
    s.anacrusisSixteenths    = 0;
    s.measureCount           = 2;
    s.splitLongNotesByBasePulse = split;
    return s;
}

// ── getAllowedSplitPatterns: cadence split is unconditional for 4/4 whole note ─

TEST_CASE("getAllowedSplitPatterns: flag=false still produces cadence split for 4/4 whole note at boundary", "[cadence-pattern]") {
    // Cadence split is unconditional — it fires regardless of splitLongNotesByBasePulse.
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(16, 0, makeSettings44(false));
    REQUIRE(patterns.size() == 2);
    CHECK(patterns[0].durationsSixteenths == std::vector<int>({4, 4, 8}));
    CHECK(patterns[1].durationsSixteenths == std::vector<int>({4, 4, 4, 4}));
}

// ── getAllowedSplitPatterns: cadence trigger ──────────────────────────────────

TEST_CASE("getAllowedSplitPatterns: 4/4 whole note at pos 0 → first pattern is [4,4,8]", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(16, 0, makeSettings44());
    REQUIRE(patterns.size() >= 1);
    REQUIRE(patterns[0].durationsSixteenths == std::vector<int>({4, 4, 8}));
}

TEST_CASE("getAllowedSplitPatterns: 4/4 whole note at pos 0 → also has [4,4,4,4] fallback", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(16, 0, makeSettings44());
    REQUIRE(patterns.size() == 2);
    CHECK(patterns[1].durationsSixteenths == std::vector<int>({4, 4, 4, 4}));
}

TEST_CASE("getAllowedSplitPatterns: 4/4 whole note at pos 16 (measure 2) → cadence pattern", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(16, 16, makeSettings44());
    REQUIRE(patterns.size() == 2);
    CHECK(patterns[0].durationsSixteenths == std::vector<int>({4, 4, 8}));
}

TEST_CASE("getAllowedSplitPatterns: 4/4 whole note at pos 32 (measure 3) → cadence pattern", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(16, 32, makeSettings44());
    REQUIRE(patterns.size() == 2);
    CHECK(patterns[0].durationsSixteenths == std::vector<int>({4, 4, 8}));
}

// ── Cadence pattern NOT triggered ────────────────────────────────────────────

TEST_CASE("getAllowedSplitPatterns: 4/4 whole note NOT at measure boundary → equal split", "[cadence-pattern]") {
    // Preceded by a quarter note, so absoluteStart=4 (not divisible by 16)
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(16, 4, makeSettings44());
    REQUIRE(patterns.size() == 1);
    CHECK(patterns[0].durationsSixteenths == std::vector<int>({4, 4, 4, 4}));
}

TEST_CASE("getAllowedSplitPatterns: 4/4 half note at measure boundary → equal split, not cadence", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(8, 0, makeSettings44());
    REQUIRE(patterns.size() == 1);
    CHECK(patterns[0].durationsSixteenths == std::vector<int>({4, 4}));
}

TEST_CASE("getAllowedSplitPatterns: 3/4 whole note at boundary → equal split (not 4/4)", "[cadence-pattern]") {
    HarmonizationSettings s = makeSettings44();
    s.timeSignature.beats = 3;  // 3/4
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(16, 0, s);
    REQUIRE(patterns.size() == 1);
    CHECK(patterns[0].durationsSixteenths == std::vector<int>({4, 4, 4, 4}));
}

// ── Short notes: not split ────────────────────────────────────────────────────

TEST_CASE("getAllowedSplitPatterns: quarter (=basePulse) → not split", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(4, 0, makeSettings44());
    REQUIRE(patterns.size() == 1);
    REQUIRE(patterns[0].durationsSixteenths.size() == 1);
    CHECK(patterns[0].durationsSixteenths[0] == 4);
}

TEST_CASE("getAllowedSplitPatterns: eighth (< basePulse) → not split", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(2, 0, makeSettings44());
    REQUIRE(patterns.size() == 1);
    CHECK(patterns[0].durationsSixteenths[0] == 2);
}

TEST_CASE("getAllowedSplitPatterns: dotted quarter (6, not divisible by 4) → not split", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(6, 0, makeSettings44());
    REQUIRE(patterns.size() == 1);
    CHECK(patterns[0].durationsSixteenths == std::vector<int>({6}));
}

// ── 6/8 meter ─────────────────────────────────────────────────────────────────

TEST_CASE("getAllowedSplitPatterns: 6/8 dotted quarter (6) at pos 0 → [2,2,2]", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(6, 0, makeSettings68());
    REQUIRE(patterns.size() == 1);
    CHECK(patterns[0].durationsSixteenths == std::vector<int>({2, 2, 2}));
}

TEST_CASE("getAllowedSplitPatterns: 6/8 dotted half (12) at pos 0 → [2,2,2,2,2,2]", "[cadence-pattern]") {
    auto patterns = HarmonicRhythmPlanner::getAllowedSplitPatterns(12, 0, makeSettings68());
    REQUIRE(patterns.size() == 1);
    REQUIRE(patterns[0].durationsSixteenths.size() == 6);
    for (int d : patterns[0].durationsSixteenths)
        CHECK(d == 2);
}

// ── buildSegments: cadence pattern applied ────────────────────────────────────

TEST_CASE("buildSegments: 4/4 whole note at pos 0 → 3 segments [4,4,8]", "[cadence-pattern]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({makeNote(NoteName::C, 16)}, makeSettings44());
    REQUIRE(segs.size() == 3);
    CHECK(segs[0].durationSixteenths          == 4);
    CHECK(segs[0].offsetInFixedNoteSixteenths == 0);
    CHECK(segs[1].durationSixteenths          == 4);
    CHECK(segs[1].offsetInFixedNoteSixteenths == 4);
    CHECK(segs[2].durationSixteenths          == 8);
    CHECK(segs[2].offsetInFixedNoteSixteenths == 8);
}

TEST_CASE("buildSegments: cadence segments share fixedNote and sourceNoteIndex", "[cadence-pattern]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({makeNote(NoteName::G, 16)}, makeSettings44());
    REQUIRE(segs.size() == 3);
    for (const auto& seg : segs) {
        CHECK(seg.fixedNote.getName() == NoteName::G);
        CHECK(seg.sourceNoteIndex     == 0);
    }
}

TEST_CASE("buildSegments: second whole note also gets cadence pattern (starts at pos 16)", "[cadence-pattern]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),  // absoluteStart=0  → cadence [4,4,8]
        makeNote(NoteName::G, 16),  // absoluteStart=16 → cadence [4,4,8]
    };
    auto segs = planner.buildSegments(notes, makeSettings44());
    REQUIRE(segs.size() == 6);
    CHECK(segs[3].durationSixteenths == 4);
    CHECK(segs[4].durationSixteenths == 4);
    CHECK(segs[5].durationSixteenths == 8);
    CHECK(segs[3].sourceNoteIndex    == 1);
    CHECK(segs[4].sourceNoteIndex    == 1);
    CHECK(segs[5].sourceNoteIndex    == 1);
}

TEST_CASE("buildSegments: whole note NOT at measure boundary gets equal split [4,4,4,4]", "[cadence-pattern]") {
    HarmonicRhythmPlanner planner;
    // quarter first → shifts whole note to absoluteStart=4
    std::vector<Note> notes = {
        makeNote(NoteName::E, 4),
        makeNote(NoteName::C, 16),
    };
    auto segs = planner.buildSegments(notes, makeSettings44());
    // first segment: quarter, not split
    // next four segments: equal split of whole note
    REQUIRE(segs.size() == 5);
    CHECK(segs[0].durationSixteenths == 4);
    CHECK(segs[1].durationSixteenths == 4);
    CHECK(segs[2].durationSixteenths == 4);
    CHECK(segs[3].durationSixteenths == 4);
    CHECK(segs[4].durationSixteenths == 4);
}

TEST_CASE("buildSegments: cadence pattern works for rests too", "[cadence-pattern]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({makeRest(16)}, makeSettings44());
    REQUIRE(segs.size() == 3);
    CHECK(segs[0].isRest              == true);
    CHECK(segs[0].durationSixteenths  == 4);
    CHECK(segs[2].durationSixteenths  == 8);
}

// ── Offset accumulation across cadence-split segments ────────────────────────

TEST_CASE("buildSegments: offsets within cadence pattern are [0,4,8]", "[cadence-pattern]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({makeNote(NoteName::C, 16)}, makeSettings44());
    REQUIRE(segs.size() == 3);
    CHECK(segs[0].offsetInFixedNoteSixteenths == 0);
    CHECK(segs[1].offsetInFixedNoteSixteenths == 4);
    CHECK(segs[2].offsetInFixedNoteSixteenths == 8);
}

// ── absoluteStart tracking across mixed sequence ─────────────────────────────

TEST_CASE("buildSegments: half note followed by whole at boundary gets cadence", "[cadence-pattern]") {
    HarmonicRhythmPlanner planner;
    // half (8) starts at 0 → split [4,4]
    // whole (16) starts at 8 → 8 % 16 != 0, so equal split [4,4,4,4]
    std::vector<Note> notes = {
        makeNote(NoteName::D, 8),
        makeNote(NoteName::C, 16),
    };
    auto segs = planner.buildSegments(notes, makeSettings44());
    REQUIRE(segs.size() == 6);
    // half → [4,4]
    CHECK(segs[0].durationSixteenths == 4);
    CHECK(segs[1].durationSixteenths == 4);
    // whole at absoluteStart=8 → NOT at measure boundary → [4,4,4,4]
    CHECK(segs[2].durationSixteenths == 4);
    CHECK(segs[3].durationSixteenths == 4);
    CHECK(segs[4].durationSixteenths == 4);
    CHECK(segs[5].durationSixteenths == 4);
}

TEST_CASE("buildSegments: half+half=whole via tie at pos 0 gets cadence pattern", "[cadence-pattern]") {
    HarmonicRhythmPlanner planner;
    // Two tied halves merge to span of 16 at absoluteStart=0 → cadence [4,4,8]
    std::vector<Note> notes = {
        makeNote(NoteName::C, 8, /*tiedToNext=*/true),
        makeNote(NoteName::C, 8),
    };
    auto segs = planner.buildSegments(notes, makeSettings44());
    REQUIRE(segs.size() == 3);
    CHECK(segs[0].durationSixteenths          == 4);
    CHECK(segs[1].durationSixteenths          == 4);
    CHECK(segs[2].durationSixteenths          == 8);
    for (const auto& seg : segs)
        CHECK(seg.sourceNoteIndex == 0);
}
