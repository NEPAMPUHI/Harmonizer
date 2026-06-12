#include <catch2/catch_test_macros.hpp>
#include "harmonization/HarmonicRhythmPlanner.h"
#include "harmonization/MelodyHarmonizer.h"
#include "harmonization/BassHarmonizer.h"
#include "domain/HarmonizationVariant.h"
#include "domain/ScoreInput.h"
#include <unordered_set>
#include <string>

// ── Helpers ───────────────────────────────────────────────────────────────────

static HarmonizationSettings makeS(bool split = false, int maxPlans = 16) {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural", "harmonic", "melodic"};
    s.allowedChords = {"T53", "T6", "S53", "S6", "D53", "D6", "D7"};
    s.timeSignature.beats    = 4;
    s.timeSignature.beatType = 4;
    s.anacrusisSixteenths    = 0;
    s.measureCount           = 2;
    s.splitLongNotesByBasePulse = split;
    s.maxHarmonicRhythmPlans    = maxPlans;
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

// ── HarmonicRhythmPlan::priorityScore ────────────────────────────────────────

TEST_CASE("RhythmRanking: HarmonicRhythmPlan default priorityScore is 0", "[rhythm-ranking]") {
    HarmonicRhythmPlan plan;
    CHECK(plan.priorityScore == 0);
}

TEST_CASE("RhythmRanking: single plan (no split) has priorityScore 0", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ mkNote(NoteName::C, 5, 4) }, makeS(false));
    REQUIRE(plans.size() == 1);
    CHECK(plans[0].priorityScore == 0);
}

TEST_CASE("RhythmRanking: cadence split — plan[0] priorityScore == 0", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ mkNote(NoteName::C, 5, 16) }, makeS(true));
    REQUIRE(plans.size() >= 2);
    CHECK(plans[0].priorityScore == 0);
}

TEST_CASE("RhythmRanking: cadence split — plan[1] priorityScore == 10", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ mkNote(NoteName::C, 5, 16) }, makeS(true));
    REQUIRE(plans.size() >= 2);
    CHECK(plans[1].priorityScore == 10);
}

TEST_CASE("RhythmRanking: plan[0] priorityScore never exceeds plan[1]", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans({ mkNote(NoteName::C, 5, 16) }, makeS(true));
    REQUIRE(plans.size() >= 2);
    CHECK(plans[0].priorityScore <= plans[1].priorityScore);
}

TEST_CASE("RhythmRanking: plans sequence is non-decreasing by priorityScore", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    auto plans = planner.buildPlans(
        { mkNote(NoteName::C, 5, 16), mkNote(NoteName::E, 5, 16) }, makeS(true));
    REQUIRE(plans.size() > 1);
    for (int i = 1; i < static_cast<int>(plans.size()); ++i)
        CHECK(plans[i].priorityScore >= plans[i-1].priorityScore);
}

TEST_CASE("RhythmRanking: two branching spans accumulate priority correctly", "[rhythm-ranking]") {
    HarmonicRhythmPlanner planner;
    // Two whole notes in 4/4 at measure boundaries → 2×2 = 4 plans
    auto plans = planner.buildPlans(
        { mkNote(NoteName::C, 5, 16), mkNote(NoteName::E, 5, 16) }, makeS(true, 16));
    REQUIRE(plans.size() == 4);
    CHECK(plans[0].priorityScore ==  0);   // pattern[0] + pattern[0]
    CHECK(plans[1].priorityScore == 10);   // pattern[0] + pattern[1]
    CHECK(plans[2].priorityScore == 10);   // pattern[1] + pattern[0]
    CHECK(plans[3].priorityScore == 20);   // pattern[1] + pattern[1]
}

// ── HarmonizationVariant::rhythmPlanPriority ─────────────────────────────────

TEST_CASE("RhythmRanking: HarmonizationVariant default rhythmPlanPriority is 0", "[rhythm-ranking]") {
    HarmonizationVariant v;
    CHECK(v.rhythmPlanPriority == 0);
}

TEST_CASE("RhythmRanking: melody variants from plan[0] carry rhythmPlanPriority 0", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 16) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        if (v.rhythmPlanIndex == 0) CHECK(v.rhythmPlanPriority == 0);
}

TEST_CASE("RhythmRanking: melody variants from plan[1] carry rhythmPlanPriority 10", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 16) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        if (v.rhythmPlanIndex == 1) CHECK(v.rhythmPlanPriority == 10);
}

TEST_CASE("RhythmRanking: bass variants from plan[0] carry rhythmPlanPriority 0", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::G, 2, 16) };
    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());
    for (const auto& v : variants)
        if (v.rhythmPlanIndex == 0) CHECK(v.rhythmPlanPriority == 0);
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

TEST_CASE("RhythmRanking: within same rhythmPlanPriority score is non-increasing (melody)", "[rhythm-ranking]") {
    ScoreInput input;
    input.notes = { mkNote(NoteName::C, 5, 16) };
    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(input, makeS(true));
    REQUIRE_FALSE(variants.empty());

    int prevPriority = -1;
    int prevScore    = INT_MAX;
    for (const auto& v : variants) {
        if (v.rhythmPlanPriority != prevPriority) {
            prevPriority = v.rhythmPlanPriority;
            prevScore    = INT_MAX;
        }
        CHECK(v.score <= prevScore);
        prevScore = v.score;
    }
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
