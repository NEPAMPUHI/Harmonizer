#include <catch2/catch_test_macros.hpp>
#include "checking/CheckChordIdentifier.h"
#include "checking/CheckHarmonicPosition.h"
#include "harmonization/ChordBuilder.h"
#include "domain/HarmonizationSettings.h"
#include "domain/Note.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static HarmonizationSettings makeSettings(const std::string& key = "C",
                                           const std::string& chordType = "T53") {
    HarmonizationSettings s;
    s.key         = key;
    s.scaleModes  = {};
    s.measureCount = 1;
    s.allowedChords = {chordType};
    return s;
}

// Build the first chord of chordType in key using the harmonizer's own logic.
// This guarantees the notes are exactly what the identifier is expected to match.
static Chord getBuiltChord(const std::string& chordType, const std::string& key = "C") {
    ChordBuilder builder;
    auto chords = builder.buildAllValid(makeSettings(key, chordType));
    REQUIRE(!chords.empty());
    return chords[0];
}

static CheckHarmonicPosition posFromChord(const Chord& c) {
    CheckHarmonicPosition pos;
    pos.measureIndex                = 1;
    pos.positionInMeasureSixteenths = 0;
    pos.durationSixteenths          = 4;
    pos.soprano    = c.getSoprano();  pos.hasSoprano = true;
    pos.alto       = c.getAlto();     pos.hasAlto    = true;
    pos.tenor      = c.getTenor();    pos.hasTenor   = true;
    pos.bass       = c.getBass();     pos.hasBass    = true;
    return pos;
}

static Note restNote() {
    return Note(NoteName::C, 4, 0, 0, 4, false, false, true);
}

// ── Test 1: T53 → known ───────────────────────────────────────────────────────

TEST_CASE("CheckChordIdentifier: T53 chord is identified as known", "[chordident]") {
    const Chord chord = getBuiltChord("T53");
    const CheckHarmonicPosition pos = posFromChord(chord);

    HarmonizationSettings settings;
    settings.key = "C";
    settings.scaleModes = {};

    CheckChordIdentifier ident;
    const auto results = ident.identify({pos}, settings);

    REQUIRE(results.size() == 1);
    CHECK(results[0].isKnownChord);
    CHECK(results[0].chord.getName() == chord.getName());
    CHECK(results[0].matchedTemplate.degree == 1);
    CHECK(results[0].matchedTemplate.type   == ChordType::Triad);
}

// ── Test 2: T6 → known ────────────────────────────────────────────────────────

TEST_CASE("CheckChordIdentifier: T6 chord is identified as known", "[chordident]") {
    const Chord chord = getBuiltChord("T6");
    const CheckHarmonicPosition pos = posFromChord(chord);

    HarmonizationSettings settings;
    settings.key = "C";
    settings.scaleModes = {};

    CheckChordIdentifier ident;
    const auto results = ident.identify({pos}, settings);

    REQUIRE(results.size() == 1);
    CHECK(results[0].isKnownChord);
    CHECK(results[0].chord.getName() == chord.getName());
    CHECK(results[0].matchedTemplate.type == ChordType::Six);
}

// ── Test 3: D7 → known ────────────────────────────────────────────────────────

TEST_CASE("CheckChordIdentifier: D7 chord is identified as known", "[chordident]") {
    const Chord chord = getBuiltChord("D7");
    const CheckHarmonicPosition pos = posFromChord(chord);

    HarmonizationSettings settings;
    settings.key = "C";
    settings.scaleModes = {};

    CheckChordIdentifier ident;
    const auto results = ident.identify({pos}, settings);

    REQUIRE(results.size() == 1);
    CHECK(results[0].isKnownChord);
    CHECK(results[0].chord.getName() == chord.getName());
    CHECK(results[0].matchedTemplate.degree == 5);
    CHECK(results[0].matchedTemplate.type   == ChordType::Seventh);
}

// ── Test 4: chromatic cluster → unknown ──────────────────────────────────────

TEST_CASE("CheckChordIdentifier: chromatic cluster → isKnownChord false", "[chordident]") {
    // C4–C#4–D4–D#4: not any recognizable SATB chord
    auto makeNote = [](NoteName n, int oct, int alter) {
        return Note(n, oct, alter, 0, 4, false);
    };

    CheckHarmonicPosition pos;
    pos.measureIndex = 1; pos.positionInMeasureSixteenths = 0; pos.durationSixteenths = 4;
    pos.soprano = makeNote(NoteName::D, 5, 1);  pos.hasSoprano = true;  // D#5
    pos.alto    = makeNote(NoteName::C, 5, 1);  pos.hasAlto    = true;  // C#5
    pos.tenor   = makeNote(NoteName::B, 4, 1);  pos.hasTenor   = true;  // B#4 ≡ C5
    pos.bass    = makeNote(NoteName::A, 3, 1);  pos.hasBass    = true;  // A#3

    HarmonizationSettings settings;
    settings.key = "C";
    settings.scaleModes = {};

    CheckChordIdentifier ident;
    const auto results = ident.identify({pos}, settings);

    REQUIRE(results.size() == 1);
    CHECK_FALSE(results[0].isKnownChord);
}

// ── Test 5: one voice is a rest → unknown ─────────────────────────────────────

TEST_CASE("CheckChordIdentifier: voice with rest → isKnownChord false", "[chordident]") {
    const Chord chord = getBuiltChord("T53");
    CheckHarmonicPosition pos = posFromChord(chord);
    pos.bass = restNote();   // replace bass with a rest

    HarmonizationSettings settings;
    settings.key = "C";
    settings.scaleModes = {};

    CheckChordIdentifier ident;
    const auto results = ident.identify({pos}, settings);

    REQUIRE(results.size() == 1);
    CHECK_FALSE(results[0].isKnownChord);
}

// ── Test 6: voice absent (hasX == false) → unknown ────────────────────────────

TEST_CASE("CheckChordIdentifier: absent voice → isKnownChord false", "[chordident]") {
    const Chord chord = getBuiltChord("T53");
    CheckHarmonicPosition pos = posFromChord(chord);
    pos.hasTenor = false;   // tenor not present

    HarmonizationSettings settings;
    settings.key = "C";
    settings.scaleModes = {};

    CheckChordIdentifier ident;
    const auto results = ident.identify({pos}, settings);

    REQUIRE(results.size() == 1);
    CHECK_FALSE(results[0].isKnownChord);
}

// ── Test 7: matching degree pattern but wrong actual note → unknown ───────────

TEST_CASE("CheckChordIdentifier: wrong pitch despite matching degree pattern → unknown",
          "[chordident]") {
    // Start with a valid T53 chord, then shift soprano by +1 semitone (alter C→C#).
    // The degree pattern (soprano=deg1) still triggers T53 candidates,
    // but the semitone comparison must fail.
    const Chord chord = getBuiltChord("T53");
    CheckHarmonicPosition pos = posFromChord(chord);

    const Note& origSop = chord.getSoprano();
    // Raise soprano by one semitone via alter+1
    pos.soprano = Note(origSop.getName(), origSop.getOctave(), origSop.getAlter() + 1,
                       0, 4, false);

    HarmonizationSettings settings;
    settings.key = "C";
    settings.scaleModes = {};

    CheckChordIdentifier ident;
    const auto results = ident.identify({pos}, settings);

    REQUIRE(results.size() == 1);
    CHECK_FALSE(results[0].isKnownChord);
}
