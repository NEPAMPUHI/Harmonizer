#include <catch2/catch_test_macros.hpp>
#include "harmonization/ScaleDegreeCalculator.h"
#include "harmonization/ChordBuilder.h"

static Note makeNote(NoteName name, int alter = 0) {
    return Note(name, 4, alter, 0, Duration{4, 16}, false);
}

static HarmonizationSettings makeSettings(const std::string& key) {
    HarmonizationSettings s;
    s.key = key;
    s.scaleModes = {"natural", "harmonic", "melodic"};
    s.allowedChords = {"T53", "T6", "T64", "S53", "S6", "S64", "D53", "D6", "D64", "D7"};
    return s;
}

static HarmonizationSettings makeSettingsWithModes(const std::string& key,
                                                   std::vector<std::string> modes) {
    HarmonizationSettings s = makeSettings(key);
    s.scaleModes = std::move(modes);
    return s;
}

// Mirrors the production pipeline: resolve degree+scaleRelation, then build chords.
static bool hasChords(const Note& note, const std::string& key) {
    ScaleDegreeCalculator calc;
    ChordBuilder builder;
    HarmonizationSettings settings = makeSettings(key);
    Note resolved = note;
    DegreeInfo info = calc.calculateDetailed(resolved, settings);
    resolved.setDegree(info.degree);
    resolved.setScaleRelation(info.relation);
    return !builder.buildForFixedMelodyNote(resolved, settings).empty();
}

// ── ScaleDegreeCalculator::calculate ─────────────────────────────────────────

TEST_CASE("Note::setDegree: initial degree is 0 after parse-style construction", "[Note][setDegree]") {
    Note parsed = makeNote(NoteName::C);  // degree arg = 0 (mirrors JobParser::parseNote)
    REQUIRE(parsed.getDegree() == 0);
}

TEST_CASE("ScaleDegreeCalculator: stores the resolved scale degree", "[ScaleDegreeCalculator]") {
    ScaleDegreeCalculator calc;

    auto resolve = [&](NoteName name, int alter, const std::string& key) {
        Note n = makeNote(name, alter);
        REQUIRE(n.getDegree() == 0);  // degree=0 before resolution
        n.setDegree(calc.calculate(n, makeSettings(key)));
        return n.getDegree();
    };

    // C major
    REQUIRE(resolve(NoteName::C, 0, "C") == 1);
    REQUIRE(resolve(NoteName::E, 0, "C") == 3);
    REQUIRE(resolve(NoteName::G, 0, "C") == 5);
    REQUIRE(resolve(NoteName::F, 1, "C") == -1);  // F# — chromatic

    // A minor (G# = raised VII = harmonic/melodic minor)
    REQUIRE(resolve(NoteName::G, 1, "a") == 7);   // G# in A minor
}

// ── ScaleDegreeCalculator::calculateDetailed ─────────────────────────────────

TEST_CASE("calculateDetailed: A minor + natural mode", "[ScaleDegreeCalculator][detailed]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettingsWithModes("a", {"natural"});

    auto fi = calc.calculateDetailed(makeNote(NoteName::F), s);
    REQUIRE(fi.degree == 6);
    REQUIRE(fi.relation == ScaleRelation::Natural);

    auto gi = calc.calculateDetailed(makeNote(NoteName::G), s);
    REQUIRE(gi.degree == 7);
    REQUIRE(gi.relation == ScaleRelation::Natural);
}

TEST_CASE("calculateDetailed: A minor + harmonic mode — G# is Raised", "[ScaleDegreeCalculator][detailed]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettingsWithModes("a", {"harmonic"});

    auto info = calc.calculateDetailed(makeNote(NoteName::G, 1), s);
    REQUIRE(info.degree == 7);
    REQUIRE(info.relation == ScaleRelation::Raised);
}

TEST_CASE("calculateDetailed: A minor + melodic mode — F# and G# are Raised", "[ScaleDegreeCalculator][detailed]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettingsWithModes("a", {"melodic"});

    auto fi = calc.calculateDetailed(makeNote(NoteName::F, 1), s);
    REQUIRE(fi.degree == 6);
    REQUIRE(fi.relation == ScaleRelation::Raised);

    auto gi = calc.calculateDetailed(makeNote(NoteName::G, 1), s);
    REQUIRE(gi.degree == 7);
    REQUIRE(gi.relation == ScaleRelation::Raised);
}

TEST_CASE("calculateDetailed: C major — Natural, Lowered, Chromatic", "[ScaleDegreeCalculator][detailed]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettings("C");

    auto ci = calc.calculateDetailed(makeNote(NoteName::C), s);
    REQUIRE(ci.degree == 1);
    REQUIRE(ci.relation == ScaleRelation::Natural);

    auto ai = calc.calculateDetailed(makeNote(NoteName::A), s);
    REQUIRE(ai.degree == 6);
    REQUIRE(ai.relation == ScaleRelation::Natural);

    auto bi = calc.calculateDetailed(makeNote(NoteName::B), s);
    REQUIRE(bi.degree == 7);
    REQUIRE(bi.relation == ScaleRelation::Natural);

    auto abi = calc.calculateDetailed(makeNote(NoteName::A, -1), s);
    REQUIRE(abi.degree == 6);
    REQUIRE(abi.relation == ScaleRelation::Lowered);

    auto bbi = calc.calculateDetailed(makeNote(NoteName::B, -1), s);
    REQUIRE(bbi.degree == 7);
    REQUIRE(bbi.relation == ScaleRelation::Lowered);

    auto fsi = calc.calculateDetailed(makeNote(NoteName::F, 1), s);
    REQUIRE(fsi.degree == -1);
    REQUIRE(fsi.relation == ScaleRelation::Chromatic);
}

TEST_CASE("calculateDetailed: G major — Natural, Lowered, Chromatic", "[ScaleDegreeCalculator][detailed]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettings("G");

    auto ei = calc.calculateDetailed(makeNote(NoteName::E), s);
    REQUIRE(ei.degree == 6);
    REQUIRE(ei.relation == ScaleRelation::Natural);

    auto fsi = calc.calculateDetailed(makeNote(NoteName::F, 1), s);
    REQUIRE(fsi.degree == 7);
    REQUIRE(fsi.relation == ScaleRelation::Natural);

    auto ebi = calc.calculateDetailed(makeNote(NoteName::E, -1), s);
    REQUIRE(ebi.degree == 6);
    REQUIRE(ebi.relation == ScaleRelation::Lowered);

    auto fi = calc.calculateDetailed(makeNote(NoteName::F), s);
    REQUIRE(fi.degree == 7);
    REQUIRE(fi.relation == ScaleRelation::Lowered);

    auto csi = calc.calculateDetailed(makeNote(NoteName::C, 1), s);
    REQUIRE(csi.degree == -1);
    REQUIRE(csi.relation == ScaleRelation::Chromatic);
}

TEST_CASE("calculateDetailed: all three modes — union preserves Natural for natural-minor notes", "[ScaleDegreeCalculator][detailed]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettingsWithModes("a", {"natural", "harmonic", "melodic"});

    // F is in natural minor → Natural VI
    auto fi = calc.calculateDetailed(makeNote(NoteName::F), s);
    REQUIRE(fi.degree == 6);
    REQUIRE(fi.relation == ScaleRelation::Natural);

    // G is in natural minor → Natural VII
    auto gi = calc.calculateDetailed(makeNote(NoteName::G), s);
    REQUIRE(gi.degree == 7);
    REQUIRE(gi.relation == ScaleRelation::Natural);

    // F# only in melodic → Raised VI
    auto fsi = calc.calculateDetailed(makeNote(NoteName::F, 1), s);
    REQUIRE(fsi.degree == 6);
    REQUIRE(fsi.relation == ScaleRelation::Raised);

    // G# in harmonic/melodic → Raised VII
    auto gsi = calc.calculateDetailed(makeNote(NoteName::G, 1), s);
    REQUIRE(gsi.degree == 7);
    REQUIRE(gsi.relation == ScaleRelation::Raised);
}

// ── canUseTemplate reads pre-resolved degree ──────────────────────────────────

TEST_CASE("canUseTemplate reads fixedNote.getDegree(), not pitch+key", "[ChordBuilder][canUseTemplate]") {
    ChordBuilder builder;
    HarmonizationSettings anySettings = makeSettings("C");  // key does not affect canUseTemplate

    // T53-I (soprano=1): {1, 5, 3, 1}
    ChordTemplate t53_I{HarmonicFunction::T, 1, ChordType::Triad,
                        Inversion::I, ChordPosition::Close, {1, 5, 3, 1}};

    // T53-III (soprano=3): {3, 1, 5, 1}
    ChordTemplate t53_III{HarmonicFunction::T, 1, ChordType::Triad,
                          Inversion::III, ChordPosition::Close, {3, 1, 5, 1}};

    // T53-V (soprano=5): {5, 3, 1, 1}
    ChordTemplate t53_V{HarmonicFunction::T, 1, ChordType::Triad,
                        Inversion::V, ChordPosition::Close, {5, 3, 1, 1}};

    // D53-I (soprano=5): {5, 2, 7, 5}
    ChordTemplate d53{HarmonicFunction::D, 5, ChordType::Triad,
                      Inversion::I, ChordPosition::Close, {5, 2, 7, 5}};

    // D7-I (soprano=5): {5, 4, 7, 5}
    ChordTemplate d7{HarmonicFunction::D, 5, ChordType::Seventh,
                     Inversion::I, ChordPosition::Close, {5, 4, 7, 5}};

    auto withDegree = [](NoteName name, int d) {
        Note n = makeNote(name);
        n.setDegree(d);
        return n;
    };

    // In melody mode, canUseTemplate checks degreesInSatbOrder[0] (soprano position).
    SECTION("degree=1 matches T53-I (soprano=1)") {
        REQUIRE(builder.canUseTemplate(t53_I, withDegree(NoteName::C, 1),
                                       HarmonizationMode::HarmonizeMelody, anySettings));
    }

    SECTION("degree=3 matches T53-III (soprano=3), not T53-I (soprano=1)") {
        REQUIRE(builder.canUseTemplate(t53_III, withDegree(NoteName::E, 3),
                                       HarmonizationMode::HarmonizeMelody, anySettings));
        REQUIRE_FALSE(builder.canUseTemplate(t53_I, withDegree(NoteName::E, 3),
                                             HarmonizationMode::HarmonizeMelody, anySettings));
    }

    SECTION("degree=5 matches T53-V, D53, D7 (all have soprano=5)") {
        Note n = withDegree(NoteName::G, 5);
        REQUIRE(builder.canUseTemplate(t53_V, n, HarmonizationMode::HarmonizeMelody, anySettings));
        REQUIRE(builder.canUseTemplate(d53,   n, HarmonizationMode::HarmonizeMelody, anySettings));
        REQUIRE(builder.canUseTemplate(d7,    n, HarmonizationMode::HarmonizeMelody, anySettings));
        // T53-I has soprano=1, so degree=5 must not match it.
        REQUIRE_FALSE(builder.canUseTemplate(t53_I, n, HarmonizationMode::HarmonizeMelody, anySettings));
    }

    SECTION("degree=-1 (chromatic) rejects all templates") {
        Note n = withDegree(NoteName::F, -1);
        REQUIRE_FALSE(builder.canUseTemplate(t53_I, n, HarmonizationMode::HarmonizeMelody, anySettings));
        REQUIRE_FALSE(builder.canUseTemplate(d53,   n, HarmonizationMode::HarmonizeMelody, anySettings));
        REQUIRE_FALSE(builder.canUseTemplate(d7,    n, HarmonizationMode::HarmonizeMelody, anySettings));
    }

    SECTION("degree=0 (unassigned) rejects all templates") {
        Note n = makeNote(NoteName::C);  // degree stays 0 — not yet resolved
        REQUIRE_FALSE(builder.canUseTemplate(t53_I, n, HarmonizationMode::HarmonizeMelody, anySettings));
        REQUIRE_FALSE(builder.canUseTemplate(d53,   n, HarmonizationMode::HarmonizeMelody, anySettings));
        REQUIRE_FALSE(builder.canUseTemplate(d7,    n, HarmonizationMode::HarmonizeMelody, anySettings));
    }

    SECTION("degree is pre-resolved: C with key=a still matches T53-I if degree=1") {
        // Even in the wrong key context, pre-set degree=1 must match soprano position.
        HarmonizationSettings wrongKey = makeSettings("a");
        Note n = withDegree(NoteName::C, 1);
        REQUIRE(builder.canUseTemplate(t53_I, n, HarmonizationMode::HarmonizeMelody, wrongKey));
    }

    SECTION("T53-I with degree=1: both melody and bass modes accept (degree is in soprano AND bass)") {
        // T53-I has {1, 5, 3, 1}: soprano=1 and bass=1, so degree=1 matches both modes.
        Note n = withDegree(NoteName::C, 1);
        bool melody = builder.canUseTemplate(t53_I, n, HarmonizationMode::HarmonizeMelody, anySettings);
        bool bass   = builder.canUseTemplate(t53_I, n, HarmonizationMode::HarmonizeBass,   anySettings);
        REQUIRE(melody);
        REQUIRE(bass);
    }
}

// ── buildForFixed*Note uses pre-resolved degree ───────────────────────────────

TEST_CASE("buildForFixedMelodyNote uses pre-resolved degree", "[ChordBuilder][melody]") {
    ChordBuilder builder;
    HarmonizationSettings settings = makeSettings("C");

    SECTION("degree=1 → chords produced") {
        Note n = makeNote(NoteName::C);
        n.setDegree(1);
        REQUIRE(!builder.buildForFixedMelodyNote(n, settings).empty());
    }

    SECTION("degree=3 → chords produced") {
        Note n = makeNote(NoteName::E);
        n.setDegree(3);
        REQUIRE(!builder.buildForFixedMelodyNote(n, settings).empty());
    }

    SECTION("degree=5 → chords produced") {
        Note n = makeNote(NoteName::G);
        n.setDegree(5);
        REQUIRE(!builder.buildForFixedMelodyNote(n, settings).empty());
    }

    SECTION("degree=0 (unassigned) → no chords, even for C in C major") {
        Note n = makeNote(NoteName::C);  // pitch is in C major but degree not resolved
        REQUIRE(builder.buildForFixedMelodyNote(n, settings).empty());
    }

    SECTION("degree=-1 (chromatic) → no chords") {
        Note n = makeNote(NoteName::F, 1);  // F#
        n.setDegree(-1);
        REQUIRE(builder.buildForFixedMelodyNote(n, settings).empty());
    }
}

TEST_CASE("buildForFixedBassNote uses pre-resolved degree", "[ChordBuilder][bass]") {
    ChordBuilder builder;
    HarmonizationSettings settings = makeSettings("C");

    SECTION("degree=1 → chords produced") {
        Note n = makeNote(NoteName::C);
        n.setDegree(1);
        REQUIRE(!builder.buildForFixedBassNote(n, settings).empty());
    }

    SECTION("degree=0 (unassigned) → no chords") {
        Note n = makeNote(NoteName::C);
        REQUIRE(builder.buildForFixedBassNote(n, settings).empty());
    }

    SECTION("degree=-1 (chromatic) → no chords") {
        Note n = makeNote(NoteName::F, 1);
        n.setDegree(-1);
        REQUIRE(builder.buildForFixedBassNote(n, settings).empty());
    }
}

// ── C major ───────────────────────────────────────────────────────────────────

TEST_CASE("C major: all 7 diatonic notes produce chords", "[ScaleDegree][major]") {
    REQUIRE(hasChords(makeNote(NoteName::C), "C"));  // degree 1
    REQUIRE(hasChords(makeNote(NoteName::D), "C"));  // degree 2
    REQUIRE(hasChords(makeNote(NoteName::E), "C"));  // degree 3
    REQUIRE(hasChords(makeNote(NoteName::F), "C"));  // degree 4
    REQUIRE(hasChords(makeNote(NoteName::G), "C"));  // degree 5
    REQUIRE(hasChords(makeNote(NoteName::A), "C"));  // degree 6
    REQUIRE(hasChords(makeNote(NoteName::B), "C"));  // degree 7
}

TEST_CASE("C major: truly chromatic notes produce no chords", "[ScaleDegree][major]") {
    REQUIRE_FALSE(hasChords(makeNote(NoteName::F,  1), "C"));  // F# (tritone)
    REQUIRE_FALSE(hasChords(makeNote(NoteName::C,  1), "C"));  // C#
    REQUIRE_FALSE(hasChords(makeNote(NoteName::G,  1), "C"));  // G#
}

TEST_CASE("C major: lowered VI and VII produce chords", "[ScaleDegree][major]") {
    REQUIRE(hasChords(makeNote(NoteName::A, -1), "C"));  // Ab — ♭VI
    REQUIRE(hasChords(makeNote(NoteName::B, -1), "C"));  // Bb — ♭VII
}

TEST_CASE("G major: lowered VI and VII produce chords", "[ScaleDegree][major]") {
    REQUIRE(hasChords(makeNote(NoteName::E, -1), "G"));  // Eb — ♭VI
    REQUIRE(hasChords(makeNote(NoteName::F,  0), "G"));  // F  — ♭VII (F natural is ♭VII in G major)
}

// ── A minor ───────────────────────────────────────────────────────────────────

TEST_CASE("A natural minor: all 7 diatonic notes produce chords", "[ScaleDegree][minor]") {
    REQUIRE(hasChords(makeNote(NoteName::A), "a"));  // degree 1
    REQUIRE(hasChords(makeNote(NoteName::B), "a"));  // degree 2
    REQUIRE(hasChords(makeNote(NoteName::C), "a"));  // degree 3
    REQUIRE(hasChords(makeNote(NoteName::D), "a"));  // degree 4
    REQUIRE(hasChords(makeNote(NoteName::E), "a"));  // degree 5
    REQUIRE(hasChords(makeNote(NoteName::F), "a"));  // degree 6 (natural)
    REQUIRE(hasChords(makeNote(NoteName::G), "a"));  // degree 7 (natural)
}

TEST_CASE("A minor: harmonic and melodic raised notes produce chords", "[ScaleDegree][minor]") {
    REQUIRE(hasChords(makeNote(NoteName::G, 1), "a"));  // G# - raised VII (harmonic/melodic)
    REQUIRE(hasChords(makeNote(NoteName::F, 1), "a"));  // F# - raised VI  (melodic)
}

TEST_CASE("A minor: chromatic notes produce no chords", "[ScaleDegree][minor]") {
    REQUIRE_FALSE(hasChords(makeNote(NoteName::A,  1), "a"));  // A#
    REQUIRE_FALSE(hasChords(makeNote(NoteName::C,  1), "a"));  // C#
    REQUIRE_FALSE(hasChords(makeNote(NoteName::D,  1), "a"));  // D#
}

// ── G# minor (bug fix: key was missing from keyToRootSemitone) ───────────────

TEST_CASE("G# minor: diatonic notes produce chords (missing key fix)", "[ScaleDegree][minor][keybug]") {
    // G# natural minor: G#, A#, B, C#, D#, E, F#
    REQUIRE(hasChords(makeNote(NoteName::G,  1), "g#"));  // G# - degree 1
    REQUIRE(hasChords(makeNote(NoteName::A,  1), "g#"));  // A# - degree 2
    REQUIRE(hasChords(makeNote(NoteName::B),     "g#"));  // B  - degree 3
    REQUIRE(hasChords(makeNote(NoteName::C,  1), "g#"));  // C# - degree 4
    REQUIRE(hasChords(makeNote(NoteName::D,  1), "g#"));  // D# - degree 5
    REQUIRE(hasChords(makeNote(NoteName::E),     "g#"));  // E  - degree 6
    REQUIRE(hasChords(makeNote(NoteName::F,  1), "g#"));  // F# - degree 7
}

// ── D# minor (bug fix: key was missing from keyToRootSemitone) ───────────────

TEST_CASE("D# minor tonic and diatonic notes produce chords (missing key fix)", "[ScaleDegree][minor][keybug]") {
    // D# natural minor: D#, E#, F#, G#, A#, B, C#
    REQUIRE(hasChords(makeNote(NoteName::D,  1), "d#"));  // D# - degree 1
    REQUIRE(hasChords(makeNote(NoteName::E,  1), "d#"));  // E# - degree 2 (not E natural!)
    REQUIRE(hasChords(makeNote(NoteName::B),    "d#"));   // B  - degree 6
}

// ── A# minor (bug fix: key was missing from keyToRootSemitone) ───────────────

TEST_CASE("A# minor tonic and diatonic notes produce chords (missing key fix)", "[ScaleDegree][minor][keybug]") {
    // A# natural minor: A#, B#, C#, D#, E#, F#, G#
    REQUIRE(hasChords(makeNote(NoteName::A,  1), "a#"));  // A#  - degree 1
    REQUIRE(hasChords(makeNote(NoteName::B,  1), "a#"));  // B#  - degree 2 (not B natural!)
    REQUIRE(hasChords(makeNote(NoteName::G,  1), "a#"));  // G#  - degree 7
}

// ── scaleMode filtering: A minor ─────────────────────────────────────────────

TEST_CASE("A minor, scaleMode=[natural]: only natural-minor notes are diatonic", "[ScaleDegree][minor][scaleMode]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettingsWithModes("a", {"natural"});

    REQUIRE(calc.calculate(makeNote(NoteName::A),     s) == 1);   // A  → 1
    REQUIRE(calc.calculate(makeNote(NoteName::F),     s) == 6);   // F  → 6 (natural ♭6)
    REQUIRE(calc.calculate(makeNote(NoteName::G),     s) == 7);   // G  → 7 (natural ♭7)
    REQUIRE(calc.calculate(makeNote(NoteName::F, 1),  s) == -1);  // F# → not in natural minor
    REQUIRE(calc.calculate(makeNote(NoteName::G, 1),  s) == -1);  // G# → not in natural minor
}

TEST_CASE("A minor, scaleMode=[harmonic]: only harmonic-minor notes are diatonic", "[ScaleDegree][minor][scaleMode]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettingsWithModes("a", {"harmonic"});

    REQUIRE(calc.calculate(makeNote(NoteName::A),     s) == 1);   // A  → 1
    REQUIRE(calc.calculate(makeNote(NoteName::F),     s) == 6);   // F  → 6 (♭6 stays)
    REQUIRE(calc.calculate(makeNote(NoteName::G),     s) == -1);  // G  → not in harmonic minor
    REQUIRE(calc.calculate(makeNote(NoteName::G, 1),  s) == 7);   // G# → 7 (raised ♮7)
    REQUIRE(calc.calculate(makeNote(NoteName::F, 1),  s) == -1);  // F# → not in harmonic minor
}

TEST_CASE("A minor, scaleMode=[melodic]: only melodic-minor notes are diatonic", "[ScaleDegree][minor][scaleMode]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettingsWithModes("a", {"melodic"});

    REQUIRE(calc.calculate(makeNote(NoteName::A),     s) == 1);   // A  → 1
    REQUIRE(calc.calculate(makeNote(NoteName::F),     s) == -1);  // F  → not in melodic minor
    REQUIRE(calc.calculate(makeNote(NoteName::F, 1),  s) == 6);   // F# → 6 (raised ♮6)
    REQUIRE(calc.calculate(makeNote(NoteName::G),     s) == -1);  // G  → not in melodic minor
    REQUIRE(calc.calculate(makeNote(NoteName::G, 1),  s) == 7);   // G# → 7 (raised ♮7)
}

TEST_CASE("A minor, scaleMode=[natural,harmonic,melodic]: union of all variants", "[ScaleDegree][minor][scaleMode]") {
    ScaleDegreeCalculator calc;
    auto s = makeSettingsWithModes("a", {"natural", "harmonic", "melodic"});

    REQUIRE(calc.calculate(makeNote(NoteName::F),     s) == 6);  // F  → 6 (natural ♭6, first match)
    REQUIRE(calc.calculate(makeNote(NoteName::F, 1),  s) == 6);  // F# → 6 (melodic ♮6)
    REQUIRE(calc.calculate(makeNote(NoteName::G),     s) == 7);  // G  → 7 (natural ♭7, first match)
    REQUIRE(calc.calculate(makeNote(NoteName::G, 1),  s) == 7);  // G# → 7 (harmonic/melodic ♮7)
}

// ── scaleMode does not affect major keys ─────────────────────────────────────

TEST_CASE("C major: scaleMode is ignored, degree behaviour unchanged", "[ScaleDegree][major][scaleMode]") {
    ScaleDegreeCalculator calc;

    for (const auto& modes : std::vector<std::vector<std::string>>{
            {"natural"}, {"harmonic"}, {"melodic"}, {"natural", "harmonic", "melodic"}, {}}) {
        auto s = makeSettingsWithModes("C", modes);
        REQUIRE(calc.calculate(makeNote(NoteName::C),     s) == 1);   // C  → 1
        REQUIRE(calc.calculate(makeNote(NoteName::F, 1),  s) == -1);  // F# → chromatic
        REQUIRE(calc.calculate(makeNote(NoteName::A, -1), s) == 6);   // Ab → ♭VI, degree 6
        REQUIRE(calc.calculate(makeNote(NoteName::B, -1), s) == 7);   // Bb → ♭VII, degree 7
    }
}
