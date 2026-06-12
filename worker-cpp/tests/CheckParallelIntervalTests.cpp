#include <catch2/catch_test_macros.hpp>
#include "checking/CheckSolutionRuleChecker.h"
#include "checking/CheckHarmonicPosition.h"
#include "checking/IdentifiedCheckChord.h"
#include "domain/Chord.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static Note pn(NoteName n, int oct) {
    return Note(n, oct, 0, 0, 4, false);
}

static ChordTemplate anyTmpl() {
    ChordTemplate t;
    t.function           = HarmonicFunction::T;
    t.degree             = 1;
    t.type               = ChordType::Triad;
    t.inversion          = Inversion::I;
    t.position           = ChordPosition::Close;
    t.degreesInSatbOrder = {1, 3, 5, 1};
    return t;
}

static IdentifiedCheckChord knownIC(const Note& s, const Note& a,
                                     const Note& t, const Note& b) {
    static const ChordTemplate tmpl = anyTmpl();
    IdentifiedCheckChord ic;
    ic.isKnownChord    = true;
    ic.chord           = Chord(s, a, t, b, tmpl);
    ic.matchedTemplate = tmpl;
    ic.position.soprano = s; ic.position.hasSoprano = true;
    ic.position.alto    = a; ic.position.hasAlto    = true;
    ic.position.tenor   = t; ic.position.hasTenor   = true;
    ic.position.bass    = b; ic.position.hasBass    = true;
    ic.position.measureIndex               = 1;
    ic.position.positionInMeasureSixteenths = 0;
    ic.position.durationSixteenths         = 4;
    return ic;
}

static IdentifiedCheckChord unknownIC(const Note& s, const Note& a,
                                       const Note& t, const Note& b) {
    auto ic = knownIC(s, a, t, b);
    ic.isKnownChord = false;
    return ic;
}

// ── Test 1: parallel fifths between tenor and bass ───────────────────────────
//
// ic1: S=E5(64) A=A4(57) T=G3(43) B=C3(36)  T-B = 7 semitones (P5)
// ic2: S=F5(65) A=B4(59) T=A3(45) B=D3(38)  T-B = 7 semitones (P5)
// Both move up by a step. S-A changes from P5→dim5 (not parallel). No other P5/P8.

TEST_CASE("CheckParallelIntervals: parallel perfect fifths bass-tenor → 1 error",
          "[parallel]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::E, 5), pn(NoteName::A, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));
    const auto ic2 = knownIC(pn(NoteName::F, 5), pn(NoteName::B, 4),
                              pn(NoteName::A, 3), pn(NoteName::D, 3));

    const auto errors = checker.check({ic1, ic2});

    // Filter to ParallelFifths only; AllVoicesSameDirection may also fire for
    // this pair (all four voices ascend) and that is correct behaviour.
    std::vector<CheckError> fifthErrors;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::ParallelFifths) fifthErrors.push_back(e);

    REQUIRE(fifthErrors.size() == 1);
    CHECK(fifthErrors[0].interval          == 5);
    CHECK(fifthErrors[0].positionIndex     == 0);
    CHECK(fifthErrors[0].nextPositionIndex == 1);
    CHECK(fifthErrors[0].renderType        == CheckRenderType::VerticalBracketPair);

    REQUIRE(fifthErrors[0].voices.size() == 2);
    CHECK(fifthErrors[0].voices[0] == VoiceType::Tenor);
    CHECK(fifthErrors[0].voices[1] == VoiceType::Bass);
}

// ── Test 2: parallel octaves between soprano and bass ────────────────────────
//
// ic1: S=C5(60) A=E4(52) T=G3(43-static) B=C3(36)  S-B = 24 semitones (octave)
// ic2: S=D5(62) A=F4(53) T=G3(43-static) B=D3(38)  S-B = 24 semitones (octave)
// Tenor stands still → T-B changes from P5(7) to P4(5). No other P5/P8.

TEST_CASE("CheckParallelIntervals: parallel perfect octaves soprano-bass → 1 error",
          "[parallel]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::C, 5), pn(NoteName::E, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));
    const auto ic2 = knownIC(pn(NoteName::D, 5), pn(NoteName::F, 4),
                              pn(NoteName::G, 3), pn(NoteName::D, 3));

    const auto errors = checker.check({ic1, ic2});

    // This S-B pair also triggers HiddenOctaves and ParallelOctavesOrUnisons.
    // Filter to the legacy semitone-based ParallelOctaves check.
    std::vector<CheckError> octErrors;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::ParallelOctaves) octErrors.push_back(e);

    REQUIRE(octErrors.size() == 1);
    CHECK(octErrors[0].interval          == 8);
    CHECK(octErrors[0].positionIndex     == 0);
    CHECK(octErrors[0].nextPositionIndex == 1);
    CHECK(octErrors[0].renderType        == CheckRenderType::VerticalBracketPair);

    REQUIRE(octErrors[0].voices.size() == 2);
    CHECK(octErrors[0].voices[0] == VoiceType::Soprano);
    CHECK(octErrors[0].voices[1] == VoiceType::Bass);
}

// ── Test 3: contrary motion → no parallel error ───────────────────────────────
//
// Soprano descends C5→B4, bass ascends C3→B3: both are octaves but contrary.
// Alto and tenor stand still → no movement involved.

TEST_CASE("CheckParallelIntervals: contrary motion in octave-pair → 0 errors",
          "[parallel]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::C, 5), pn(NoteName::E, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));
    // soprano goes DOWN, bass goes UP
    const auto ic2 = knownIC(pn(NoteName::B, 4), pn(NoteName::E, 4),
                              pn(NoteName::G, 3), pn(NoteName::B, 3));

    const auto errors = checker.check({ic1, ic2});
    CHECK(errors.empty());
}

// ── Test 4: one voice stands still → no parallel error ───────────────────────
//
// Bass stays on C3 in both chords; soprano ascends → S-B interval changes.
// T-B would have P5 in both but tenor also stands still.

TEST_CASE("CheckParallelIntervals: standing voice prevents parallel detection → 0 errors",
          "[parallel]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::C, 5), pn(NoteName::E, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));
    // bass and tenor stand still
    const auto ic2 = knownIC(pn(NoteName::D, 5), pn(NoteName::F, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));

    const auto errors = checker.check({ic1, ic2});
    CHECK(errors.empty());
}

// ── Test 5: same direction but non-P5/P8 interval → no error ─────────────────
//
// All four voices ascend by a step; all pairwise intervals are seconds or
// thirds — no perfect fifth or octave in either chord.

TEST_CASE("CheckParallelIntervals: non-perfect-fifth/octave parallel motion → 0 errors",
          "[parallel]") {
    CheckSolutionRuleChecker checker;

    // S=C5(60) A=B4(59) T=A4(57) B=G4(55) — tightly packed, no P5 or P8
    const auto ic1 = knownIC(pn(NoteName::C, 5), pn(NoteName::B, 4),
                              pn(NoteName::A, 4), pn(NoteName::G, 4));
    // S=D5(62) A=C5(60) T=B4(59) B=A4(57)
    const auto ic2 = knownIC(pn(NoteName::D, 5), pn(NoteName::C, 5),
                              pn(NoteName::B, 4), pn(NoteName::A, 4));

    const auto errors = checker.check({ic1, ic2});
    // All voices move in the same direction, so AllVoicesSameDirection may fire.
    // The point of this test is that no ParallelFifths or ParallelOctaves fires.
    for (const auto& e : errors) {
        CHECK(e.code != CheckErrorCode::ParallelFifths);
        CHECK(e.code != CheckErrorCode::ParallelOctaves);
    }
}

// ── Test 6: unknown chord in pair → voice-leading check still runs ────────────
//
// ic1 is known, ic2 is unknown with pitches that produce ParallelFifths (T-B P5).
// After the fix, pairwise checks run regardless of isKnownChord, so both
// UnknownChord and ParallelFifths are emitted.

TEST_CASE("CheckParallelIntervals: unknown chord in pair does not skip voice-leading check",
          "[parallel]") {
    CheckSolutionRuleChecker checker;

    // Same pitches as Test 1 (T-B: G3→A3 and C3→D3 both P5 moving same direction)
    const auto ic1 = knownIC( pn(NoteName::E, 5), pn(NoteName::A, 4),
                               pn(NoteName::G, 3), pn(NoteName::C, 3));
    const auto ic2 = unknownIC(pn(NoteName::F, 5), pn(NoteName::B, 4),
                                pn(NoteName::A, 3), pn(NoteName::D, 3));

    const auto errors = checker.check({ic1, ic2});

    bool hasUnknown = false, hasParallelFifths = false;
    for (const auto& e : errors) {
        if (e.code == CheckErrorCode::UnknownChord)    hasUnknown       = true;
        if (e.code == CheckErrorCode::ParallelFifths)  hasParallelFifths = true;
    }
    CHECK(hasUnknown);
    CHECK(hasParallelFifths);
}

// ── Test 7: three consecutive chords → 2 parallel-fifth errors ───────────────
//
// ic1: T=G3(43) B=C3(36)  → P5
// ic2: T=A3(45) B=D3(38)  → P5   pair (0,1) fires
// ic3: T=B3(47) B=E3(40)  → P5   pair (1,2) fires

TEST_CASE("CheckParallelIntervals: three consecutive chords with T-B P5 → 2 errors",
          "[parallel]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::E, 5), pn(NoteName::A, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));
    const auto ic2 = knownIC(pn(NoteName::F, 5), pn(NoteName::B, 4),
                              pn(NoteName::A, 3), pn(NoteName::D, 3));
    const auto ic3 = knownIC(pn(NoteName::G, 5), pn(NoteName::C, 5),
                              pn(NoteName::B, 3), pn(NoteName::E, 3));

    const auto errors = checker.check({ic1, ic2, ic3});

    // Filter to just ParallelFifths to avoid dependence on unrelated errors
    std::vector<CheckError> fifthErrors;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::ParallelFifths) fifthErrors.push_back(e);

    REQUIRE(fifthErrors.size() == 2);

    CHECK(fifthErrors[0].positionIndex     == 0);
    CHECK(fifthErrors[0].nextPositionIndex == 1);
    CHECK(fifthErrors[1].positionIndex     == 1);
    CHECK(fifthErrors[1].nextPositionIndex == 2);

    for (const auto& e : fifthErrors) {
        REQUIRE(e.voices.size() == 2);
        CHECK(e.voices[0] == VoiceType::Tenor);
        CHECK(e.voices[1] == VoiceType::Bass);
        CHECK(e.interval  == 5);
    }
}
