#include <catch2/catch_test_macros.hpp>
#include "domain/HarmonyRules.h"
#include "domain/HarmonicPosition.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static Note n(NoteName name, int octave, int alter = 0) {
    return Note(name, octave, alter, 1, 4, false);
}

static HarmonicPosition makePos(bool strong, bool medium, bool weak) {
    HarmonicPosition pos;
    pos.index = 0;
    pos.measureIndex = 0;
    pos.startSixteenth = 0;
    pos.durationSixteenths = 4;
    pos.isStrongBeat = strong;
    pos.isMediumBeat = medium;
    pos.isWeakBeat = weak;
    return pos;
}

// T64 in C major: G4/E4/C4/G3 — passes isValidChord without position
static Chord makeSixFourChord() {
    static const ChordTemplate tmpl = {
        HarmonicFunction::T, 1, ChordType::SixFour, Inversion::V, ChordPosition::Close, {5, 3, 1, 5}
    };
    return Chord(n(NoteName::G, 4), n(NoteName::E, 4), n(NoteName::C, 4), n(NoteName::G, 3), tmpl);
}

// T53 in C major: E5/C5/E4/C3 — used as base chord for voice-leap tests.
// All voices within SATB ranges: S=E5[48-72], A=C5[43-62], T=E4[36-53], B=C3[28-48].
static const ChordTemplate T53_TMPL = {
    HarmonicFunction::T, 1, ChordType::Triad, Inversion::III, ChordPosition::Close, {3, 1, 3, 1}
};

static Chord makeCurrentChord() {
    return Chord(n(NoteName::E, 5), n(NoteName::C, 5), n(NoteName::E, 4), n(NoteName::C, 3), T53_TMPL);
}

// ── Rule 1: SixFour beat rule ─────────────────────────────────────────────────

TEST_CASE("HarmonyRules: Rejects SixFour on strong beat", "[HarmonyRules][SixFour]") {
    Chord chord = makeSixFourChord();
    REQUIRE(HarmonyRules::isValidChord(chord));

    HarmonicPosition pos = makePos(true, false, false);
    CHECK_FALSE(HarmonyRules::isValidChord(chord, pos));
}

TEST_CASE("HarmonyRules: Rejects SixFour on medium beat", "[HarmonyRules][SixFour]") {
    Chord chord = makeSixFourChord();
    REQUIRE(HarmonyRules::isValidChord(chord));

    HarmonicPosition pos = makePos(false, true, false);
    CHECK_FALSE(HarmonyRules::isValidChord(chord, pos));
}

TEST_CASE("HarmonyRules: Allows SixFour on weak beat", "[HarmonyRules][SixFour]") {
    Chord chord = makeSixFourChord();
    REQUIRE(HarmonyRules::isValidChord(chord));

    HarmonicPosition pos = makePos(false, false, true);
    CHECK(HarmonyRules::isValidChord(chord, pos));
}

// ── Rule 2: No voice leap greater than octave ─────────────────────────────────
// current chord: E5/C5/E4/C3 (T53, all voices in SATB ranges)
// previous chord: same except one voice is placed 2 octaves below — leap = 24 st.

TEST_CASE("HarmonyRules: Rejects soprano leap greater than octave", "[HarmonyRules][VoiceLeap]") {
    Chord curr = makeCurrentChord();
    REQUIRE(HarmonyRules::isValidChord(curr));

    // prev soprano = E3 (two octaves below E5): leap = 24 semitones
    Chord prev(n(NoteName::E, 3), n(NoteName::C, 5), n(NoteName::E, 4), n(NoteName::C, 3), T53_TMPL);
    CHECK_FALSE(HarmonyRules::isValidConnection(prev, curr));
}

TEST_CASE("HarmonyRules: Rejects alto leap greater than octave", "[HarmonyRules][VoiceLeap]") {
    Chord curr = makeCurrentChord();
    REQUIRE(HarmonyRules::isValidChord(curr));

    // prev alto = C3 (two octaves below C5): leap = 24 semitones
    Chord prev(n(NoteName::E, 5), n(NoteName::C, 3), n(NoteName::E, 4), n(NoteName::C, 3), T53_TMPL);
    CHECK_FALSE(HarmonyRules::isValidConnection(prev, curr));
}

TEST_CASE("HarmonyRules: Rejects tenor leap greater than octave", "[HarmonyRules][VoiceLeap]") {
    Chord curr = makeCurrentChord();
    REQUIRE(HarmonyRules::isValidChord(curr));

    // prev tenor = E2 (two octaves below E4): leap = 24 semitones
    Chord prev(n(NoteName::E, 5), n(NoteName::C, 5), n(NoteName::E, 2), n(NoteName::C, 3), T53_TMPL);
    CHECK_FALSE(HarmonyRules::isValidConnection(prev, curr));
}

TEST_CASE("HarmonyRules: Rejects bass leap greater than octave", "[HarmonyRules][VoiceLeap]") {
    Chord curr = makeCurrentChord();
    REQUIRE(HarmonyRules::isValidChord(curr));

    // prev bass = C1 (two octaves below C3): leap = 24 semitones
    Chord prev(n(NoteName::E, 5), n(NoteName::C, 5), n(NoteName::E, 4), n(NoteName::C, 1), T53_TMPL);
    CHECK_FALSE(HarmonyRules::isValidConnection(prev, curr));
}

TEST_CASE("HarmonyRules: Allows soprano leap exactly one octave", "[HarmonyRules][VoiceLeap]") {
    // curr: E5/E4/C4/C3 — S-A exactly one octave (at the boundary), all voices in range
    const Chord curr(n(NoteName::E, 5), n(NoteName::E, 4), n(NoteName::C, 4), n(NoteName::C, 3), T53_TMPL);
    REQUIRE(HarmonyRules::isValidChord(curr));

    // prev soprano = E4 (one octave below E5): leap = 12 semitones — exactly allowed.
    // prev alto = E4 so prev.soprano == curr.alto — no cross-chord voice overlap.
    const Chord prev(n(NoteName::E, 4), n(NoteName::E, 4), n(NoteName::C, 4), n(NoteName::C, 3), T53_TMPL);
    CHECK(HarmonyRules::isValidConnection(prev, curr));
}
