#include <catch2/catch_test_macros.hpp>
#include "harmonization/HarmonicRhythmPlanner.h"
#include "harmonization/MelodyHarmonizer.h"
#include "harmonization/BassHarmonizer.h"
#include "domain/HarmonizationVariant.h"
#include "domain/ScoreInput.h"
#include <unordered_set>
#include <string>

// ── Helpers ───────────────────────────────────────────────────────────────────

static HarmonizationSettings makeS(bool split = false) {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural", "harmonic", "melodic"};
    s.allowedChords = {"T53", "T6", "S53", "S6", "D53", "D6", "D7"};
    s.timeSignature.beats    = 4;
    s.timeSignature.beatType = 4;
    s.anacrusisSixteenths    = 0;
    s.measureCount           = 2;
    s.splitLongNotesByBasePulse = split;
    return s;
}

static Note mkNote(NoteName name, int octave, int dur) {
    return Note(name, octave, 0, 0, dur, true);
}

// Canonical SATB semitone string for a variant — used for dedup checks in tests.
static std::string chordSeqKey(const HarmonizationVariant& v) {
    std::string key;
    for (const auto& chord : v.musicScore.chords) {
        key += std::to_string(chord.getSoprano().getSemitone()); key += ',';
        key += std::to_string(chord.getAlto().getSemitone());    key += ',';
        key += std::to_string(chord.getTenor().getSemitone());   key += ',';
        key += std::to_string(chord.getBass().getSemitone());    key += '|';
    }
    return key;
}

// ── HarmonicRhythmPlan struct ─────────────────────────────────────────────────

TEST_CASE("RhythmRanking: HarmonicRhythmPlan default has no segments", "[rhythm-ranking]") {
    HarmonicRhythmPlan plan;
    CHECK(plan.segments.empty());
}

TEST_CASE("RhythmRanking: single plan (no split) has exactly 1 plan", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ mkNote(NoteName::C, 5, 4) }, makeS(false));
    REQUIRE(plans.size() == 1);
}

TEST_CASE("RhythmRanking: whole note at 4/4 boundary → 1 plan with cadence [4,4,8]", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ mkNote(NoteName::C, 5, 16) }, makeS(true));
    REQUIRE(plans.size() == 1);
    REQUIRE(plans[0].segments.size() == 3);
    CHECK(plans[0].segments[0].durationSixteenths == 4);
    CHECK(plans[0].segments[1].durationSixteenths == 4);
    CHECK(plans[0].segments[2].durationSixteenths == 8);
}

TEST_CASE("RhythmRanking: two whole notes in 4/4 → 1 plan with 6 segments", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans(
        { mkNote(NoteName::C, 5, 16), mkNote(NoteName::E, 5, 16) }, makeS(true));
    REQUIRE(plans.size() == 1);
    CHECK(plans[0].segments.size() == 6);
}

// ── HarmonizationVariant: rhythmPlanIndex and rhythmPlanPriority always 0 ────

TEST_CASE("RhythmRanking: HarmonizationVariant default rhythmPlanPriority is 0", "[rhythm-ranking]") {
    HarmonizationVariant v;
    CHECK(v.rhythmPlanPriority == 0);
}

TEST_CASE("RhythmRanking: melody variants always carry rhythmPlanIndex 0", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 16) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

TEST_CASE("RhythmRanking: melody variants always carry rhythmPlanPriority 0", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 16) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanPriority == 0);
}

TEST_CASE("RhythmRanking: bass variants always carry rhythmPlanIndex 0", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::G, 2, 16) };
    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanIndex == 0);
}

TEST_CASE("RhythmRanking: split=false → all variants have rhythmPlanPriority 0", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 4), mkNote(NoteName::E, 5, 4),
                    mkNote(NoteName::G, 5, 4), mkNote(NoteName::C, 5, 4) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(false));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        CHECK(v.rhythmPlanPriority == 0);
}

// ── Deduplication ─────────────────────────────────────────────────────────────

TEST_CASE("RhythmRanking: no duplicate chord sequences in melody output (split)", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 16) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    std::unordered_set<std::string> seen;
    for (const auto& v : variants)
        CHECK(seen.insert(chordSeqKey(v)).second);
}

TEST_CASE("RhythmRanking: no duplicate chord sequences in bass output (split)", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::G, 2, 16) };
    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    std::unordered_set<std::string> seen;
    for (const auto& v : variants)
        CHECK(seen.insert(chordSeqKey(v)).second);
}

TEST_CASE("RhythmRanking: dedup preserves all distinct melody harmonizations", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 4), mkNote(NoteName::E, 5, 4),
                    mkNote(NoteName::G, 5, 4), mkNote(NoteName::C, 5, 4) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(false));
    REQUIRE(variants.size() >= 2);
    std::unordered_set<std::string> seen;
    for (const auto& v : variants)
        seen.insert(chordSeqKey(v));
    CHECK(seen.size() == variants.size());
}

// ── Output ordering ───────────────────────────────────────────────────────────

TEST_CASE("RhythmRanking: output ordered by non-increasing score (melody)", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 16) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    for (size_t i = 1; i < variants.size(); ++i)
        CHECK(variants[i].score <= variants[i - 1].score);
}

TEST_CASE("RhythmRanking: output ordered by non-increasing score (bass)", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::G, 2, 16) };
    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    for (size_t i = 1; i < variants.size(); ++i)
        CHECK(variants[i].score <= variants[i - 1].score);
}

TEST_CASE("RhythmRanking: split=false output score is non-increasing (melody)", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 4), mkNote(NoteName::E, 5, 4),
                    mkNote(NoteName::G, 5, 4), mkNote(NoteName::C, 5, 4) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(false));
    REQUIRE_FALSE(variants.empty());
    for (int i = 1; i < static_cast<int>(variants.size()); ++i)
        CHECK(variants[i].score <= variants[i-1].score);
}
