#include <catch2/catch_test_macros.hpp>
#include "checking/CheckSolutionRuleChecker.h"
#include "checking/CheckHarmonicPosition.h"
#include "checking/IdentifiedCheckChord.h"
#include "domain/Chord.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static Note avsd_pn(NoteName n, int oct) {
    return Note(n, oct, 0, 0, 4, false);
}

static Note avsd_rest() {
    return Note(NoteName::C, 5, 0, 0, 4, true);
}

static ChordTemplate avsd_tmpl() {
    ChordTemplate t;
    t.function           = HarmonicFunction::T;
    t.degree             = 1;
    t.type               = ChordType::Triad;
    t.inversion          = Inversion::I;
    t.position           = ChordPosition::Close;
    t.degreesInSatbOrder = {1, 3, 5, 1};
    return t;
}

static IdentifiedCheckChord avsd_knownIC(const Note& s, const Note& a,
                                          const Note& t, const Note& b) {
    static const ChordTemplate tmpl = avsd_tmpl();
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

static IdentifiedCheckChord avsd_unknownIC(const Note& s, const Note& a,
                                            const Note& t, const Note& b) {
    auto ic = avsd_knownIC(s, a, t, b);
    ic.isKnownChord = false;
    return ic;
}

// ic with soprano absent (hasSoprano = false) — exercises the "absent voice" skip path.
static IdentifiedCheckChord avsd_knownIC_noSoprano(const Note& a, const Note& t,
                                                     const Note& b) {
    static const ChordTemplate tmpl = avsd_tmpl();
    Note dummyS = avsd_pn(NoteName::C, 5);
    IdentifiedCheckChord ic;
    ic.isKnownChord    = true;
    ic.chord           = Chord(dummyS, a, t, b, tmpl);
    ic.matchedTemplate = tmpl;
    ic.position.soprano = dummyS; ic.position.hasSoprano = false; // absent
    ic.position.alto    = a;      ic.position.hasAlto    = true;
    ic.position.tenor   = t;      ic.position.hasTenor   = true;
    ic.position.bass    = b;      ic.position.hasBass    = true;
    ic.position.measureIndex               = 1;
    ic.position.positionInMeasureSixteenths = 0;
    ic.position.durationSixteenths         = 4;
    return ic;
}

// ── chord pair used across several tests ──────────────────────────────────────
//
// SOPRANO_RANGE C4..C6   ALTO_RANGE G3..D5   TENOR_RANGE C3..F4   BASS_RANGE E2..C4
//
// ic_lo: S=D5(62) A=A4(57) T=D4(50) B=F3(41)
// ic_hi: S=E5(64) A=B4(59) T=F4(53) B=G3(43)
//
// All four voices move upward from ic_lo → ic_hi. No note is outside its SATB
// range. No adjacent-voice gap exceeds an octave. No parallel P5/P8 interval,
// no hidden fifths/octaves, no chromatic transfers — so the only diagnostic
// that fires for this pair is AllVoicesSameDirection.

static IdentifiedCheckChord ic_lo() {
    return avsd_knownIC(avsd_pn(NoteName::D, 5), avsd_pn(NoteName::A, 4),
                        avsd_pn(NoteName::D, 4), avsd_pn(NoteName::F, 3));
}
static IdentifiedCheckChord ic_hi() {
    return avsd_knownIC(avsd_pn(NoteName::E, 5), avsd_pn(NoteName::B, 4),
                        avsd_pn(NoteName::F, 4), avsd_pn(NoteName::G, 3));
}

// ── Test 1: all 4 voices move up → 1 AllVoicesSameDirection error ─────────────

TEST_CASE("AllVoicesSameDirection: all voices ascend → 1 error", "[avsd]") {
    CheckSolutionRuleChecker checker;
    const auto errors = checker.check({ic_lo(), ic_hi()});

    REQUIRE(errors.size() == 1);
    CHECK(errors[0].code              == CheckErrorCode::AllVoicesSameDirection);
    CHECK(errors[0].positionIndex     == 0);
    CHECK(errors[0].nextPositionIndex == 1);
    CHECK(errors[0].renderType        == CheckRenderType::MotionLines);
    CHECK(errors[0].interval          == 0);

    REQUIRE(errors[0].voices.size() == 4);
    CHECK(errors[0].voices[0] == VoiceType::Soprano);
    CHECK(errors[0].voices[1] == VoiceType::Alto);
    CHECK(errors[0].voices[2] == VoiceType::Tenor);
    CHECK(errors[0].voices[3] == VoiceType::Bass);
}

// ── Test 2: all 4 voices move down → 1 error ──────────────────────────────────

TEST_CASE("AllVoicesSameDirection: all voices descend → 1 error", "[avsd]") {
    CheckSolutionRuleChecker checker;
    // Reversed: hi → lo, so all voices move down.
    const auto errors = checker.check({ic_hi(), ic_lo()});

    REQUIRE(errors.size() == 1);
    CHECK(errors[0].code              == CheckErrorCode::AllVoicesSameDirection);
    CHECK(errors[0].positionIndex     == 0);
    CHECK(errors[0].nextPositionIndex == 1);
    CHECK(errors[0].renderType        == CheckRenderType::MotionLines);
}

// ── Test 3: three voices up, bass down → no error ─────────────────────────────

TEST_CASE("AllVoicesSameDirection: 3 up 1 down → 0 errors", "[avsd]") {
    CheckSolutionRuleChecker checker;

    // S, A, T ascend; B descends (from F4→E4 = down).
    const auto from = avsd_knownIC(avsd_pn(NoteName::E, 5), avsd_pn(NoteName::C, 5),
                                   avsd_pn(NoteName::A, 4), avsd_pn(NoteName::F, 4));
    const auto to   = avsd_knownIC(avsd_pn(NoteName::G, 5), avsd_pn(NoteName::D, 5),
                                   avsd_pn(NoteName::B, 4), avsd_pn(NoteName::E, 4));

    const auto errors = checker.check({from, to});
    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::AllVoicesSameDirection);
}

// ── Test 4: three voices up, bass stands still → no error ─────────────────────

TEST_CASE("AllVoicesSameDirection: 3 up 1 still → 0 errors", "[avsd]") {
    CheckSolutionRuleChecker checker;

    // S, A, T ascend; B stays on E4.
    const auto from = avsd_knownIC(avsd_pn(NoteName::E, 5), avsd_pn(NoteName::C, 5),
                                   avsd_pn(NoteName::A, 4), avsd_pn(NoteName::E, 4));
    const auto to   = avsd_knownIC(avsd_pn(NoteName::G, 5), avsd_pn(NoteName::D, 5),
                                   avsd_pn(NoteName::B, 4), avsd_pn(NoteName::E, 4));

    const auto errors = checker.check({from, to});
    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::AllVoicesSameDirection);
}

// ── Test 5: unknown chord in pair → pairwise check still runs ─────────────────
//
// After the fix, pairwise checks (including AllVoicesSameDirection) run even
// when one chord in the pair is unknown. Both UnknownChord and
// AllVoicesSameDirection must be present in the result.

TEST_CASE("AllVoicesSameDirection: unknown chord does not skip pairwise check", "[avsd]") {
    CheckSolutionRuleChecker checker;

    // ic2 is the same pitches as ic_hi but marked unknown — all 4 voices ascend.
    const auto ic1 = ic_lo();
    const auto ic2 = avsd_unknownIC(avsd_pn(NoteName::G, 5), avsd_pn(NoteName::D, 5),
                                    avsd_pn(NoteName::B, 4), avsd_pn(NoteName::F, 4));

    const auto errors = checker.check({ic1, ic2});

    bool hasUnknown = false, hasAVSD = false;
    for (const auto& e : errors) {
        if (e.code == CheckErrorCode::UnknownChord)          hasUnknown = true;
        if (e.code == CheckErrorCode::AllVoicesSameDirection) hasAVSD    = true;
    }
    CHECK(hasUnknown);
    CHECK(hasAVSD);
}

// ── Test 6: absent voice in position → pair skipped ───────────────────────────

TEST_CASE("AllVoicesSameDirection: absent voice skips pair", "[avsd]") {
    CheckSolutionRuleChecker checker;

    // ic1 has no soprano voice (hasSoprano = false).
    const auto ic1 = avsd_knownIC_noSoprano(avsd_pn(NoteName::C, 5),
                                             avsd_pn(NoteName::A, 4),
                                             avsd_pn(NoteName::E, 4));
    const auto ic2 = ic_hi();

    const auto errors = checker.check({ic1, ic2});

    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::AllVoicesSameDirection);
}

// ── Test 7: coexists with parallel-interval errors ────────────────────────────
//
// ic1: S=E5(64) A=A4(57) T=G3(43) B=C3(36)
// ic2: S=F5(65) A=B4(59) T=A3(45) B=D3(38)
//
// T-B moves from P5 → P5 in the same direction → ParallelFifths.
// All four voices also move upward → AllVoicesSameDirection.
// The checker must not stop after the first error.

TEST_CASE("AllVoicesSameDirection: coexists with ParallelFifths", "[avsd]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = avsd_knownIC(avsd_pn(NoteName::E, 5), avsd_pn(NoteName::A, 4),
                                   avsd_pn(NoteName::G, 3), avsd_pn(NoteName::C, 3));
    const auto ic2 = avsd_knownIC(avsd_pn(NoteName::F, 5), avsd_pn(NoteName::B, 4),
                                   avsd_pn(NoteName::A, 3), avsd_pn(NoteName::D, 3));

    const auto errors = checker.check({ic1, ic2});

    bool hasParallel = false;
    bool hasAvsd     = false;
    for (const auto& e : errors) {
        if (e.code == CheckErrorCode::ParallelFifths)       hasParallel = true;
        if (e.code == CheckErrorCode::AllVoicesSameDirection) hasAvsd   = true;
    }

    CHECK(hasParallel);
    CHECK(hasAvsd);
    REQUIRE(errors.size() >= 2);
}
