#include <catch2/catch_test_macros.hpp>
#include "harmonization/VariantScorer.h"
#include "domain/HarmonizationVariant.h"
#include "domain/HarmonizationJob.h"
#include "domain/Score.h"
#include "domain/Chord.h"
#include "domain/Note.h"
#include "domain/ChordTemplate.h"
#include <vector>

static Note makeNote(NoteName name, int octave, int alter = 0) {
    return Note(name, octave, alter, 1, 4, true);
}

static Chord makeChord(Note soprano, Note alto, Note tenor, Note bass) {
    ChordTemplate t{HarmonicFunction::T, 1, ChordType::Triad, Inversion::I,
                    ChordPosition::Close, {1, 3, 5, 1}};
    return Chord(soprano, alto, tenor, bass, t);
}

static HarmonizationVariant makeVariant(std::vector<Chord> chords, int scoreVal = 0) {
    HarmonizationVariant v;
    v.musicScore.chords = std::move(chords);
    v.score = scoreVal;
    return v;
}

static HarmonizationSettings makeSettings() {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural"};
    return s;
}

// Fixed voices that don't move between chords (no outer penalty in melody mode)
static const Note FIXED_S = makeNote(NoteName::C, 5);
static const Note FIXED_A = makeNote(NoteName::E, 4);
static const Note FIXED_T = makeNote(NoteName::G, 4);

TEST_CASE("applyFinalRanking sorts variants by score descending", "[VariantScorer]") {
    VariantScorer scorer;
    std::vector<HarmonizationVariant> variants;
    for (int s : {5, 3, 8, 1, 10}) {
        HarmonizationVariant v;
        v.score = s;
        variants.push_back(v);
    }
    scorer.applyFinalRanking(variants);
    REQUIRE(variants.size() == 5);
    REQUIRE(variants[0].score == 10);
    REQUIRE(variants[1].score == 8);
    REQUIRE(variants[2].score == 5);
    REQUIRE(variants[3].score == 3);
    REQUIRE(variants[4].score == 1);
}

TEST_CASE("applyFinalRanking trims to MAX_VARIANTS_TO_FRONTEND", "[VariantScorer]") {
    VariantScorer scorer;
    std::vector<HarmonizationVariant> variants;
    for (int i = 0; i < 35; ++i) {
        HarmonizationVariant v;
        v.score = i;
        variants.push_back(v);
    }
    scorer.applyFinalRanking(variants);
    REQUIRE(static_cast<int>(variants.size()) == MAX_VARIANTS_TO_FRONTEND);
    REQUIRE(static_cast<int>(variants.size()) == 30);
    REQUIRE(variants[0].score == 34);  // highest score first
}

TEST_CASE("applyFinalRanking returns all when fewer than 30 variants", "[VariantScorer]") {
    VariantScorer scorer;
    std::vector<HarmonizationVariant> variants;
    for (int i = 0; i < 10; ++i) {
        HarmonizationVariant v;
        v.score = i;
        variants.push_back(v);
    }
    scorer.applyFinalRanking(variants);
    REQUIRE(variants.size() == 10);
}

TEST_CASE("HarmonizeMelody: bass leap fourth gives penalty 1", "[VariantScorer]") {
    VariantScorer scorer;
    // Bass: C4 → F4 = perfect fourth
    Chord c1 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::C, 4));
    Chord c2 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::F, 4));
    auto variant = makeVariant({c1, c2});
    REQUIRE(scorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeMelody) == 1);
}

TEST_CASE("HarmonizeMelody: bass leap fifth gives penalty 2", "[VariantScorer]") {
    VariantScorer scorer;
    // Bass: C4 → G4 = perfect fifth
    Chord c1 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::C, 4));
    Chord c2 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::G, 4));
    auto variant = makeVariant({c1, c2});
    REQUIRE(scorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeMelody) == 2);
}

TEST_CASE("HarmonizeMelody: bass leap sixth gives penalty 3", "[VariantScorer]") {
    VariantScorer scorer;
    // Bass: C4 → A4 = major sixth
    Chord c1 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::C, 4));
    Chord c2 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::A, 4));
    auto variant = makeVariant({c1, c2});
    REQUIRE(scorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeMelody) == 3);
}

TEST_CASE("HarmonizeMelody: bass leap seventh gives penalty 5", "[VariantScorer]") {
    VariantScorer scorer;
    // Bass: C4 → B4 = major seventh
    Chord c1 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::C, 4));
    Chord c2 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::B, 4));
    auto variant = makeVariant({c1, c2});
    REQUIRE(scorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeMelody) == 5);
}

TEST_CASE("HarmonizeMelody: bass leap octave gives penalty 4", "[VariantScorer]") {
    VariantScorer scorer;
    // Bass: C4 → C5 = perfect octave
    Chord c1 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::C, 4));
    Chord c2 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::C, 5));
    auto variant = makeVariant({c1, c2});
    REQUIRE(scorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeMelody) == 4);
}

TEST_CASE("HarmonizeBass: soprano leap fifth gives penalty 2", "[VariantScorer]") {
    VariantScorer scorer;
    static const Note FIXED_B = makeNote(NoteName::C, 3);
    // Soprano: C5 → G5 = perfect fifth
    Chord c1 = makeChord(makeNote(NoteName::C, 5), FIXED_A, FIXED_T, FIXED_B);
    Chord c2 = makeChord(makeNote(NoteName::G, 5), FIXED_A, FIXED_T, FIXED_B);
    auto variant = makeVariant({c1, c2});
    REQUIRE(scorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeBass) == 2);
}

TEST_CASE("Alto augmented interval gives penalty 20", "[VariantScorer]") {
    VariantScorer scorer;
    // Alto: Eb4 → F#4 = augmented second
    Note eb4 = makeNote(NoteName::E, 4, -1);
    Note fs4 = makeNote(NoteName::F, 4,  1);
    Chord c1 = makeChord(FIXED_S, eb4, FIXED_T, makeNote(NoteName::C, 4));
    Chord c2 = makeChord(FIXED_S, fs4, FIXED_T, makeNote(NoteName::C, 4));
    auto variant = makeVariant({c1, c2});
    REQUIRE(scorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeMelody) == 20);
}

TEST_CASE("Tenor augmented interval gives penalty 20", "[VariantScorer]") {
    VariantScorer scorer;
    // Tenor: Eb4 → F#4 = augmented second
    Note eb4 = makeNote(NoteName::E, 4, -1);
    Note fs4 = makeNote(NoteName::F, 4,  1);
    Chord c1 = makeChord(FIXED_S, FIXED_A, eb4, makeNote(NoteName::C, 4));
    Chord c2 = makeChord(FIXED_S, FIXED_A, fs4, makeNote(NoteName::C, 4));
    auto variant = makeVariant({c1, c2});
    REQUIRE(scorer.calculateMelodicLinePenalty(variant, HarmonizationMode::HarmonizeMelody) == 20);
}

TEST_CASE("Existing chord-priority score is decremented by penalties only", "[VariantScorer]") {
    VariantScorer scorer;
    HarmonizationSettings settings = makeSettings();

    // Chord pair with no movement: all voices stay the same
    Chord c1 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::C, 4));
    Chord c2 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::C, 4));
    auto noPenaltyVariant = makeVariant({c1, c2});
    int baseScore = scorer.score(noPenaltyVariant, settings, HarmonizationMode::HarmonizeMelody);
    // smoothness(50) + functional(50) - penalty(0) = 100
    REQUIRE(baseScore == 100);

    // Chord pair where bass leaps a fifth (penalty 2)
    Chord c3 = makeChord(FIXED_S, FIXED_A, FIXED_T, makeNote(NoteName::G, 4));
    auto penaltyVariant = makeVariant({c1, c3});
    int penaltyScore = scorer.score(penaltyVariant, settings, HarmonizationMode::HarmonizeMelody);
    REQUIRE(penaltyScore == baseScore - 2);
    REQUIRE(penaltyScore == 98);
}
