#include <catch2/catch_test_macros.hpp>
#include "domain/Note.h"
#include "harmonization/HarmonicRhythmPlanner.h"

static Note makeNote(NoteName name, int dur, bool tiedToNext = false) {
    return Note(name, 4, 0, 1, dur, false, tiedToNext, false);
}

static Note makeRest(int dur) {
    return Note(NoteName::C, 4, 0, 0, dur, false, false, true);
}

static HarmonizationSettings makeSettings() {
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

// ── Note accessors ────────────────────────────────────────────────────────────

TEST_CASE("Note::isRest returns false for normal note", "[note][rest]") {
    auto n = makeNote(NoteName::C, 4);
    CHECK_FALSE(n.isRest());
}

TEST_CASE("Note::isRest returns true for rest", "[note][rest]") {
    auto r = makeRest(4);
    CHECK(r.isRest());
}

TEST_CASE("Note::isTiedToNext returns false by default", "[note][tie]") {
    auto n = makeNote(NoteName::C, 4);
    CHECK_FALSE(n.isTiedToNext());
}

TEST_CASE("Note::isTiedToNext returns true when set", "[note][tie]") {
    auto n = makeNote(NoteName::C, 4, /*tiedToNext=*/true);
    CHECK(n.isTiedToNext());
}

TEST_CASE("Note::getDurationSixteenths preserved for rest", "[note][rest]") {
    auto r = makeRest(8);
    CHECK(r.getDurationSixteenths() == 8);
}

// ── Tie grouping ──────────────────────────────────────────────────────────────

TEST_CASE("HarmonicRhythmPlanner: two tied same-pitch notes → one segment", "[rhythm-planner][tie]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 8, /*tiedToNext=*/true),
        makeNote(NoteName::C, 4),
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 1);
    CHECK(segments[0].durationSixteenths == 12);
    CHECK(segments[0].sourceNoteIndex    == 0);
    CHECK(segments[0].fixedNote.getName() == NoteName::C);
    CHECK_FALSE(segments[0].isRest);
}

TEST_CASE("HarmonicRhythmPlanner: three tied same-pitch notes → merged span, then cadence split", "[rhythm-planner][tie]") {
    // E/4+E/4+E/8 ties merge into a 16-sixteenth span at pos 0 in 4/4.
    // Cadence split fires unconditionally → [4,4,8] (3 segments).
    // All segments share sourceNoteIndex=0, confirming the tie was merged.
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::E, 4, true),
        makeNote(NoteName::E, 4, true),
        makeNote(NoteName::E, 8),
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 3);
    CHECK(segments[0].durationSixteenths == 4);
    CHECK(segments[1].durationSixteenths == 4);
    CHECK(segments[2].durationSixteenths == 8);
    for (const auto& seg : segments) {
        CHECK(seg.sourceNoteIndex     == 0);
        CHECK(seg.fixedNote.getName() == NoteName::E);
    }
}

TEST_CASE("HarmonicRhythmPlanner: tied notes with different pitch → two segments", "[rhythm-planner][tie]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4, true),
        makeNote(NoteName::D, 4),
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 2);
    CHECK(segments[0].durationSixteenths  == 4);
    CHECK(segments[0].fixedNote.getName() == NoteName::C);
    CHECK(segments[1].durationSixteenths  == 4);
    CHECK(segments[1].fixedNote.getName() == NoteName::D);
}

TEST_CASE("HarmonicRhythmPlanner: tie chain followed by separate note", "[rhythm-planner][tie]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::G, 4, true),
        makeNote(NoteName::G, 4),       // end of tie chain
        makeNote(NoteName::A, 4),       // separate note
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 2);
    CHECK(segments[0].durationSixteenths  == 8);
    CHECK(segments[0].sourceNoteIndex     == 0);
    CHECK(segments[1].durationSixteenths  == 4);
    CHECK(segments[1].sourceNoteIndex     == 2);
}

TEST_CASE("HarmonicRhythmPlanner: sourceNoteIndex skips consumed tie members", "[rhythm-planner][tie]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4, true),  // index 0
        makeNote(NoteName::C, 4),        // index 1 — merged
        makeNote(NoteName::E, 4),        // index 2 — next segment
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 2);
    CHECK(segments[0].sourceNoteIndex == 0);
    CHECK(segments[1].sourceNoteIndex == 2);
}

// ── Rest handling ─────────────────────────────────────────────────────────────

TEST_CASE("HarmonicRhythmPlanner: single rest → one segment with isRest=true", "[rhythm-planner][rest]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = { makeRest(4) };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 1);
    CHECK(segments[0].isRest               == true);
    CHECK(segments[0].durationSixteenths   == 4);
    CHECK(segments[0].sourceNoteIndex      == 0);
}

TEST_CASE("HarmonicRhythmPlanner: rest in the middle keeps isRest flag", "[rhythm-planner][rest]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4),
        makeRest(4),
        makeNote(NoteName::E, 4),
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 3);
    CHECK_FALSE(segments[0].isRest);
    CHECK(segments[1].isRest == true);
    CHECK_FALSE(segments[2].isRest);
}

TEST_CASE("HarmonicRhythmPlanner: rest with tiedToNext is NOT merged into next note", "[rhythm-planner][rest]") {
    HarmonicRhythmPlanner planner;
    // Rest with tiedToNext=true should still produce two segments:
    // rests cannot participate in tie chains.
    Note rest = Note(NoteName::C, 4, 0, 0, 4, false, /*tiedToNext=*/true, /*isRest=*/true);
    std::vector<Note> notes = {
        rest,
        makeNote(NoteName::C, 4),
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 2);
    CHECK(segments[0].isRest              == true);
    CHECK(segments[0].durationSixteenths  == 4);
    CHECK(segments[1].isRest              == false);
    CHECK(segments[1].durationSixteenths  == 4);
}

TEST_CASE("HarmonicRhythmPlanner: note tiedToNext cannot merge into a rest", "[rhythm-planner][rest]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::C, 4, /*tiedToNext=*/true),
        makeRest(4),
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 2);
    CHECK_FALSE(segments[0].isRest);
    CHECK(segments[1].isRest == true);
}

TEST_CASE("HarmonicRhythmPlanner: mixed ties and rests, sourceNoteIndex correct", "[rhythm-planner][rest][tie]") {
    HarmonicRhythmPlanner planner;
    std::vector<Note> notes = {
        makeNote(NoteName::D, 4, true),  // 0 - merged into span
        makeNote(NoteName::D, 4),        // 1 - merged → segment 0, sourceNoteIndex=0
        makeRest(4),                     // 2    → segment 1, sourceNoteIndex=2
        makeNote(NoteName::F, 4),        // 3    → segment 2, sourceNoteIndex=3
    };
    auto segments = planner.buildSegments(notes, makeSettings());

    REQUIRE(segments.size() == 3);
    CHECK(segments[0].sourceNoteIndex    == 0);
    CHECK(segments[0].durationSixteenths == 8);
    CHECK(segments[1].sourceNoteIndex    == 2);
    CHECK(segments[1].isRest             == true);
    CHECK(segments[2].sourceNoteIndex    == 3);
    CHECK_FALSE(segments[2].isRest);
}
