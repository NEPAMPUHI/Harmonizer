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

// Returns true if variants are ordered non-decreasingly by rhythmPlanIndex.
static bool planOrderPreserved(const std::vector<HarmonizationVariant>& variants) {
    int prev = -1;
    for (const auto& v : variants) {
        if (v.rhythmPlanIndex < prev) return false;
        prev = v.rhythmPlanIndex;
    }
    return true;
}

// ── rhythmPlanIndex: default settings (1 plan) ───────────────────────────────

TEST_CASE("MultiPlan/melody: rhythmPlanIndex=0 for all variants with single plan", "[multi-plan]") {
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

TEST_CASE("MultiPlan/bass: rhythmPlanIndex=0 for all variants with single plan", "[multi-plan]") {
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

// ── Multi-plan: both plan indices appear in result ────────────────────────────

TEST_CASE("MultiPlan/melody: whole note with split → variants from both plans present", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());

    bool seen0 = false, seen1 = false;
    for (const auto& v : variants) {
        if (v.rhythmPlanIndex == 0) seen0 = true;
        if (v.rhythmPlanIndex == 1) seen1 = true;
    }
    CHECK(seen0);
    CHECK(seen1);
}

TEST_CASE("MultiPlan/bass: whole note with split → variants from both plans present", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::G, 2) };

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());

    bool seen0 = false, seen1 = false;
    for (const auto& v : variants) {
        if (v.rhythmPlanIndex == 0) seen0 = true;
        if (v.rhythmPlanIndex == 1) seen1 = true;
    }
    CHECK(seen0);
    CHECK(seen1);
}

// ── Final output ordering: sorted by score descending ───────────────────────

TEST_CASE("MultiPlan/melody: output variants are ordered by non-increasing score", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (size_t i = 1; i < variants.size(); ++i)
        CHECK(variants[i].score <= variants[i - 1].score);
}

TEST_CASE("MultiPlan/bass: output variants are ordered by non-increasing score", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::G, 2) };

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    REQUIRE_FALSE(variants.empty());
    for (size_t i = 1; i < variants.size(); ++i)
        CHECK(variants[i].score <= variants[i - 1].score);
}

// ── Each plan produces independent harmonization ─────────────────────────────

TEST_CASE("MultiPlan/melody: plan 0 positions count matches [4,4,8] pattern", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    // All plan-0 variants should have 3 positions (from [4,4,8]).
    for (const auto& v : variants) {
        if (v.rhythmPlanIndex == 0)
            CHECK(v.musicScore.positions.size() == 3);
    }
}

TEST_CASE("MultiPlan/melody: plan 1 positions count matches [4,4,4,4] pattern", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    // All plan-1 variants should have 4 positions (from [4,4,4,4]).
    for (const auto& v : variants) {
        if (v.rhythmPlanIndex == 1)
            CHECK(v.musicScore.positions.size() == 4);
    }
}

TEST_CASE("MultiPlan/melody: plan 0 last position has duration 8", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    for (const auto& v : variants) {
        if (v.rhythmPlanIndex == 0 && !v.musicScore.positions.empty())
            CHECK(v.musicScore.positions.back().durationSixteenths == 8);
    }
}

TEST_CASE("MultiPlan/melody: plan 1 all positions have duration 4", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/true));

    for (const auto& v : variants) {
        if (v.rhythmPlanIndex == 1) {
            for (const auto& pos : v.musicScore.positions)
                CHECK(pos.durationSixteenths == 4);
        }
    }
}

// ── maxHarmonicRhythmPlans limits plans entering harmonizer ──────────────────

TEST_CASE("MultiPlan/melody: maxHarmonicRhythmPlans=1 produces only plan-0 variants", "[multi-plan]") {
    ScoreInput input;
    input.notes = { wholeNote(NoteName::C, 5) };

    auto settings = makeSettings(/*split=*/true);
    settings.maxHarmonicRhythmPlans = 1;

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, settings);

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

// ── No regressions: split=false behaves identically to before ────────────────

TEST_CASE("MultiPlan/melody: C-E-G-C quarter notes produce variants (regression)", "[multi-plan]") {
    ScoreInput input;
    input.notes = { quarterNote(NoteName::C, 5), quarterNote(NoteName::E, 5),
                    quarterNote(NoteName::G, 5), quarterNote(NoteName::C, 5) };

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/false));

    REQUIRE_FALSE(variants.empty());
    // split=false → 1 plan → all rhythmPlanIndex=0
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

TEST_CASE("MultiPlan/bass: C-G-C-G quarter notes produce variants (regression)", "[multi-plan]") {
    ScoreInput input;
    input.notes = { quarterNote(NoteName::C, 3), quarterNote(NoteName::G, 2),
                    quarterNote(NoteName::C, 3), quarterNote(NoteName::G, 2) };

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeSettings(/*split=*/false));

    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}
