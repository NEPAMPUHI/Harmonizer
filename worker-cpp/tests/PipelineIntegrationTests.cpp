#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "infrastructure/JobParser.h"
#include "harmonization/MelodyHarmonizer.h"
#include "harmonization/BassHarmonizer.h"
#include "harmonization/NoteDegreesResolver.h"
#include "harmonization/ChordBuilder.h"
#include "harmonization/ScaleDegreeCalculator.h"
#include "domain/HarmonicPositionBuilder.h"
#include "harmonization/HarmonicRhythmPlanner.h"
#include "domain/HarmonyRules.h"

static HarmonizationSettings makePipelineSettings() {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural", "harmonic", "melodic"};
    s.allowedChords = {"T53", "T6", "S53", "S6", "D53", "D6", "D7"};
    return s;
}

static HarmonizationSettings makePipelineSettingsForKey(const std::string& key) {
    HarmonizationSettings s = makePipelineSettings();
    s.key = key;
    return s;
}

// Mirrors what NoteDegreesResolver does in production.
static void resolveNoteInPlace(Note& note, const HarmonizationSettings& settings) {
    ScaleDegreeCalculator calc;
    DegreeInfo info = calc.calculateDetailed(note, settings);
    note.setDegree(info.degree);
    note.setScaleRelation(info.relation);
}

// Canonical frontend-like JSON using the actual JobParser field names.
// (Frontend serialises via workerRequest.js: step/octave/alter/durationSixteenths.)

static constexpr const char* MELODY_JSON = R"({
  "jobId": "job_test_melody_001",
  "mode": "harmonize_melody",
  "settings": {
    "key": "C",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": ["T53", "T6", "S53", "S6", "D53", "D6", "D7"]
  },
  "input": {
    "notes": [
      { "step": "C", "octave": 4, "alter": 0, "durationSixteenths": 4 },
      { "step": "E", "octave": 4, "alter": 0, "durationSixteenths": 4 },
      { "step": "G", "octave": 4, "alter": 0, "durationSixteenths": 4 },
      { "step": "C", "octave": 5, "alter": 0, "durationSixteenths": 4 }
    ]
  }
})";

static constexpr const char* BASS_JSON = R"({
  "jobId": "job_test_bass_001",
  "mode": "harmonize_bass",
  "settings": {
    "key": "C",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": ["T53", "T6", "S53", "S6", "D53", "D6", "D7"]
  },
  "input": {
    "notes": [
      { "step": "C", "octave": 3, "alter": 0, "durationSixteenths": 4 },
      { "step": "G", "octave": 2, "alter": 0, "durationSixteenths": 4 },
      { "step": "C", "octave": 3, "alter": 0, "durationSixteenths": 4 },
      { "step": "G", "octave": 2, "alter": 0, "durationSixteenths": 4 }
    ]
  }
})";

// Single-beat lowered notes — produce exactly one harmonic position each.
// (durationSixteenths=4 in 4/4 covers only beat 1; remaining 3 beats have no note → skipped.)

static constexpr const char* MELODY_LOWERED_VI_C_JSON = R"({
  "jobId": "test_lowered_vi_melody_c",
  "mode": "harmonize_melody",
  "settings": {
    "key": "C",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": ["T53", "T6", "S53", "S6", "D53", "D6", "D7"]
  },
  "input": { "notes": [
    { "step": "A", "octave": 4, "alter": -1, "durationSixteenths": 4 }
  ]}
})";

static constexpr const char* MELODY_LOWERED_VII_C_JSON = R"({
  "jobId": "test_lowered_vii_melody_c",
  "mode": "harmonize_melody",
  "settings": {
    "key": "C",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": ["T53", "T6", "S53", "S6", "D53", "D6", "D7"]
  },
  "input": { "notes": [
    { "step": "B", "octave": 4, "alter": -1, "durationSixteenths": 4 }
  ]}
})";

static constexpr const char* MELODY_LOWERED_VI_G_JSON = R"({
  "jobId": "test_lowered_vi_melody_g",
  "mode": "harmonize_melody",
  "settings": {
    "key": "G",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": ["T53", "T6", "S53", "S6", "D53", "D6", "D7"]
  },
  "input": { "notes": [
    { "step": "E", "octave": 4, "alter": -1, "durationSixteenths": 4 }
  ]}
})";

static constexpr const char* MELODY_LOWERED_VII_G_JSON = R"({
  "jobId": "test_lowered_vii_melody_g",
  "mode": "harmonize_melody",
  "settings": {
    "key": "G",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": ["T53", "T6", "S53", "S6", "D53", "D6", "D7"]
  },
  "input": { "notes": [
    { "step": "F", "octave": 4, "alter": 0, "durationSixteenths": 4 }
  ]}
})";

static constexpr const char* BASS_LOWERED_VI_C_JSON = R"({
  "jobId": "test_lowered_vi_bass_c",
  "mode": "harmonize_bass",
  "settings": {
    "key": "C",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": ["T53", "T6", "S53", "S6", "D53", "D6", "D7"]
  },
  "input": { "notes": [
    { "step": "A", "octave": 3, "alter": -1, "durationSixteenths": 4 }
  ]}
})";

static constexpr const char* BASS_LOWERED_VII_C_JSON = R"({
  "jobId": "test_lowered_vii_bass_c",
  "mode": "harmonize_bass",
  "settings": {
    "key": "C",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": [],
    "allowedChords": ["T53", "T6", "S53", "S6", "D53", "D6", "D7"]
  },
  "input": { "notes": [
    { "step": "B", "octave": 2, "alter": -1, "durationSixteenths": 4 }
  ]}
})";

// 4/4, G4-G4-G4-C5: G4 (degree 5) on beats 1+3 (strong) makes D64/T64 candidate nodes;
// the beat rule must block them. Beat 2 (weak) may keep SixFour.
static constexpr const char* SIXFOUR_BEAT_RULE_JSON = R"({
  "jobId": "test_sixfour_beat_rule",
  "mode": "harmonize_melody",
  "settings": {
    "key": "C",
    "scaleMode": ["natural", "harmonic", "melodic"],
    "measureCount": 1,
    "timeSignature": { "beats": 4, "beatType": 4 },
    "anacrusisSixteenths": 0,
    "forbiddenRules": ["s_after_d"],
    "allowedChords": ["T53", "T6", "T64", "S53", "S6", "D53", "D6", "D7", "D64"]
  },
  "input": {
    "notes": [
      { "step": "G", "octave": 4, "alter": 0, "durationSixteenths": 4 },
      { "step": "G", "octave": 4, "alter": 0, "durationSixteenths": 4 },
      { "step": "G", "octave": 4, "alter": 0, "durationSixteenths": 4 },
      { "step": "C", "octave": 5, "alter": 0, "durationSixteenths": 4 }
    ]
  }
})";

// ── Parse stage ───────────────────────────────────────────────────────────────

TEST_CASE("Pipeline/melody: JSON parses without error", "[pipeline][parse]") {
    JobParser parser;
    REQUIRE_NOTHROW(parser.parse(MELODY_JSON));
}

TEST_CASE("Pipeline/bass: JSON parses without error", "[pipeline][parse]") {
    JobParser parser;
    REQUIRE_NOTHROW(parser.parse(BASS_JSON));
}

TEST_CASE("Pipeline/melody: parsed notes have expected fields", "[pipeline][parse]") {
    JobParser parser;
    auto job = parser.parse(MELODY_JSON);

    REQUIRE(job.input.notes.size() == 4);

    // step/octave/alter are read correctly; degree must still be 0 (unresolved).
    REQUIRE(job.input.notes[0].getName()   == NoteName::C);
    REQUIRE(job.input.notes[0].getOctave() == 4);
    REQUIRE(job.input.notes[0].getAlter()  == 0);
    REQUIRE(job.input.notes[0].getDegree() == 0);

    REQUIRE(job.input.notes[1].getName()   == NoteName::E);
    REQUIRE(job.input.notes[2].getName()   == NoteName::G);
    REQUIRE(job.input.notes[3].getOctave() == 5);
}

TEST_CASE("Pipeline/bass: parsed notes have expected fields", "[pipeline][parse]") {
    JobParser parser;
    auto job = parser.parse(BASS_JSON);

    REQUIRE(job.input.notes.size() == 4);
    REQUIRE(job.input.notes[0].getName()   == NoteName::C);
    REQUIRE(job.input.notes[0].getOctave() == 3);
    REQUIRE(job.input.notes[1].getName()   == NoteName::G);
    REQUIRE(job.input.notes[1].getOctave() == 2);
    // alter must be present and equal to 0 (not missing/garbage)
    REQUIRE(job.input.notes[0].getAlter()  == 0);
}

// ── Degree resolution stage ───────────────────────────────────────────────────

TEST_CASE("Pipeline/melody: NoteDegreesResolver assigns non-zero degrees", "[pipeline][degree]") {
    JobParser parser;
    auto job = parser.parse(MELODY_JSON);

    HarmonicRhythmPlanner planner;
    HarmonicPositionBuilder builder;
    auto segments  = planner.buildSegments(job.input.notes, job.settings);
    auto positions = builder.build(segments, job.settings, HarmonizationMode::HarmonizeMelody);

    NoteDegreesResolver resolver;
    resolver.resolveInPlace(positions, job.settings);

    // C, E, G, C in C major → degrees 1, 3, 5, 1
    REQUIRE(positions[0].fixedNote.getDegree() == 1);
    REQUIRE(positions[1].fixedNote.getDegree() == 3);
    REQUIRE(positions[2].fixedNote.getDegree() == 5);
    REQUIRE(positions[3].fixedNote.getDegree() == 1);
}

TEST_CASE("Pipeline/bass: NoteDegreesResolver assigns non-zero degrees", "[pipeline][degree]") {
    JobParser parser;
    auto job = parser.parse(BASS_JSON);

    HarmonicRhythmPlanner planner;
    HarmonicPositionBuilder builder;
    auto segments  = planner.buildSegments(job.input.notes, job.settings);
    auto positions = builder.build(segments, job.settings, HarmonizationMode::HarmonizeBass);

    NoteDegreesResolver resolver;
    resolver.resolveInPlace(positions, job.settings);

    // C and G in C major → degrees 1 and 5
    REQUIRE(positions[0].fixedNote.getDegree() == 1);
    REQUIRE(positions[1].fixedNote.getDegree() == 5);
    REQUIRE(positions[2].fixedNote.getDegree() == 1);
    REQUIRE(positions[3].fixedNote.getDegree() == 5);
}

// ── Full harmonization pipeline ───────────────────────────────────────────────

TEST_CASE("Pipeline/melody: C major C-E-G-C produces at least one harmonization variant", "[pipeline][regression]") {
    JobParser parser;
    auto job = parser.parse(MELODY_JSON);

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(job.input, job.settings);

    REQUIRE_FALSE(variants.empty());
}

TEST_CASE("Pipeline/bass: C major C-G-C-G produces at least one harmonization variant", "[pipeline][regression]") {
    JobParser parser;
    auto job = parser.parse(BASS_JSON);

    BassHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(job.input, job.settings);

    REQUIRE_FALSE(variants.empty());
}

// ── ChordBuilder: voice correctness for known notes ───────────────────────────

TEST_CASE("Pipeline/melody: buildForFixedMelodyNote — soprano matches fixed note and chords are valid",
          "[pipeline][chord-validity]") {
    ChordBuilder builder;
    auto settings = makePipelineSettings();
    Note melody(NoteName::C, 5, 0, 1, 4, false);

    auto chords = builder.buildForFixedMelodyNote(melody, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getSoprano().getName()   == NoteName::C);
        CHECK(chord.getSoprano().getOctave() == 5);
        CHECK(HarmonyRules::isValidChord(chord));
    }
}

TEST_CASE("Pipeline/melody: buildForFixedMelodyNote — multiple chords per position (bass variants)",
          "[pipeline][chord-validity]") {
    ChordBuilder builder;
    auto settings = makePipelineSettings();
    Note melody(NoteName::G, 4, 0, 5, 4, false);

    auto chords = builder.buildForFixedMelodyNote(melody, settings);

    // Multiple templates each produce ≥1 valid chord; total must exceed template count.
    REQUIRE(chords.size() > 1);
    for (const auto& chord : chords)
        CHECK(HarmonyRules::isValidChord(chord));
}

TEST_CASE("Pipeline/bass: buildForFixedBassNote — bass matches fixed note and chords are valid",
          "[pipeline][chord-validity]") {
    ChordBuilder builder;
    auto settings = makePipelineSettings();
    Note bassNote(NoteName::C, 3, 0, 1, 4, false);

    auto chords = builder.buildForFixedBassNote(bassNote, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& chord : chords) {
        CHECK(chord.getBass().getName()   == NoteName::C);
        CHECK(chord.getBass().getOctave() == 3);
        CHECK(HarmonyRules::isValidChord(chord));
    }
}

TEST_CASE("Pipeline/bass: buildForFixedBassNote — multiple chords per position (tenor variants)",
          "[pipeline][chord-validity]") {
    ChordBuilder builder;
    auto settings = makePipelineSettings();
    Note bassNote(NoteName::G, 2, 0, 5, 4, false);

    auto chords = builder.buildForFixedBassNote(bassNote, settings);

    REQUIRE(chords.size() > 1);
    for (const auto& chord : chords)
        CHECK(HarmonyRules::isValidChord(chord));
}

// ── ScaleRelation::Lowered — ChordBuilder voice correctness ──────────────────

// Returns true if no voice in the chord is the "natural" form of the lowered note.
// E.g. for Ab soprano: ensure no voice is A natural (name=A, alter=0).
static bool noNaturalConflict(const Chord& chord, NoteName name, int naturalAlter) {
    auto isConflict = [name, naturalAlter](const Note& n) {
        return n.getName() == name && n.getAlter() == naturalAlter;
    };
    return !isConflict(chord.getSoprano()) && !isConflict(chord.getAlto()) &&
           !isConflict(chord.getTenor())   && !isConflict(chord.getBass());
}

TEST_CASE("Pipeline/lowered: C major Ab melody soprano — chords valid, identity preserved, no A-natural",
          "[pipeline][lowered][melody]") {
    ChordBuilder builder;
    auto settings = makePipelineSettingsForKey("C");
    Note soprano(NoteName::A, 4, -1, 0, 4, false);
    resolveNoteInPlace(soprano, settings);

    REQUIRE(soprano.getDegree() == 6);
    REQUIRE(soprano.getScaleRelation() == ScaleRelation::Lowered);

    auto chords = builder.buildForFixedMelodyNote(soprano, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& c : chords) {
        CHECK(c.getSoprano().getName()  == NoteName::A);   // Ab, not G#
        CHECK(c.getSoprano().getAlter() == -1);
        CHECK(HarmonyRules::isValidChord(c));              // no Unsupported interval thrown
        CHECK(noNaturalConflict(c, NoteName::A, 0));       // no A natural anywhere
    }
}

TEST_CASE("Pipeline/lowered: C major Bb melody soprano — chords valid, identity preserved, no B-natural",
          "[pipeline][lowered][melody]") {
    ChordBuilder builder;
    auto settings = makePipelineSettingsForKey("C");
    Note soprano(NoteName::B, 4, -1, 0, 4, false);
    resolveNoteInPlace(soprano, settings);

    REQUIRE(soprano.getDegree() == 7);
    REQUIRE(soprano.getScaleRelation() == ScaleRelation::Lowered);

    auto chords = builder.buildForFixedMelodyNote(soprano, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& c : chords) {
        CHECK(c.getSoprano().getName()  == NoteName::B);
        CHECK(c.getSoprano().getAlter() == -1);
        CHECK(HarmonyRules::isValidChord(c));
        CHECK(noNaturalConflict(c, NoteName::B, 0));       // no B natural anywhere
    }
}

TEST_CASE("Pipeline/lowered: G major Eb melody soprano — chords valid, identity preserved, no E-natural",
          "[pipeline][lowered][melody]") {
    ChordBuilder builder;
    auto settings = makePipelineSettingsForKey("G");
    Note soprano(NoteName::E, 4, -1, 0, 4, false);
    resolveNoteInPlace(soprano, settings);

    REQUIRE(soprano.getDegree() == 6);
    REQUIRE(soprano.getScaleRelation() == ScaleRelation::Lowered);

    auto chords = builder.buildForFixedMelodyNote(soprano, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& c : chords) {
        CHECK(c.getSoprano().getName()  == NoteName::E);
        CHECK(c.getSoprano().getAlter() == -1);
        CHECK(HarmonyRules::isValidChord(c));
        CHECK(noNaturalConflict(c, NoteName::E, 0));       // no E natural anywhere
    }
}

TEST_CASE("Pipeline/lowered: G major F melody soprano (♭VII) — chords valid, identity preserved, no F-sharp",
          "[pipeline][lowered][melody]") {
    ChordBuilder builder;
    auto settings = makePipelineSettingsForKey("G");
    Note soprano(NoteName::F, 4, 0, 0, 4, false);
    resolveNoteInPlace(soprano, settings);

    REQUIRE(soprano.getDegree() == 7);
    REQUIRE(soprano.getScaleRelation() == ScaleRelation::Lowered);

    auto chords = builder.buildForFixedMelodyNote(soprano, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& c : chords) {
        CHECK(c.getSoprano().getName()  == NoteName::F);
        CHECK(c.getSoprano().getAlter() == 0);             // F natural, not E#
        CHECK(HarmonyRules::isValidChord(c));
        CHECK(noNaturalConflict(c, NoteName::F, 1));       // no F# anywhere
    }
}

TEST_CASE("Pipeline/lowered: C major Ab bass — chords valid, identity preserved, no A-natural",
          "[pipeline][lowered][bass]") {
    ChordBuilder builder;
    auto settings = makePipelineSettingsForKey("C");
    Note bass(NoteName::A, 3, -1, 0, 4, false);
    resolveNoteInPlace(bass, settings);

    REQUIRE(bass.getDegree() == 6);
    REQUIRE(bass.getScaleRelation() == ScaleRelation::Lowered);

    auto chords = builder.buildForFixedBassNote(bass, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& c : chords) {
        CHECK(c.getBass().getName()  == NoteName::A);
        CHECK(c.getBass().getAlter() == -1);
        CHECK(HarmonyRules::isValidChord(c));
        CHECK(noNaturalConflict(c, NoteName::A, 0));
    }
}

TEST_CASE("Pipeline/lowered: C major Bb bass — chords valid, identity preserved, no B-natural",
          "[pipeline][lowered][bass]") {
    ChordBuilder builder;
    auto settings = makePipelineSettingsForKey("C");
    Note bass(NoteName::B, 2, -1, 0, 4, false);
    resolveNoteInPlace(bass, settings);

    REQUIRE(bass.getDegree() == 7);
    REQUIRE(bass.getScaleRelation() == ScaleRelation::Lowered);

    auto chords = builder.buildForFixedBassNote(bass, settings);

    REQUIRE_FALSE(chords.empty());
    for (const auto& c : chords) {
        CHECK(c.getBass().getName()  == NoteName::B);
        CHECK(c.getBass().getAlter() == -1);
        CHECK(HarmonyRules::isValidChord(c));
        CHECK(noNaturalConflict(c, NoteName::B, 0));
    }
}

// ── ScaleRelation::Lowered — full harmonization pipeline ─────────────────────

TEST_CASE("Pipeline/lowered: C major Ab melody — full harmonizer produces variants",
          "[pipeline][lowered][full]") {
    JobParser parser;
    auto job = parser.parse(MELODY_LOWERED_VI_C_JSON);

    REQUIRE(job.input.notes[0].getName()  == NoteName::A);
    REQUIRE(job.input.notes[0].getAlter() == -1);

    MelodyHarmonizer harmonizer;
    std::vector<HarmonizationVariant> variants;
    REQUIRE_NOTHROW(variants = harmonizer.harmonize(job.input, job.settings));
    REQUIRE_FALSE(variants.empty());
}

TEST_CASE("Pipeline/lowered: C major Bb melody — full harmonizer produces variants",
          "[pipeline][lowered][full]") {
    JobParser parser;
    auto job = parser.parse(MELODY_LOWERED_VII_C_JSON);

    REQUIRE(job.input.notes[0].getName()  == NoteName::B);
    REQUIRE(job.input.notes[0].getAlter() == -1);

    MelodyHarmonizer harmonizer;
    std::vector<HarmonizationVariant> variants;
    REQUIRE_NOTHROW(variants = harmonizer.harmonize(job.input, job.settings));
    REQUIRE_FALSE(variants.empty());
}

TEST_CASE("Pipeline/lowered: G major Eb melody — full harmonizer produces variants",
          "[pipeline][lowered][full]") {
    JobParser parser;
    auto job = parser.parse(MELODY_LOWERED_VI_G_JSON);

    REQUIRE(job.input.notes[0].getName()  == NoteName::E);
    REQUIRE(job.input.notes[0].getAlter() == -1);

    MelodyHarmonizer harmonizer;
    std::vector<HarmonizationVariant> variants;
    REQUIRE_NOTHROW(variants = harmonizer.harmonize(job.input, job.settings));
    REQUIRE_FALSE(variants.empty());
}

TEST_CASE("Pipeline/lowered: G major F melody (♭VII) — full harmonizer produces variants",
          "[pipeline][lowered][full]") {
    JobParser parser;
    auto job = parser.parse(MELODY_LOWERED_VII_G_JSON);

    REQUIRE(job.input.notes[0].getName()  == NoteName::F);
    REQUIRE(job.input.notes[0].getAlter() == 0);

    MelodyHarmonizer harmonizer;
    std::vector<HarmonizationVariant> variants;
    REQUIRE_NOTHROW(variants = harmonizer.harmonize(job.input, job.settings));
    REQUIRE_FALSE(variants.empty());
}

TEST_CASE("Pipeline/lowered: C major Ab bass — full harmonizer produces variants",
          "[pipeline][lowered][full]") {
    JobParser parser;
    auto job = parser.parse(BASS_LOWERED_VI_C_JSON);

    REQUIRE(job.input.notes[0].getName()  == NoteName::A);
    REQUIRE(job.input.notes[0].getAlter() == -1);

    BassHarmonizer harmonizer;
    std::vector<HarmonizationVariant> variants;
    REQUIRE_NOTHROW(variants = harmonizer.harmonize(job.input, job.settings));
    REQUIRE_FALSE(variants.empty());
}

TEST_CASE("Pipeline/lowered: C major Bb bass — full harmonizer produces variants",
          "[pipeline][lowered][full]") {
    JobParser parser;
    auto job = parser.parse(BASS_LOWERED_VII_C_JSON);

    REQUIRE(job.input.notes[0].getName()  == NoteName::B);
    REQUIRE(job.input.notes[0].getAlter() == -1);

    BassHarmonizer harmonizer;
    std::vector<HarmonizationVariant> variants;
    REQUIRE_NOTHROW(variants = harmonizer.harmonize(job.input, job.settings));
    REQUIRE_FALSE(variants.empty());
}

// ── Beat rule: no SixFour on strong/medium beat ───────────────────────────────

TEST_CASE("Pipeline/beat-rule: No SixFour chord on strong or medium beat in 4/4",
          "[pipeline][beat-rule][SixFour]") {
    JobParser parser;
    auto job = parser.parse(SIXFOUR_BEAT_RULE_JSON);

    MelodyHarmonizer harmonizer;
    auto variants = harmonizer.harmonize(job.input, job.settings);
    REQUIRE_FALSE(variants.empty());

    for (const auto& variant : variants) {
        const auto& chords    = variant.musicScore.chords;
        const auto& positions = variant.musicScore.positions;
        REQUIRE(chords.size() == positions.size());

        for (size_t i = 0; i < chords.size(); ++i) {
            if (positions[i].isStrongBeat || positions[i].isMediumBeat) {
                CHECK(chords[i].getType() != ChordType::SixFour);
            }
        }
    }
}

// ── durationSixteenths: JSON parsing round-trip ──────────────────────────────

TEST_CASE("JobParser: durationSixteenths parsed correctly for all standard durations",
          "[duration][parser]") {
    JobParser parser;

    auto makeNoteJson = [](int dur) -> std::string {
        return R"({"step":"C","octave":4,"alter":0,"durationSixteenths":)" + std::to_string(dur) + "}";
    };

    using json = nlohmann::json;

    const std::pair<int,int> cases[] = {
        {16,  16},   // whole
        { 8,   8},   // half
        { 4,   4},   // quarter
        { 2,   2},   // eighth
        { 1,   1},   // sixteenth
    };

    for (auto [input, expected] : cases) {
        auto j   = json::parse(makeNoteJson(input));
        auto job = json::parse(R"({
            "jobId":"t","mode":"harmonize_melody",
            "settings":{"key":"C","scaleMode":["natural"],"measureCount":1,
                        "timeSignature":{"beats":4,"beatType":4},
                        "anacrusisSixteenths":0,
                        "forbiddenRules":[],
                        "allowedChords":["T53"]},
            "input":{"notes":[)" + makeNoteJson(input) + R"(]}
        })");
        HarmonizationJob parsed = parser.parse(job.dump());
        REQUIRE_FALSE(parsed.input.notes.empty());
        CHECK(parsed.input.notes[0].getDurationSixteenths() == expected);
    }
}
