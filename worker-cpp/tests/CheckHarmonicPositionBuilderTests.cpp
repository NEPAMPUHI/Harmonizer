#include <catch2/catch_test_macros.hpp>
#include "checking/CheckHarmonicPositionBuilder.h"
#include "domain/CheckSolutionInput.h"
#include "domain/Note.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static Note pitch(NoteName n, int oct, int dur) {
    return Note(n, oct, 0, 0, dur, false);
}
static Note rest(int dur) {
    return Note(NoteName::C, 4, 0, 0, dur, false, false, true);
}

static CheckSolutionInput oneVoiceMeasure(
    std::vector<Note> sop, std::vector<Note> alt,
    std::vector<Note> ten, std::vector<Note> bas) {
    CheckMeasureInput m;
    m.soprano = std::move(sop);
    m.alto    = std::move(alt);
    m.tenor   = std::move(ten);
    m.bass    = std::move(bas);
    CheckSolutionInput in;
    in.measures.push_back(std::move(m));
    return in;
}

// ── Test 1: all voices, one quarter note each → 1 position, all hasX true ────

TEST_CASE("CHPB: one quarter per voice → 1 position, all voices present",
          "[checkbuilder]") {
    auto input = oneVoiceMeasure(
        { pitch(NoteName::E, 5, 4) },
        { pitch(NoteName::C, 5, 4) },
        { pitch(NoteName::G, 4, 4) },
        { pitch(NoteName::C, 3, 4) }
    );

    CheckHarmonicPositionBuilder builder;
    const auto positions = builder.build(input);

    REQUIRE(positions.size() == 1);
    const auto& p = positions[0];

    CHECK(p.measureIndex                == 1);
    CHECK(p.positionInMeasureSixteenths == 0);
    CHECK(p.durationSixteenths          == 4);

    CHECK(p.hasSoprano);
    CHECK(p.hasAlto);
    CHECK(p.hasTenor);
    CHECK(p.hasBass);

    CHECK(p.soprano.getName() == NoteName::E);
    CHECK(p.alto.getName()    == NoteName::C);
    CHECK(p.tenor.getName()   == NoteName::G);
    CHECK(p.bass.getName()    == NoteName::C);
}

// ── Test 2: soprano 2×quarter, others 1×half → 2 positions ──────────────────

TEST_CASE("CHPB: soprano split in half → 2 positions, all voices in each",
          "[checkbuilder]") {
    // soprano: [0..4) [4..8)   others: [0..8)
    auto input = oneVoiceMeasure(
        { pitch(NoteName::E, 5, 4), pitch(NoteName::D, 5, 4) },
        { pitch(NoteName::C, 5, 8) },
        { pitch(NoteName::G, 4, 8) },
        { pitch(NoteName::C, 3, 8) }
    );

    CheckHarmonicPositionBuilder builder;
    const auto positions = builder.build(input);

    REQUIRE(positions.size() == 2);

    // position 0: [0, 4)
    CHECK(positions[0].positionInMeasureSixteenths == 0);
    CHECK(positions[0].durationSixteenths          == 4);
    CHECK(positions[0].hasSoprano);
    CHECK(positions[0].soprano.getName() == NoteName::E);
    CHECK(positions[0].hasAlto);
    CHECK(positions[0].hasTenor);
    CHECK(positions[0].hasBass);

    // position 1: [4, 8)
    CHECK(positions[1].positionInMeasureSixteenths == 4);
    CHECK(positions[1].durationSixteenths          == 4);
    CHECK(positions[1].hasSoprano);
    CHECK(positions[1].soprano.getName() == NoteName::D);
    CHECK(positions[1].hasAlto);
    CHECK(positions[1].hasTenor);
    CHECK(positions[1].hasBass);
}

// ── Test 3: soprano shorter than others → gap in second segment ──────────────

TEST_CASE("CHPB: soprano ends early → hasSoprano false in tail segment",
          "[checkbuilder]") {
    // soprano: [0..4)   others: [0..8)
    auto input = oneVoiceMeasure(
        { pitch(NoteName::E, 5, 4) },
        { pitch(NoteName::C, 5, 8) },
        { pitch(NoteName::G, 4, 8) },
        { pitch(NoteName::C, 3, 8) }
    );

    CheckHarmonicPositionBuilder builder;
    const auto positions = builder.build(input);

    REQUIRE(positions.size() == 2);

    // segment [0,4): soprano present
    CHECK(positions[0].hasSoprano);
    CHECK(positions[0].hasAlto);
    CHECK(positions[0].hasTenor);
    CHECK(positions[0].hasBass);

    // segment [4,8): soprano absent
    CHECK_FALSE(positions[1].hasSoprano);
    CHECK(positions[1].hasAlto);
    CHECK(positions[1].hasTenor);
    CHECK(positions[1].hasBass);
    CHECK(positions[1].positionInMeasureSixteenths == 4);
    CHECK(positions[1].durationSixteenths          == 4);
}

// ── Test 4: rest in bass → hasVoice true, note.isRest() true ─────────────────

TEST_CASE("CHPB: bass rest → hasBass true and bass note is rest",
          "[checkbuilder]") {
    auto input = oneVoiceMeasure(
        { pitch(NoteName::E, 5, 4) },
        { pitch(NoteName::C, 5, 4) },
        { pitch(NoteName::G, 4, 4) },
        { rest(4) }
    );

    CheckHarmonicPositionBuilder builder;
    const auto positions = builder.build(input);

    REQUIRE(positions.size() == 1);
    const auto& p = positions[0];

    CHECK(p.hasBass);
    CHECK(p.bass.isRest());
    CHECK(p.bass.getDurationSixteenths() == 4);

    // Other voices should be pitched
    CHECK_FALSE(p.soprano.isRest());
    CHECK_FALSE(p.alto.isRest());
    CHECK_FALSE(p.tenor.isRest());
}

// ── Test 5: two measures → measureIndex 1 and 2 ──────────────────────────────

TEST_CASE("CHPB: two measures → positions have correct measureIndex",
          "[checkbuilder]") {
    CheckMeasureInput m0, m1;
    m0.soprano = { pitch(NoteName::E, 5, 4) };
    m0.alto    = { pitch(NoteName::C, 5, 4) };
    m0.tenor   = { pitch(NoteName::G, 4, 4) };
    m0.bass    = { pitch(NoteName::C, 3, 4) };

    m1.soprano = { pitch(NoteName::D, 5, 8) };
    m1.alto    = { pitch(NoteName::B, 4, 8) };
    m1.tenor   = { pitch(NoteName::G, 4, 8) };
    m1.bass    = { pitch(NoteName::G, 3, 8) };

    CheckSolutionInput input;
    input.measures.push_back(m0);
    input.measures.push_back(m1);

    CheckHarmonicPositionBuilder builder;
    const auto positions = builder.build(input);

    REQUIRE(positions.size() == 2);
    CHECK(positions[0].measureIndex == 1);
    CHECK(positions[1].measureIndex == 2);
    CHECK(positions[0].durationSixteenths == 4);
    CHECK(positions[1].durationSixteenths == 8);
}

// ── Test 6: empty input → zero positions ─────────────────────────────────────

TEST_CASE("CHPB: empty input → no positions", "[checkbuilder]") {
    CheckSolutionInput input;

    CheckHarmonicPositionBuilder builder;
    CHECK(builder.build(input).empty());
}

// ── Test 7: measure where all voices are empty → no positions from that measure

TEST_CASE("CHPB: all voices empty in a measure → zero positions", "[checkbuilder]") {
    auto input = oneVoiceMeasure({}, {}, {}, {});

    CheckHarmonicPositionBuilder builder;
    CHECK(builder.build(input).empty());
}
