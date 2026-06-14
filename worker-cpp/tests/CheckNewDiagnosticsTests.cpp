#include <catch2/catch_test_macros.hpp>
#include "checking/CheckSolutionRuleChecker.h"
#include "checking/CheckHarmonicPosition.h"
#include "checking/IdentifiedCheckChord.h"
#include "domain/Chord.h"
#include "domain/HarmonyRules.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static Note nd_pn(NoteName n, int oct, int alter = 0) {
    return Note(n, oct, alter, 0, 4, false);
}

static ChordTemplate nd_tmpl(HarmonicFunction fn = HarmonicFunction::T,
                              int degree          = 1,
                              ChordType type      = ChordType::Triad) {
    ChordTemplate t;
    t.function           = fn;
    t.degree             = degree;
    t.type               = type;
    t.inversion          = Inversion::I;
    t.position           = ChordPosition::Close;
    t.degreesInSatbOrder = {1, 3, 5, 1};
    return t;
}

static IdentifiedCheckChord nd_knownIC(const Note& s, const Note& a,
                                        const Note& t, const Note& b,
                                        HarmonicFunction fn = HarmonicFunction::T,
                                        int degree          = 1,
                                        ChordType type      = ChordType::Triad) {
    ChordTemplate tmpl = nd_tmpl(fn, degree, type);
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

// Convenience: a "clean" known IC using notes well within SATB ranges.
// S=E5(64) A=C5(60) T=A4(57) B=E4(52)  — no voice-leading errors between copies.
static IdentifiedCheckChord nd_cleanIC() {
    return nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                      nd_pn(NoteName::A, 4), nd_pn(NoteName::E, 4));
}

// ── VoiceRangeViolation ───────────────────────────────────────────────────────
// BASS_RANGE is E2..C4. D2 is below the range.

TEST_CASE("Diag: VoiceRangeViolation — bass below range", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // D2 = semitone 26 < E2 = 28 → out of BASS_RANGE
    auto ic = nd_knownIC(nd_pn(NoteName::E, 4), nd_pn(NoteName::C, 4),
                         nd_pn(NoteName::G, 3), nd_pn(NoteName::D, 2));

    const auto errors = checker.check({ic});
    bool found = false;
    for (const auto& e : errors) {
        if (e.code == CheckErrorCode::VoiceRangeViolation
            && !e.voices.empty() && e.voices[0] == VoiceType::Bass)
            found = true;
    }
    CHECK(found);
}

// SOPRANO_RANGE is C4..C6. D6 is above the range.

TEST_CASE("Diag: VoiceRangeViolation — soprano above range", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // D6 = semitone 86 > C6 = 84
    auto ic = nd_knownIC(nd_pn(NoteName::D, 6), nd_pn(NoteName::C, 5),
                         nd_pn(NoteName::A, 4), nd_pn(NoteName::E, 3));

    const auto errors = checker.check({ic});
    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::VoiceRangeViolation
            && !e.voices.empty() && e.voices[0] == VoiceType::Soprano)
            found = true;
    CHECK(found);
}

// All voices within range → no VoiceRangeViolation.

TEST_CASE("Diag: VoiceRangeViolation — all in range → 0 violations", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // SOPRANO_RANGE: C4..C6   ALTO_RANGE: G3..D5
    // TENOR_RANGE:  C3..F4   BASS_RANGE:  E2..C4
    auto ic = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                         nd_pn(NoteName::F, 4), nd_pn(NoteName::C, 3));

    const auto errors = checker.check({ic});
    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::VoiceRangeViolation);
}

// ── MoreThanOctaveBetweenAdjacentVoices ──────────────────────────────────────
// S-A gap: getInterval(S,A).number > 8
// Use S=C5(60), A=B3(47): C5→B3 is more than 9 diatonic notes apart (>8).

TEST_CASE("Diag: MoreThanOctaveBetweenAdjacentVoices — S-A", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // S=C5, A=B3: diatonic number C5→B3 > 8 (10th roughly)
    auto ic = nd_knownIC(nd_pn(NoteName::C, 5), nd_pn(NoteName::B, 3),
                         nd_pn(NoteName::G, 3), nd_pn(NoteName::C, 3));

    const auto errors = checker.check({ic});
    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::MoreThanOctaveBetweenAdjacentVoices
            && e.voices.size() == 2
            && e.voices[0] == VoiceType::Soprano
            && e.voices[1] == VoiceType::Alto)
            found = true;
    CHECK(found);
}

// ── VoiceCrossing ─────────────────────────────────────────────────────────────
// curr.soprano < prev.alto → S-A crossing.

TEST_CASE("Diag: VoiceCrossing — soprano crosses below previous alto", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // prev: S=E5, A=C5;  curr: S=B4 (< prev.A=C5) → crossing
    auto ic1 = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::A, 4), nd_pn(NoteName::E, 4));
    auto ic2 = nd_knownIC(nd_pn(NoteName::B, 4), nd_pn(NoteName::D, 5),
                           nd_pn(NoteName::B, 4), nd_pn(NoteName::G, 4));

    const auto errors = checker.check({ic1, ic2});
    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::VoiceCrossing
            && e.voices.size() == 2
            && e.voices[0] == VoiceType::Soprano
            && e.voices[1] == VoiceType::Alto)
            found = true;
    CHECK(found);
}

// ── ChromaticSemitoneTransfer ─────────────────────────────────────────────────
// prev.soprano = F4, curr.alto = F#4 → same name F, different alter.

TEST_CASE("Diag: ChromaticSemitoneTransfer — F natural to F sharp across voices",
          "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // ic1 soprano = F4 (alter=0), ic2 alto = F#4 (alter=1): chromatic transfer
    auto ic1 = nd_knownIC(nd_pn(NoteName::F, 4, 0), nd_pn(NoteName::C, 4, 0),
                           nd_pn(NoteName::A, 3, 0), nd_pn(NoteName::F, 3, 0));
    auto ic2 = nd_knownIC(nd_pn(NoteName::A, 4, 0), nd_pn(NoteName::F, 4, 1),  // F#
                           nd_pn(NoteName::C, 4, 0), nd_pn(NoteName::F, 3, 0));

    const auto errors = checker.check({ic1, ic2});
    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::ChromaticSemitoneTransfer) {
            found = true;
            CHECK(e.voices.size() == 2);
        }
    CHECK(found);
}

// ── HiddenOctaves ─────────────────────────────────────────────────────────────
// curr.bass.getSimpleInterval(curr.soprano).number == 1  AND  same direction.
// C3→C5: simple interval = 1 (2 octaves reduced).

TEST_CASE("Diag: HiddenOctaves — bass and soprano reach octave in same direction",
          "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // prev: S=B4(59), B=B2(35);  curr: S=C5(60), B=C3(36)
    // Both S and B move up. Curr S-B = C5-C3: diatonic 1 after simplification.
    auto ic1 = nd_knownIC(nd_pn(NoteName::B, 4), nd_pn(NoteName::G, 4),
                           nd_pn(NoteName::D, 4), nd_pn(NoteName::B, 2));
    auto ic2 = nd_knownIC(nd_pn(NoteName::C, 5), nd_pn(NoteName::E, 4),
                           nd_pn(NoteName::G, 3), nd_pn(NoteName::C, 3));

    const auto errors = checker.check({ic1, ic2});
    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::HiddenOctaves
            && e.interval == 8
            && e.voices.size() == 2
            && e.voices[0] == VoiceType::Soprano
            && e.voices[1] == VoiceType::Bass)
            found = true;
    CHECK(found);
}

// ── HiddenFifths ─────────────────────────────────────────────────────────────
// curr.bass.getSimpleInterval(curr.soprano).number == 5  AND  same direction.
// C4 → G5: C-G = 5th; simple interval = 5.

TEST_CASE("Diag: HiddenFifths — bass and soprano reach fifth in same direction",
          "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // prev: S=D5, B=G3; curr: S=G5, B=C4
    // S moves up D5→G5; B moves up G3→C4.
    // Curr S=G5(67), B=C4(48): G5-C4 = C4 to G5 = 12th. Simple: 12-7=5. ✓
    auto ic1 = nd_knownIC(nd_pn(NoteName::D, 5), nd_pn(NoteName::A, 4),
                           nd_pn(NoteName::F, 4), nd_pn(NoteName::G, 3));
    auto ic2 = nd_knownIC(nd_pn(NoteName::G, 5), nd_pn(NoteName::B, 4),
                           nd_pn(NoteName::E, 4), nd_pn(NoteName::C, 4));

    const auto errors = checker.check({ic1, ic2});
    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::HiddenFifths
            && e.interval == 5
            && e.voices.size() == 2
            && e.voices[0] == VoiceType::Soprano
            && e.voices[1] == VoiceType::Bass)
            found = true;
    CHECK(found);
}

// Contrary motion → no hidden fifths.

TEST_CASE("Diag: HiddenFifths — contrary motion → no error", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // S goes up, B goes down → not same direction.
    auto ic1 = nd_knownIC(nd_pn(NoteName::D, 5), nd_pn(NoteName::A, 4),
                           nd_pn(NoteName::F, 4), nd_pn(NoteName::C, 4));
    auto ic2 = nd_knownIC(nd_pn(NoteName::G, 5), nd_pn(NoteName::B, 4),
                           nd_pn(NoteName::E, 4), nd_pn(NoteName::G, 3));

    const auto errors = checker.check({ic1, ic2});
    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::HiddenFifths);
}

// ── VoiceLeapGreaterThanOctave ────────────────────────────────────────────────
// Bass leaps from E2(28) to C4(48): diff = 20 > 12 semitones.

TEST_CASE("Diag: VoiceLeapGreaterThanOctave — bass leap > octave", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    auto ic1 = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::A, 4), nd_pn(NoteName::E, 2));
    auto ic2 = nd_knownIC(nd_pn(NoteName::F, 5), nd_pn(NoteName::D, 5),
                           nd_pn(NoteName::B, 4), nd_pn(NoteName::C, 4));

    const auto errors = checker.check({ic1, ic2});
    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::VoiceLeapGreaterThanOctave
            && !e.voices.empty() && e.voices[0] == VoiceType::Bass)
            found = true;
    CHECK(found);
}

// A leap of exactly an octave (12 semitones) is allowed.

TEST_CASE("Diag: VoiceLeapGreaterThanOctave — exactly octave is OK", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // E3(40) → E4(52): diff = 12, not > 12 → no error
    auto ic1 = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::A, 4), nd_pn(NoteName::E, 3));
    auto ic2 = nd_knownIC(nd_pn(NoteName::F, 5), nd_pn(NoteName::D, 5),
                           nd_pn(NoteName::B, 4), nd_pn(NoteName::E, 4));

    const auto errors = checker.check({ic1, ic2});
    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::VoiceLeapGreaterThanOctave);
}

// ── FunctionalProgressionError ────────────────────────────────────────────────
// T → T → T: always valid functionally.

TEST_CASE("Diag: FunctionalProgressionError — T to T is valid", "[new_diag]") {
    CheckSolutionRuleChecker checker;

    auto ic1 = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::A, 4), nd_pn(NoteName::C, 4),
                           HarmonicFunction::T, 1, ChordType::Triad);
    auto ic2 = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::A, 4), nd_pn(NoteName::C, 4),
                           HarmonicFunction::T, 1, ChordType::Triad);

    const auto errors = checker.check({ic1, ic2});
    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::FunctionalProgressionError);
}

// ── ConsecutiveFourthsInBass ──────────────────────────────────────────────────
// Three chords where bass moves by a 4th each time, strictly ascending.

TEST_CASE("Diag: ConsecutiveFourthsInBass — strictly ascending fourths in bass",
          "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // Bass: C3(36) → F3(41) → Bb3(46), two ascending fourths.
    auto ic1 = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::G, 4), nd_pn(NoteName::C, 3));
    auto ic2 = nd_knownIC(nd_pn(NoteName::F, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::A, 4), nd_pn(NoteName::F, 3));
    // Bb3: B with alter=-1
    auto ic3 = nd_knownIC(nd_pn(NoteName::D, 5), nd_pn(NoteName::B, 4, -1),
                           nd_pn(NoteName::F, 4), nd_pn(NoteName::B, 3, -1));

    const auto errors = checker.check({ic1, ic2, ic3});
    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::ConsecutiveFourthsInBass
            && !e.voices.empty() && e.voices[0] == VoiceType::Bass
            && e.positionIndex == 0 && e.nextPositionIndex == 2)
            found = true;
    CHECK(found);
}

// Non-consecutive (bass: 4th then 5th) → no ConsecutiveFourthsInBass.

TEST_CASE("Diag: ConsecutiveFourthsInBass — mixed intervals → no error",
          "[new_diag]") {
    CheckSolutionRuleChecker checker;

    // Bass: C3 → F3 (4th) → C4 (5th): not two consecutive 4ths.
    auto ic1 = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::G, 4), nd_pn(NoteName::C, 3));
    auto ic2 = nd_knownIC(nd_pn(NoteName::F, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::A, 4), nd_pn(NoteName::F, 3));
    auto ic3 = nd_knownIC(nd_pn(NoteName::E, 5), nd_pn(NoteName::C, 5),
                           nd_pn(NoteName::G, 4), nd_pn(NoteName::C, 4));

    const auto errors = checker.check({ic1, ic2, ic3});
    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::ConsecutiveFourthsInBass);
}
