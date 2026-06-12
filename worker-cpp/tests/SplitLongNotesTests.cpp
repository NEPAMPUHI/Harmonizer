#include <catch2/catch_test_macros.hpp>
#include "harmonization/HarmonicRhythmPlanner.h"

static Note makeNote(NoteName name, int dur, bool tiedToNext = false) {
    return Note(name, 4, 0, 1, dur, false, tiedToNext, false);
}

static Note makeRest(int dur) {
    return Note(NoteName::C, 4, 0, 0, dur, false, false, true);
}

static HarmonizationSettings makeSettings(int beats, int beatType, bool split = false) {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural"};
    s.allowedChords = {"T53"};
    s.timeSignature.beats    = beats;
    s.timeSignature.beatType = beatType;
    s.anacrusisSixteenths    = 0;
    s.measureCount           = 2;
    s.splitLongNotesByBasePulse = split;
    return s;
}

// ── Default behaviour (flag off) ─────────────────────────────────────────────

TEST_CASE("SplitLong: flag=false — 4/4 whole note at boundary still gets cadence split [4,4,8]", "[split]") {
    // Cadence split fires unconditionally; splitLongNotesByBasePulse does not suppress it.
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({ makeNote(NoteName::C, 16) }, makeSettings(4, 4, false));
    REQUIRE(segs.size() == 3);
    CHECK(segs[0].durationSixteenths          == 4);
    CHECK(segs[0].offsetInFixedNoteSixteenths == 0);
    CHECK(segs[1].durationSixteenths          == 4);
    CHECK(segs[2].durationSixteenths          == 8);
}

TEST_CASE("SplitLong: flag=false — half note stays as 1 segment", "[split]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({ makeNote(NoteName::E, 8) }, makeSettings(4, 4, false));
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].durationSixteenths == 8);
}

// ── Split whole note in 4/4 ───────────────────────────────────────────────────

TEST_CASE("SplitLong: whole note (16) in 4/4 at non-boundary → 4 equal segments of 4", "[split]") {
    HarmonicRhythmPlanner planner;
    // Quarter first shifts whole note to absoluteStart=4 (not a measure boundary).
    std::vector<Note> notes = { makeNote(NoteName::A, 4), makeNote(NoteName::C, 16) };
    auto segs = planner.buildSegments(notes, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 5);  // 1 (quarter) + 4 (equal-split whole)
    for (int i = 1; i <= 4; ++i)
        CHECK(segs[i].durationSixteenths == 4);
    CHECK(segs[1].offsetInFixedNoteSixteenths == 0);
    CHECK(segs[2].offsetInFixedNoteSixteenths == 4);
    CHECK(segs[3].offsetInFixedNoteSixteenths == 8);
    CHECK(segs[4].offsetInFixedNoteSixteenths == 12);
}

TEST_CASE("SplitLong: all sub-segments reference same fixedNote", "[split]") {
    HarmonicRhythmPlanner planner;
    // At pos 0 in 4/4 whole note yields cadence pattern [4,4,8] — 3 segments.
    // Property (shared fixedNote) holds regardless of split pattern.
    auto segs = planner.buildSegments({ makeNote(NoteName::G, 16) }, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 3);
    for (const auto& seg : segs)
        CHECK(seg.fixedNote.getName() == NoteName::G);
}

TEST_CASE("SplitLong: all sub-segments share sourceNoteIndex=0", "[split]") {
    HarmonicRhythmPlanner planner;
    // At pos 0 in 4/4 whole note yields cadence pattern [4,4,8] — 3 segments.
    // Property (shared sourceNoteIndex) holds regardless of split pattern.
    auto segs = planner.buildSegments({ makeNote(NoteName::C, 16) }, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 3);
    for (const auto& seg : segs)
        CHECK(seg.sourceNoteIndex == 0);
}

// ── Split half note in 4/4 ────────────────────────────────────────────────────

TEST_CASE("SplitLong: half note (8) in 4/4 → 2 segments of 4", "[split]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({ makeNote(NoteName::D, 8) }, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 2);
    CHECK(segs[0].durationSixteenths          == 4);
    CHECK(segs[0].offsetInFixedNoteSixteenths == 0);
    CHECK(segs[1].durationSixteenths          == 4);
    CHECK(segs[1].offsetInFixedNoteSixteenths == 4);
}

// ── Notes ≤ basePulse are not split ──────────────────────────────────────────

TEST_CASE("SplitLong: quarter (4) in 4/4 — equal to basePulse, not split", "[split]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({ makeNote(NoteName::E, 4) }, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].durationSixteenths          == 4);
    CHECK(segs[0].offsetInFixedNoteSixteenths == 0);
}

TEST_CASE("SplitLong: eighth (2) in 4/4 — shorter than basePulse, not split", "[split]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({ makeNote(NoteName::F, 2) }, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].durationSixteenths == 2);
}

TEST_CASE("SplitLong: sixteenth (1) in 4/4 — shorter than basePulse, not split", "[split]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({ makeNote(NoteName::G, 1) }, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].durationSixteenths == 1);
}

// ── Non-divisible duration is not split ──────────────────────────────────────

TEST_CASE("SplitLong: duration 6 in 4/4 (not divisible by basePulse 4) → not split", "[split]") {
    HarmonicRhythmPlanner planner;
    // dotted quarter = 6 sixteenths; 6 % 4 != 0 → stays as 1 segment
    auto segs = planner.buildSegments({ makeNote(NoteName::C, 6) }, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].durationSixteenths == 6);
}

// ── beatType 8 (eighth-note pulse) ───────────────────────────────────────────

TEST_CASE("SplitLong: beatType=8 gives basePulse=2; dotted-quarter (6) → 3 segments", "[split]") {
    HarmonicRhythmPlanner planner;
    // 6/8: beatType=8 → basePulse=2; 6 % 2 == 0 → [2,2,2]
    auto segs = planner.buildSegments({ makeNote(NoteName::C, 6) }, makeSettings(6, 8, true));
    REQUIRE(segs.size() == 3);
    for (const auto& seg : segs)
        CHECK(seg.durationSixteenths == 2);
    CHECK(segs[0].offsetInFixedNoteSixteenths == 0);
    CHECK(segs[1].offsetInFixedNoteSixteenths == 2);
    CHECK(segs[2].offsetInFixedNoteSixteenths == 4);
}

TEST_CASE("SplitLong: beatType=8 — eighth (2) equal to basePulse, not split", "[split]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({ makeNote(NoteName::A, 2) }, makeSettings(6, 8, true));
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].durationSixteenths == 2);
}

// ── Unsupported beatType falls back to no split ───────────────────────────────

TEST_CASE("SplitLong: beatType=3 (non-power-of-2) — no split regardless of flag", "[split]") {
    HarmonicRhythmPlanner planner;
    // 16 % 3 != 0 → basePulse=0 → no split
    auto segs = planner.buildSegments({ makeNote(NoteName::C, 12) }, makeSettings(4, 3, true));
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].durationSixteenths == 12);
}

// ── Rest splitting ────────────────────────────────────────────────────────────

TEST_CASE("SplitLong: whole rest at non-boundary split into 4 equal segments", "[split][rest]") {
    HarmonicRhythmPlanner planner;
    // Quarter before the rest shifts it to absoluteStart=4 (not a measure boundary).
    std::vector<Note> notes = { makeNote(NoteName::A, 4), makeRest(16) };
    auto segs = planner.buildSegments(notes, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 5);  // 1 (quarter) + 4 (equal-split rest)
    for (int i = 1; i <= 4; ++i) {
        CHECK(segs[i].isRest             == true);
        CHECK(segs[i].durationSixteenths == 4);
    }
    CHECK(segs[1].offsetInFixedNoteSixteenths == 0);
    CHECK(segs[4].offsetInFixedNoteSixteenths == 12);
}

TEST_CASE("SplitLong: rest shorter than basePulse not split", "[split][rest]") {
    HarmonicRhythmPlanner planner;
    auto segs = planner.buildSegments({ makeRest(2) }, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].isRest             == true);
    CHECK(segs[0].durationSixteenths == 2);
}

// ── Tied notes: merged span is then split ────────────────────────────────────

TEST_CASE("SplitLong: two tied quarters merged to half, then split → 2 segments", "[split][tie]") {
    HarmonicRhythmPlanner planner;
    // tie chain: C4 + C4 → totalDuration=8, then split into [4,4]
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4, /*tiedToNext=*/true),
        makeNote(NoteName::C, 4),
    };
    auto segs = planner.buildSegments(notes, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 2);
    CHECK(segs[0].durationSixteenths          == 4);
    CHECK(segs[0].offsetInFixedNoteSixteenths == 0);
    CHECK(segs[0].sourceNoteIndex             == 0);
    CHECK(segs[1].durationSixteenths          == 4);
    CHECK(segs[1].offsetInFixedNoteSixteenths == 4);
    CHECK(segs[1].sourceNoteIndex             == 0);
}

TEST_CASE("SplitLong: tied whole (4 quarters merged) at non-boundary → 4 equal segments", "[split][tie]") {
    HarmonicRhythmPlanner planner;
    // Quarter before the chain shifts it to absoluteStart=4 → equal split [4,4,4,4].
    std::vector<Note> notes = {
        makeNote(NoteName::A, 4),
        makeNote(NoteName::E, 4, true),
        makeNote(NoteName::E, 4, true),
        makeNote(NoteName::E, 4, true),
        makeNote(NoteName::E, 4),
    };
    auto segs = planner.buildSegments(notes, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 5);  // 1 (quarter A) + 4 (equal-split tied E)
    for (int i = 1; i <= 4; ++i) {
        CHECK(segs[i].durationSixteenths          == 4);
        CHECK(segs[i].offsetInFixedNoteSixteenths == (i - 1) * 4);
        CHECK(segs[i].sourceNoteIndex             == 1);
        CHECK(segs[i].fixedNote.getName()         == NoteName::E);
    }
}

// ── Mixed notes: split long, leave short ─────────────────────────────────────

TEST_CASE("SplitLong: mixed sequence — only long notes are split", "[split]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4),   // quarter → 1 segment
        makeNote(NoteName::D, 8),   // half    → 2 segments
        makeNote(NoteName::E, 2),   // eighth  → 1 segment
    };
    auto segs = planner.buildSegments(notes, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 4);

    CHECK(segs[0].fixedNote.getName() == NoteName::C);
    CHECK(segs[0].durationSixteenths  == 4);

    CHECK(segs[1].fixedNote.getName()          == NoteName::D);
    CHECK(segs[1].durationSixteenths           == 4);
    CHECK(segs[1].offsetInFixedNoteSixteenths  == 0);
    CHECK(segs[1].sourceNoteIndex              == 1);

    CHECK(segs[2].fixedNote.getName()          == NoteName::D);
    CHECK(segs[2].durationSixteenths           == 4);
    CHECK(segs[2].offsetInFixedNoteSixteenths  == 4);
    CHECK(segs[2].sourceNoteIndex              == 1);

    CHECK(segs[3].fixedNote.getName() == NoteName::E);
    CHECK(segs[3].durationSixteenths  == 2);
    CHECK(segs[3].sourceNoteIndex     == 2);
}

TEST_CASE("SplitLong: sourceNoteIndex reflects original note ordering after splits", "[split]") {
    HarmonicRhythmPlanner planner;
    // Whole note at pos 0 in 4/4 → cadence pattern [4,4,8] = 3 segs; then quarter = 1 seg.
    std::vector<Note> notes = {
        makeNote(NoteName::C, 16),  // idx 0 → 3 segs (cadence)
        makeNote(NoteName::G,  4),  // idx 1 → 1 seg
    };
    auto segs = planner.buildSegments(notes, makeSettings(4, 4, true));
    REQUIRE(segs.size() == 4);
    for (int i = 0; i < 3; ++i)
        CHECK(segs[i].sourceNoteIndex == 0);
    CHECK(segs[3].sourceNoteIndex == 1);
}
