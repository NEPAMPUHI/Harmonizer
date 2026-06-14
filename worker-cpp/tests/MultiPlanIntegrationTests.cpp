#include <catch2/catch_test_macros.hpp>
#include "harmonization/MelodyHarmonizer.h"
#include "harmonization/BassHarmonizer.h"
#include "domain/ScoreInput.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static HarmonizationSettings makeSettings(bool split = false) {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural", "harmonic", "melodic"};
    s.allowedChords = {"T53", "T6", "S53", "S6", "D53", "D6", "D7"};
    s.timeSignature.beats    = 4;
    s.timeSignature.beatType = 4;
    s.anacrusisSixteenths    = 0;
    s.measureCount           = 1;
    s.splitLongNotesByBasePulse = split;
    return s;
}

// Whole note at beat 1 of a measure.
static Note wholeNote(NoteName name, int octave) {
    return Note(name, octave, 0, 0, 16, /*isStrongBeat=*/true);
}

// Quarter note, unresolved degree.
static Note quarterNote(NoteName name, int octave) {
    return Note(name, octave, 0, 0, 4, /*isStrongBeat=*/true);
}

// ── rhythmPlanIndex: always 0 ────────────────────────────────────────────────

TEST_CASE("SinglePlan/melody: rhythmPlanIndex=0 for all variants", "[single-plan]") {
    ScoreInput input;
    input.notes = { quarterNote(NoteName::C, 5),
                    quarterNote(NoteName::E, 5),
                    quarterNote(NoteName::G, 5),
                    quarterNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/false));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

TEST_CASE("SinglePlan/bass: rhythmPlanIndex=0 for all variants", "[single-plan]") {
    ScoreInput input;
    input.notes = { quarterNote(NoteName::C, 3),
                    quarterNote(NoteName::G, 2),
                    quarterNote(NoteName::C, 3),
                    quarterNote(NoteName::G, 2) };

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/false));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

TEST_CASE("SinglePlan/melody: whole note with split → rhythmPlanIndex=0 for all", "[single-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

TEST_CASE("SinglePlan/bass: whole note with split → rhythmPlanIndex=0 for all", "[single-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::G, 2) };

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

// ── rhythmPlanPriority: always 0 ─────────────────────────────────────────────

TEST_CASE("SinglePlan/melody: rhythmPlanPriority=0 for all variants", "[single-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanPriority == 0);
}

// ── Whole note in 4/4: uses cadence pattern [4,4,8] ─────────────────────────

TEST_CASE("SinglePlan/melody: whole note → 3 positions from cadence pattern [4,4,8]", "[single-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.musicScore.positions.size() == 3);
}

TEST_CASE("SinglePlan/melody: whole note last position has duration 8 (half)", "[single-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants) {
        if (!v.musicScore.positions.empty())
            CHECK(v.musicScore.positions.back().durationSixteenths == 8);
    }
}

TEST_CASE("SinglePlan/bass: whole note → 3 positions from cadence pattern [4,4,8]", "[single-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::G, 2) };

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.musicScore.positions.size() == 3);
}

// ── Final output ordering: sorted by score descending ───────────────────────

TEST_CASE("SinglePlan/melody: output variants are ordered by non-increasing score", "[single-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (size_t i = 1; i < variants.size(); ++i)
        CHECK(variants[i].score <= variants[i - 1].score);
}

TEST_CASE("SinglePlan/bass: output variants are ordered by non-increasing score", "[single-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::G, 2) };

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (size_t i = 1; i < variants.size(); ++i)
        CHECK(variants[i].score <= variants[i - 1].score);
}

// ── No regressions: basic harmonization still works ──────────────────────────

TEST_CASE("SinglePlan/melody: C-E-G-C quarter notes produce variants (regression)", "[single-plan]") {
    ScoreInput input;
    input.notes = { quarterNote(NoteName::C, 5), quarterNote(NoteName::E, 5),
                    quarterNote(NoteName::G, 5), quarterNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/false));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

TEST_CASE("SinglePlan/bass: C-G-C-G quarter notes produce variants (regression)", "[single-plan]") {
    ScoreInput input;
    input.notes = { quarterNote(NoteName::C, 3), quarterNote(NoteName::G, 2),
                    quarterNote(NoteName::C, 3), quarterNote(NoteName::G, 2) };

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/false));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}
