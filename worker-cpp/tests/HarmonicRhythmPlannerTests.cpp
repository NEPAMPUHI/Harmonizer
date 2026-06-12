#include <catch2/catch_test_macros.hpp>
#include "harmonization/HarmonicRhythmPlanner.h"
#include "domain/HarmonicPositionBuilder.h"
#include "domain/HarmonicSegment.h"

static HarmonizationSettings makeSettings4_4() {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural"};
    s.allowedChords = {"T53"};
    s.timeSignature.beats    = 4;
    s.timeSignature.beatType = 4;
    s.anacrusisSixteenths    = 0;
    s.measureCount           = 1;
    return s;
}

static Note makeNote(NoteName name, int dur) {
    return Note(name, 4, 0, 1, dur, false);
}

// ── HarmonicRhythmPlanner ─────────────────────────────────────────────────────

TEST_CASE("HarmonicRhythmPlanner: empty input produces empty segments", "[rhythm-planner]") {
    HarmonicRhythmPlanner planner;
    auto segments = planner.buildSegments({}, makeSettings4_4());
    CHECK(segments.empty());
}

TEST_CASE("HarmonicRhythmPlanner: one note → one segment", "[rhythm-planner]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = { makeNote(NoteName::C, 4) };
    auto segments = planner.buildSegments(notes, makeSettings4_4());

    REQUIRE(segments.size() == 1);
    CHECK(segments[0].durationSixteenths          == 4);
    CHECK(segments[0].offsetInFixedNoteSixteenths == 0);
    CHECK(segments[0].sourceNoteIndex             == 0);
    CHECK(segments[0].fixedNote.getName()         == NoteName::C);
}

TEST_CASE("HarmonicRhythmPlanner: N notes → N segments, 1:1 mapping", "[rhythm-planner]") {
    // Uses a dotted-half (12) as the longest note — avoids the whole-note cadence
    // split so the trivial 1:1 mapping holds for all five notes.
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 12),   // dotted-half
        makeNote(NoteName::E,  8),   // half
        makeNote(NoteName::G,  4),   // quarter
        makeNote(NoteName::B,  2),   // eighth
        makeNote(NoteName::D,  1),   // sixteenth
    };
    auto segments = planner.buildSegments(notes, makeSettings4_4());

    REQUIRE(segments.size() == 5);
    CHECK(segments[0].durationSixteenths == 12);
    CHECK(segments[1].durationSixteenths ==  8);
    CHECK(segments[2].durationSixteenths ==  4);
    CHECK(segments[3].durationSixteenths ==  2);
    CHECK(segments[4].durationSixteenths ==  1);
}

TEST_CASE("HarmonicRhythmPlanner: sourceNoteIndex increments sequentially", "[rhythm-planner]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4),
        makeNote(NoteName::D, 4),
        makeNote(NoteName::E, 4),
    };
    auto segments = planner.buildSegments(notes, makeSettings4_4());

    REQUIRE(segments.size() == 3);
    CHECK(segments[0].sourceNoteIndex == 0);
    CHECK(segments[1].sourceNoteIndex == 1);
    CHECK(segments[2].sourceNoteIndex == 2);
}

TEST_CASE("HarmonicRhythmPlanner: offsetInFixedNoteSixteenths is always 0 (trivial plan)", "[rhythm-planner]") {
    // Half notes produce no subdivision, so both segments have offset 0.
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 8),
        makeNote(NoteName::G, 8),
    };
    auto segments = planner.buildSegments(notes, makeSettings4_4());

    REQUIRE(segments.size() == 2);
    CHECK(segments[0].offsetInFixedNoteSixteenths == 0);
    CHECK(segments[1].offsetInFixedNoteSixteenths == 0);
}

// ── HarmonicPositionBuilder with segments ────────────────────────────────────

TEST_CASE("HarmonicPositionBuilder: 4 quarter notes in 4/4, no anacrusis", "[position-builder]") {
    auto settings = makeSettings4_4();
    HarmonicRhythmPlanner planner;
    HarmonicPositionBuilder builder;

    std::vector<Note> notes = {
        makeNote(NoteName::C, 4),
        makeNote(NoteName::D, 4),
        makeNote(NoteName::E, 4),
        makeNote(NoteName::F, 4),
    };
    auto segments  = planner.buildSegments(notes, settings);
    auto positions = builder.build(segments, settings, HarmonizationMode::HarmonizeMelody);

    REQUIRE(positions.size() == 4);

    // All in measure 1
    for (const auto& p : positions)
        CHECK(p.measureIndex == 1);

    // startSixteenth: 0, 4, 8, 12
    CHECK(positions[0].startSixteenth == 0);
    CHECK(positions[1].startSixteenth == 4);
    CHECK(positions[2].startSixteenth == 8);
    CHECK(positions[3].startSixteenth == 12);

    // Beat types in 4/4: strong, weak, strong, weak
    CHECK(positions[0].isStrongBeat == true);
    CHECK(positions[1].isWeakBeat   == true);
    CHECK(positions[2].isStrongBeat == true);
    CHECK(positions[3].isWeakBeat   == true);

    // durations
    for (const auto& p : positions)
        CHECK(p.durationSixteenths == 4);
}

TEST_CASE("HarmonicPositionBuilder: anacrusis + main measure", "[position-builder]") {
    auto settings = makeSettings4_4();
    settings.anacrusisSixteenths = 4; // 1-beat anacrusis

    HarmonicRhythmPlanner planner;
    HarmonicPositionBuilder builder;

    std::vector<Note> notes = {
        makeNote(NoteName::G, 4),   // anacrusis beat
        makeNote(NoteName::C, 4),
        makeNote(NoteName::D, 4),
        makeNote(NoteName::E, 4),
        makeNote(NoteName::F, 4),
    };
    auto segments  = planner.buildSegments(notes, settings);
    auto positions = builder.build(segments, settings, HarmonizationMode::HarmonizeMelody);

    REQUIRE(positions.size() == 5);
    CHECK(positions[0].measureIndex    == 0);   // anacrusis
    CHECK(positions[0].startSixteenth  == 0);
    CHECK(positions[1].measureIndex    == 1);   // beat 1 of main measure
    CHECK(positions[1].startSixteenth  == 0);
    CHECK(positions[1].isStrongBeat    == true);
}

TEST_CASE("HarmonicPositionBuilder: fixedNote is preserved from segment", "[position-builder]") {
    auto settings = makeSettings4_4();
    HarmonicRhythmPlanner planner;
    HarmonicPositionBuilder builder;

    std::vector<Note> notes = {
        makeNote(NoteName::E, 4),
        makeNote(NoteName::G, 4),
    };
    auto segments  = planner.buildSegments(notes, settings);
    auto positions = builder.build(segments, settings, HarmonizationMode::HarmonizeMelody);

    REQUIRE(positions.size() == 2);
    CHECK(positions[0].fixedNote.getName() == NoteName::E);
    CHECK(positions[1].fixedNote.getName() == NoteName::G);
}
