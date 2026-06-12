#include <catch2/catch_test_macros.hpp>
#include "harmonization/NoteDegreesResolver.h"
#include "harmonization/ChordBuilder.h"
#include "domain/HarmonicPosition.h"
#include "domain/ScaleRelation.h"

static HarmonizationSettings makeSettings(const std::string& key) {
    HarmonizationSettings s;
    s.key = key;
    s.scaleModes = {"natural", "harmonic", "melodic"};
    s.allowedChords = {"T53", "T6", "T64", "S53", "S6", "S64", "D53", "D6", "D64", "D7"};
    return s;
}

static HarmonicPosition makePosition(NoteName name, int alter = 0) {
    Note n(name, 4, alter, 0, 4, false);
    HarmonicPosition pos{};
    pos.fixedNote = n;
    return pos;
}

// ── resolveInPlace assigns diatonic degrees 1-7 ──────────────────────────────

TEST_CASE("NoteDegreesResolver: C major diatonic notes get degrees 1-7", "[NoteDegreesResolver]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("C");

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::C),  // degree 1
        makePosition(NoteName::D),  // degree 2
        makePosition(NoteName::E),  // degree 3
        makePosition(NoteName::F),  // degree 4
        makePosition(NoteName::G),  // degree 5
        makePosition(NoteName::A),  // degree 6
        makePosition(NoteName::B),  // degree 7
    };

    for (const auto& p : positions)
        REQUIRE(p.fixedNote.getDegree() == 0);  // all unassigned before resolve

    resolver.resolveInPlace(positions, settings);

    REQUIRE(positions[0].fixedNote.getDegree() == 1);
    REQUIRE(positions[1].fixedNote.getDegree() == 2);
    REQUIRE(positions[2].fixedNote.getDegree() == 3);
    REQUIRE(positions[3].fixedNote.getDegree() == 4);
    REQUIRE(positions[4].fixedNote.getDegree() == 5);
    REQUIRE(positions[5].fixedNote.getDegree() == 6);
    REQUIRE(positions[6].fixedNote.getDegree() == 7);
}

TEST_CASE("NoteDegreesResolver: truly chromatic notes get degree -1", "[NoteDegreesResolver]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("C");

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::F,  1),  // F# — tritone, not in C major
        makePosition(NoteName::C,  1),  // C#
        makePosition(NoteName::G,  1),  // G#
    };

    resolver.resolveInPlace(positions, settings);

    for (const auto& p : positions)
        REQUIRE(p.fixedNote.getDegree() == -1);
}

TEST_CASE("NoteDegreesResolver: C major lowered VI and VII get degree 6 and 7", "[NoteDegreesResolver]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("C");

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::A, -1),  // Ab — ♭VI
        makePosition(NoteName::B, -1),  // Bb — ♭VII
    };

    resolver.resolveInPlace(positions, settings);

    REQUIRE(positions[0].fixedNote.getDegree() == 6);
    REQUIRE(positions[1].fixedNote.getDegree() == 7);
}

TEST_CASE("NoteDegreesResolver: A minor raised notes get degree 6 and 7", "[NoteDegreesResolver]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("a");

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::F, 1),  // F# — raised VI (melodic minor)
        makePosition(NoteName::G, 1),  // G# — raised VII (harmonic/melodic minor)
    };

    resolver.resolveInPlace(positions, settings);

    REQUIRE(positions[0].fixedNote.getDegree() == 6);
    REQUIRE(positions[1].fixedNote.getDegree() == 7);
}

TEST_CASE("NoteDegreesResolver: empty positions list is safe", "[NoteDegreesResolver]") {
    NoteDegreesResolver resolver;
    std::vector<HarmonicPosition> empty;
    REQUIRE_NOTHROW(resolver.resolveInPlace(empty, makeSettings("C")));
}

// ── resolveInPlace assigns scaleRelation ─────────────────────────────────────

TEST_CASE("NoteDegreesResolver: diatonic notes get scaleRelation Natural", "[NoteDegreesResolver][scaleRelation]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("C");

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::C),
        makePosition(NoteName::G),
    };
    resolver.resolveInPlace(positions, settings);

    REQUIRE(positions[0].fixedNote.getScaleRelation() == ScaleRelation::Natural);
    REQUIRE(positions[1].fixedNote.getScaleRelation() == ScaleRelation::Natural);
}

TEST_CASE("NoteDegreesResolver: chromatic note gets scaleRelation Chromatic", "[NoteDegreesResolver][scaleRelation]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("C");

    std::vector<HarmonicPosition> positions = { makePosition(NoteName::F, 1) };  // F#
    resolver.resolveInPlace(positions, settings);

    REQUIRE(positions[0].fixedNote.getDegree() == -1);
    REQUIRE(positions[0].fixedNote.getScaleRelation() == ScaleRelation::Chromatic);
}

TEST_CASE("NoteDegreesResolver: A minor raised notes get scaleRelation Raised", "[NoteDegreesResolver][scaleRelation]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("a");  // scaleModes = {natural, harmonic, melodic}

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::F, 1),  // F# — raised VI (melodic)
        makePosition(NoteName::G, 1),  // G# — raised VII (harmonic/melodic)
    };
    resolver.resolveInPlace(positions, settings);

    REQUIRE(positions[0].fixedNote.getDegree() == 6);
    REQUIRE(positions[0].fixedNote.getScaleRelation() == ScaleRelation::Raised);
    REQUIRE(positions[1].fixedNote.getDegree() == 7);
    REQUIRE(positions[1].fixedNote.getScaleRelation() == ScaleRelation::Raised);
}

TEST_CASE("NoteDegreesResolver: C major lowered VI/VII get scaleRelation Lowered", "[NoteDegreesResolver][scaleRelation]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("C");

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::A, -1),  // Ab — ♭VI
        makePosition(NoteName::B, -1),  // Bb — ♭VII
    };
    resolver.resolveInPlace(positions, settings);

    REQUIRE(positions[0].fixedNote.getDegree() == 6);
    REQUIRE(positions[0].fixedNote.getScaleRelation() == ScaleRelation::Lowered);
    REQUIRE(positions[1].fixedNote.getDegree() == 7);
    REQUIRE(positions[1].fixedNote.getScaleRelation() == ScaleRelation::Lowered);
}

TEST_CASE("NoteDegreesResolver: A minor natural notes get scaleRelation Natural", "[NoteDegreesResolver][scaleRelation]") {
    NoteDegreesResolver resolver;
    auto settings = makeSettings("a");

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::F),  // F — natural VI
        makePosition(NoteName::G),  // G — natural VII
    };
    resolver.resolveInPlace(positions, settings);

    REQUIRE(positions[0].fixedNote.getDegree() == 6);
    REQUIRE(positions[0].fixedNote.getScaleRelation() == ScaleRelation::Natural);
    REQUIRE(positions[1].fixedNote.getDegree() == 7);
    REQUIRE(positions[1].fixedNote.getScaleRelation() == ScaleRelation::Natural);
}

// ── pipeline: resolved degrees reach ChordBuilder ────────────────────────────

TEST_CASE("NoteDegreesResolver output is consumed correctly by ChordBuilder", "[NoteDegreesResolver][integration]") {
    NoteDegreesResolver resolver;
    ChordBuilder builder;
    auto settings = makeSettings("C");

    std::vector<HarmonicPosition> positions = {
        makePosition(NoteName::C),      // will be degree 1 → chords expected
        makePosition(NoteName::F, 1),   // will be degree -1 → no chords expected
    };

    resolver.resolveInPlace(positions, settings);

    auto chordsForC  = builder.buildForFixedMelodyNote(positions[0].fixedNote, settings);
    auto chordsForFs = builder.buildForFixedMelodyNote(positions[1].fixedNote, settings);

    REQUIRE(!chordsForC.empty());
    REQUIRE(chordsForFs.empty());
}
