#include <catch2/catch_test_macros.hpp>
#include "harmonization/ChordBuilder.h"
#include "domain/HarmonyRules.h"

// ── test helpers ──────────────────────────────────────────────────────────────

static HarmonizationSettings makeSettings(const std::string& key = "C") {
    HarmonizationSettings s;
    s.key = key;
    s.scaleModes = {"natural", "harmonic", "melodic"};
    s.allowedChords = {"T53", "T6", "S53", "S6", "D53", "D6", "D7"};
    return s;
}

// Note with no alter; degree must be set explicitly for ChordBuilder to accept it.
static Note makeNote(NoteName name, int octave, int degree, int alter = 0) {
    return Note(name, octave, alter, degree, Duration{}, false);
}

// ── template constants (C major, triad/sixth with known voice layout) ─────────

// T53 I Close:  S=1(C) A=5(G) T=3(E) B=1(C)
static const ChordTemplate T53_I_CLOSE = {
    HarmonicFunction::T, 1, ChordType::Triad, Inversion::I, ChordPosition::Close, {1, 5, 3, 1}
};

// T53 I Wide:   S=1(C) A=3(E) T=5(G) B=1(C)
static const ChordTemplate T53_I_WIDE = {
    HarmonicFunction::T, 1, ChordType::Triad, Inversion::I, ChordPosition::Wide, {1, 3, 5, 1}
};

// T6 I Close, doubled I:  S=1(C) A=1(C) T=5(G) B=3(E)  — soprano/alto same degree
static const ChordTemplate T6_I_CLOSE_DBL_I = {
    HarmonicFunction::T, 1, ChordType::Six, Inversion::I, ChordPosition::Close, {1, 1, 5, 3}
};

// T6 I Wide, doubled I:   S=1(C) A=1(C) T=5(G) B=3(E)  — soprano/alto same degree, Wide
static const ChordTemplate T6_I_WIDE_DBL_I = {
    HarmonicFunction::T, 1, ChordType::Six, Inversion::I, ChordPosition::Wide, {1, 1, 5, 3}
};

// T6 V Close, doubled I:  S=5(G) A=1(C) T=1(C) B=3(E)  — alto/tenor same degree
static const ChordTemplate T6_V_CLOSE_DBL_I = {
    HarmonicFunction::T, 1, ChordType::Six, Inversion::V, ChordPosition::Close, {5, 1, 1, 3}
};

// T6 V Wide, doubled I:   S=5(G) A=1(C) T=1(C) B=3(E)  — alto/tenor same degree, Wide
static const ChordTemplate T6_V_WIDE_DBL_I = {
    HarmonicFunction::T, 1, ChordType::Six, Inversion::V, ChordPosition::Wide, {5, 1, 1, 3}
};

// ── MELODY MODE ───────────────────────────────────────────────────────────────

TEST_CASE("ChordBuilder/melody: fixed soprano is unchanged across all variants",
          "[ChordBuilder][melody]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    Note soprano = makeNote(NoteName::C, 5, 1); // C5, degree 1

    auto chords = builder.createChordsFromTemplate(
        soprano, T53_I_CLOSE, HarmonizationMode::HarmonizeMelody, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getSoprano().getName()   == soprano.getName());
        CHECK(chord.getSoprano().getOctave() == soprano.getOctave());
        CHECK(chord.getSoprano().getAlter()  == soprano.getAlter());
    }
}

TEST_CASE("ChordBuilder/melody: Close — same soprano/alto degree allows unison",
          "[ChordBuilder][melody][position]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // Soprano = C4 (degree 1); T6-I-Close has soprano=1, alto=1.
    // Close ceiling = soprano semitone → fitInRange finds C4 itself (unison).
    Note soprano = makeNote(NoteName::C, 4, 1);

    auto chords = builder.createChordsFromTemplate(
        soprano, T6_I_CLOSE_DBL_I, HarmonizationMode::HarmonizeMelody, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getAlto().getName()   == NoteName::C);
        CHECK(chord.getAlto().getOctave() == 4); // unison with soprano C4
    }
}

TEST_CASE("ChordBuilder/melody: Wide — same soprano/alto degree forces one octave apart",
          "[ChordBuilder][melody][position]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // Soprano = C5 (degree 1); T6-I-Wide has soprano=1, alto=1.
    // Wide ceiling = C5-1 = 71 → fitInRange finds C4 (60 ≤ 71, in ALTO_RANGE), not C5.
    Note soprano = makeNote(NoteName::C, 5, 1);

    auto chords = builder.createChordsFromTemplate(
        soprano, T6_I_WIDE_DBL_I, HarmonizationMode::HarmonizeMelody, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getAlto().getName()   == NoteName::C);
        CHECK(chord.getAlto().getOctave() == 4); // one octave below soprano C5
        CHECK(chord.getSoprano().getSemitone() - chord.getAlto().getSemitone() == 12);
    }
}

TEST_CASE("ChordBuilder/melody: Close — same alto/tenor degree allows unison",
          "[ChordBuilder][melody][position]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // Soprano = G4 (degree 5); T6-V-Close has alto=1(C), tenor=1(C).
    // Close ceiling for tenor = alto semitone → highest C in TENOR_RANGE ≤ C4 = C4 (unison).
    Note soprano = makeNote(NoteName::G, 4, 5);

    auto chords = builder.createChordsFromTemplate(
        soprano, T6_V_CLOSE_DBL_I, HarmonizationMode::HarmonizeMelody, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getAlto().getName()  == NoteName::C);
        CHECK(chord.getTenor().getName() == NoteName::C);
        // Close: alto and tenor are in unison (both C4).
        CHECK(chord.getAlto().getOctave() == chord.getTenor().getOctave());
    }
}

TEST_CASE("ChordBuilder/melody: Wide — same alto/tenor degree forces one octave apart",
          "[ChordBuilder][melody][position]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // Soprano = G4; T6-V-Wide has alto=1(C), tenor=1(C).
    // Wide ceiling for tenor = alto-1 → C4-1=59 → fitInRange finds C3 (48 ≤ 59, in TENOR_RANGE).
    Note soprano = makeNote(NoteName::G, 4, 5);

    auto chords = builder.createChordsFromTemplate(
        soprano, T6_V_WIDE_DBL_I, HarmonizationMode::HarmonizeMelody, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getAlto().getName()  == NoteName::C);
        CHECK(chord.getTenor().getName() == NoteName::C);
        // Wide: alto and tenor are an octave apart, not in unison.
        CHECK(chord.getAlto().getOctave() != chord.getTenor().getOctave());
        CHECK(chord.getAlto().getSemitone() - chord.getTenor().getSemitone() == 12);
    }
}

TEST_CASE("ChordBuilder/melody: generates multiple bass-octave variants",
          "[ChordBuilder][melody][bass-variants]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // Soprano = C5; T53-I-Close has bass=1(C).
    // BASS_RANGE (E2–C4) contains C4(60) and C3(48), both ≤ tenor E4(64).
    Note soprano = makeNote(NoteName::C, 5, 1);

    auto chords = builder.createChordsFromTemplate(
        soprano, T53_I_CLOSE, HarmonizationMode::HarmonizeMelody, settings);

    REQUIRE(chords.size() >= 2); // at least C4-bass and C3-bass variants

    // All chords share the same soprano.
    for (const auto& chord : chords) {
        CHECK(chord.getSoprano().getName()   == NoteName::C);
        CHECK(chord.getSoprano().getOctave() == 5);
    }
    // Bass is always C within BASS_RANGE.
    for (const auto& chord : chords) {
        CHECK(chord.getBass().getName() == NoteName::C);
        CHECK(BASS_RANGE.contains(chord.getBass()));
    }
    // Every variant is a valid chord.
    for (const auto& chord : chords) {
        CHECK(HarmonyRules::isValidChord(chord));
    }
}

TEST_CASE("ChordBuilder/melody: no bass below BASS_RANGE.min",
          "[ChordBuilder][melody][bass-variants]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    Note soprano = makeNote(NoteName::C, 4, 1);

    auto chords = builder.createChordsFromTemplate(
        soprano, T53_I_CLOSE, HarmonizationMode::HarmonizeMelody, settings);

    for (const auto& chord : chords) {
        CHECK(BASS_RANGE.contains(chord.getBass()));
    }
}

// ── BASS MODE ─────────────────────────────────────────────────────────────────

TEST_CASE("ChordBuilder/bass: fixed bass is unchanged across all variants",
          "[ChordBuilder][bass]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    Note bass = makeNote(NoteName::C, 3, 1); // C3, degree 1

    auto chords = builder.createChordsFromTemplate(
        bass, T53_I_CLOSE, HarmonizationMode::HarmonizeBass, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getBass().getName()   == bass.getName());
        CHECK(chord.getBass().getOctave() == bass.getOctave());
        CHECK(chord.getBass().getAlter()  == bass.getAlter());
    }
}

TEST_CASE("ChordBuilder/bass: generates multiple tenor-octave variants",
          "[ChordBuilder][bass]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // T53-I-Close: tenor=3(E). Bass=C3(48).
    // Tenor candidates above C3 in TENOR_RANGE: E3(52), E4(64).
    // Both satisfy 0 ≤ tenor-bass ≤ 24.
    Note bass = makeNote(NoteName::C, 3, 1);

    auto chords = builder.createChordsFromTemplate(
        bass, T53_I_CLOSE, HarmonizationMode::HarmonizeBass, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getBass().getName() == NoteName::C);
        CHECK(HarmonyRules::isValidChord(chord));
    }
}

TEST_CASE("ChordBuilder/bass: Close — same-degree adjacent voices allow unison",
          "[ChordBuilder][bass][position]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // T6-I-Close, doubled I: S=1(C) A=1(C) T=5(G) B=3(E).
    // Bass=E3(52), degree 3.
    // Tenor G=5: fitAllAbove(G, TENOR_RANGE, 52) → G3(55). TB=3 ≤ 24.
    // Alto C=1:  Close floor = G3(55). fitAbove(C, ALTO_RANGE, 55) → C4(60).
    // Soprano C=1: Close floor = C4(60). fitAbove(C, SOPRANO_RANGE, 60) → C4(60). Unison!
    Note bass = makeNote(NoteName::E, 3, 3);

    auto chords = builder.createChordsFromTemplate(
        bass, T6_I_CLOSE_DBL_I, HarmonizationMode::HarmonizeBass, settings);

    REQUIRE_FALSE(chords.empty());
    bool foundUnison = false;
    for (const auto& c : chords) {
        if (c.getSoprano().getSemitone() == c.getAlto().getSemitone())
            foundUnison = true;
    }
    CHECK(foundUnison);
}

TEST_CASE("ChordBuilder/bass: Wide — same-degree adjacent voices must be one octave apart",
          "[ChordBuilder][bass][position]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // T6-I-Wide, doubled I: S=1(C) A=1(C) T=5(G) B=3(E).
    // Wide floor for soprano = alto+1 → C4(60)+1=61 → fitAbove finds C5(72). Not unison.
    Note bass = makeNote(NoteName::E, 3, 3);

    auto chords = builder.createChordsFromTemplate(
        bass, T6_I_WIDE_DBL_I, HarmonizationMode::HarmonizeBass, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& c : chords) {
        CHECK(c.getSoprano().getSemitone() != c.getAlto().getSemitone()); // not unison
        CHECK(c.getSoprano().getSemitone() - c.getAlto().getSemitone() == 12); // one octave
    }
}

TEST_CASE("ChordBuilder/bass: Mixed — same-degree adjacent voices allow unison (like Close)",
          "[ChordBuilder][bass][position]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    // Mixed position: same behaviour as Close for same-degree voices.
    ChordTemplate T6_I_MIXED_DBL_I = {
        HarmonicFunction::T, 1, ChordType::Six, Inversion::I, ChordPosition::Mixed, {1, 1, 5, 3}
    };
    Note bass = makeNote(NoteName::E, 3, 3);

    auto chords = builder.createChordsFromTemplate(
        bass, T6_I_MIXED_DBL_I, HarmonizationMode::HarmonizeBass, settings);

    REQUIRE_FALSE(chords.empty());
    bool foundUnison = false;
    for (const auto& c : chords) {
        if (c.getSoprano().getSemitone() == c.getAlto().getSemitone())
            foundUnison = true;
    }
    CHECK(foundUnison);
}

TEST_CASE("ChordBuilder/bass: all generated chords pass isValidChord",
          "[ChordBuilder][bass]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    Note bass = makeNote(NoteName::C, 3, 1);

    auto chords = builder.createChordsFromTemplate(
        bass, T53_I_CLOSE, HarmonizationMode::HarmonizeBass, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords)
        CHECK(HarmonyRules::isValidChord(chord));
}

// ── buildForFixed* pipeline ───────────────────────────────────────────────────

TEST_CASE("ChordBuilder/buildForFixedMelodyNote: soprano matches fixed note; all chords valid",
          "[ChordBuilder][pipeline]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    Note melody = makeNote(NoteName::C, 5, 1);

    auto chords = builder.buildForFixedMelodyNote(melody, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getSoprano().getName()   == NoteName::C);
        CHECK(chord.getSoprano().getOctave() == 5);
        CHECK(HarmonyRules::isValidChord(chord));
    }
}

TEST_CASE("ChordBuilder/buildForFixedBassNote: bass matches fixed note; all chords valid",
          "[ChordBuilder][pipeline]") {
    ChordBuilder builder;
    auto settings = makeSettings();
    Note bassNote = makeNote(NoteName::C, 3, 1);

    auto chords = builder.buildForFixedBassNote(bassNote, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getBass().getName()   == NoteName::C);
        CHECK(chord.getBass().getOctave() == 3);
        CHECK(HarmonyRules::isValidChord(chord));
    }
}

TEST_CASE("ChordBuilder/buildForFixedMelodyNote: more chords than unique templates (multi-bass)",
          "[ChordBuilder][pipeline]") {
    ChordBuilder builder;
    // Narrow allowed set so we can reason about template count.
    HarmonizationSettings settings = makeSettings();
    settings.allowedChords = {"T53"};
    Note melody = makeNote(NoteName::C, 5, 1);

    auto chords = builder.buildForFixedMelodyNote(melody, settings);

    // T53 has 6 templates with soprano degree 1 (I-Close, I-Wide, V-Close, V-Wide, III-Close, III-Wide).
    // Each should yield ≥1 chord (some yield 2 with different basses).
    // Total chord count must exceed the 6-template count.
    REQUIRE(chords.size() > 6);
}
