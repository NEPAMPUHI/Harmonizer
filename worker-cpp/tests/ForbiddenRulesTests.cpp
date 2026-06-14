#include <catch2/catch_test_macros.hpp>
#include "domain/ActiveRuleSet.h"
#include "domain/HarmonyRules.h"
#include "domain/HarmonizationSettings.h"
#include "checking/CheckSolutionRuleChecker.h"
#include "checking/CheckHarmonicPosition.h"
#include "checking/IdentifiedCheckChord.h"
#include "domain/Chord.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static Note pn(NoteName name, int oct, int alter = 0) {
    return Note(name, oct, alter, 1, 4, false);
}

static ChordTemplate makeTmpl(HarmonicFunction fn, int degree, ChordType type) {
    ChordTemplate t;
    t.function           = fn;
    t.degree             = degree;
    t.type               = type;
    t.inversion          = Inversion::I;
    t.position           = ChordPosition::Close;
    t.degreesInSatbOrder = {1, 3, 5, 1};
    return t;
}

// T-1-53 template (C major triad, root position)
static const ChordTemplate T53 = makeTmpl(HarmonicFunction::T, 1, ChordType::Triad);

// D-5-53 template
static const ChordTemplate D53 = makeTmpl(HarmonicFunction::D, 5, ChordType::Triad);

// S-4-53 template
static const ChordTemplate S53 = makeTmpl(HarmonicFunction::S, 4, ChordType::Triad);

// Build an IdentifiedCheckChord (all voices pitched, isKnownChord=true) with
// a generic T-1-53 template so that no single-chord diagnostics fire for it.
static IdentifiedCheckChord knownIC(const Note& s, const Note& a,
                                     const Note& t, const Note& b,
                                     const ChordTemplate& tmpl = T53) {
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

static HarmonizationSettings settingsWithRules(const std::vector<std::string>& ids) {
    HarmonizationSettings s;
    s.key = "C";
    s.scaleModes = {"natural"};
    s.forbiddenRules = ids;
    return s;
}

// ── ActiveRuleSet::fromSettings ───────────────────────────────────────────────

TEST_CASE("ActiveRuleSet: fromSettings with all IDs → all flags true", "[ActiveRuleSet]") {
    const auto s = settingsWithRules({
        "parallel_fifths", "parallel_octaves", "parallel_seconds",
        "all_voices_same_dir", "voice_crossing", "chromatic_transfer",
        "hidden_octaves", "bass_leap_sequence", "large_interval_sa_at", "s_after_d"
    });
    const auto rs = ActiveRuleSet::fromSettings(s);

    CHECK(rs.parallelFifths);
    CHECK(rs.parallelOctaves);
    CHECK(rs.parallelSeconds);
    CHECK(rs.allVoicesSameDir);
    CHECK(rs.voiceCrossing);
    CHECK(rs.chromaticTransfer);
    CHECK(rs.hiddenIntervals);
    CHECK(rs.bassLeapSequence);
    CHECK(rs.largeIntervalSaAt);
    CHECK(rs.functionalRules);
}

TEST_CASE("ActiveRuleSet: fromSettings with empty list → all flags false", "[ActiveRuleSet]") {
    const auto rs = ActiveRuleSet::fromSettings(settingsWithRules({}));

    CHECK_FALSE(rs.parallelFifths);
    CHECK_FALSE(rs.parallelOctaves);
    CHECK_FALSE(rs.parallelSeconds);
    CHECK_FALSE(rs.allVoicesSameDir);
    CHECK_FALSE(rs.voiceCrossing);
    CHECK_FALSE(rs.chromaticTransfer);
    CHECK_FALSE(rs.hiddenIntervals);
    CHECK_FALSE(rs.bassLeapSequence);
    CHECK_FALSE(rs.largeIntervalSaAt);
    CHECK_FALSE(rs.functionalRules);
}

TEST_CASE("ActiveRuleSet: allEnabled has every flag true", "[ActiveRuleSet]") {
    const auto rs = ActiveRuleSet::allEnabled();
    CHECK(rs.parallelFifths);
    CHECK(rs.functionalRules);
    CHECK(rs.bassLeapSequence);
}

TEST_CASE("ActiveRuleSet: allDisabled has every flag false", "[ActiveRuleSet]") {
    const auto rs = ActiveRuleSet::allDisabled();
    CHECK_FALSE(rs.parallelFifths);
    CHECK_FALSE(rs.functionalRules);
    CHECK_FALSE(rs.largeIntervalSaAt);
}

// ── HarmonyRules: parallel_fifths toggle in isValidConnection ─────────────────
//
// Transition: E5/G4/E4/C3 → D5/A4/F4/D3
//
// A-B parallel P5 analysis:
//   Prev: A=G4(55), B=C3(36). diff=19. 19%12=7. Diatonic G4=33, C3=22. diff=11. 11%7+1=5. P5.
//   Curr: A=A4(57), B=D3(38). diff=19. 19%12=7. Diatonic A4=34, D3=23. diff=11. 11%7+1=5. P5.
//   Both A (G4→A4) and B (C3→D3) ascend → parallel perfect fifths.
//
// No other voice-leading violation: S descends (E5→D5) while A, T, B ascend,
// so not all-same-direction. No voice crossing. No chromatic transfer. S-B
// moves in opposite directions → no hidden interval. T-B: P5→P8 (different
// number). S-A: M3→M3, but S moves down while A moves up → not parallel.
// Functional: T→T triad, no functional rule violated.

TEST_CASE("HarmonyRules: parallel fifths blocked with rule enabled", "[ForbiddenRules]") {
    const Chord prev(pn(NoteName::E, 5), pn(NoteName::G, 4),
                     pn(NoteName::E, 4), pn(NoteName::C, 3), T53);
    const Chord curr(pn(NoteName::D, 5), pn(NoteName::A, 4),
                     pn(NoteName::F, 4), pn(NoteName::D, 3), T53);

    CHECK_FALSE(HarmonyRules::isValidConnection(prev, curr, ActiveRuleSet::allEnabled()));
}

TEST_CASE("HarmonyRules: parallel fifths allowed when rule disabled", "[ForbiddenRules]") {
    const Chord prev(pn(NoteName::E, 5), pn(NoteName::G, 4),
                     pn(NoteName::E, 4), pn(NoteName::C, 3), T53);
    const Chord curr(pn(NoteName::D, 5), pn(NoteName::A, 4),
                     pn(NoteName::F, 4), pn(NoteName::D, 3), T53);

    ActiveRuleSet rules = ActiveRuleSet::allEnabled();
    rules.parallelFifths = false;
    CHECK(HarmonyRules::isValidConnection(prev, curr, rules));
}

// ── HarmonyRules: s_after_d (functionalRules) toggle in isValidConnection ────
//
// Transition: D53 (B4/G4/D4/G3) → S53 (C5/A4/F4/F3)
//
// Normally blocked by checkAfterDominant(): current.function == S → false.
// All voice-leading rules pass (verified manually):
//   voiceCrossing ✓, not-all-same-dir (B↓, others↑) ✓, no chromatic ✓,
//   hidden (S↑, B↓ = opposite dirs) ✓, no parallel fifths (T-B: P5→P8
//   different directions) ✓, augmented in bass: G3→F3 = M2 ✓.

TEST_CASE("HarmonyRules: S-after-D blocked with functional rules enabled", "[ForbiddenRules]") {
    const Chord d(pn(NoteName::B, 4), pn(NoteName::G, 4),
                  pn(NoteName::D, 4), pn(NoteName::G, 3), D53);
    const Chord s(pn(NoteName::C, 5), pn(NoteName::A, 4),
                  pn(NoteName::F, 4), pn(NoteName::F, 3), S53);

    CHECK_FALSE(HarmonyRules::isValidConnection(d, s, ActiveRuleSet::allEnabled()));
}

TEST_CASE("HarmonyRules: S-after-D allowed when s_after_d rule disabled", "[ForbiddenRules]") {
    const Chord d(pn(NoteName::B, 4), pn(NoteName::G, 4),
                  pn(NoteName::D, 4), pn(NoteName::G, 3), D53);
    const Chord s(pn(NoteName::C, 5), pn(NoteName::A, 4),
                  pn(NoteName::F, 4), pn(NoteName::F, 3), S53);

    ActiveRuleSet rules = ActiveRuleSet::allEnabled();
    rules.functionalRules = false;
    CHECK(HarmonyRules::isValidConnection(d, s, rules));
}

// ── HarmonyRules: large_interval_sa_at toggle in isValidChord ────────────────
//
// Chord: S=C5, A=A3, T=C4, B=C3
// S-A interval: C5(diatonic=36) – A3(diatonic=27) → number=10 > 8 → fails.
// A-T: A3(27) – C4(29) → number=3 ≤ 8 ✓.  T-B: C4–C3 → number=8 ≤ 15 ✓.

TEST_CASE("HarmonyRules: S-A > octave rejected with large_interval_sa_at enabled",
          "[ForbiddenRules]") {
    const ChordTemplate tmpl = makeTmpl(HarmonicFunction::T, 1, ChordType::Triad);
    const Chord chord(pn(NoteName::C, 5), pn(NoteName::A, 3),
                      pn(NoteName::C, 4), pn(NoteName::C, 3), tmpl);

    CHECK_FALSE(HarmonyRules::isValidChord(chord, ActiveRuleSet::allEnabled()));
}

TEST_CASE("HarmonyRules: S-A > octave allowed when large_interval_sa_at disabled",
          "[ForbiddenRules]") {
    const ChordTemplate tmpl = makeTmpl(HarmonicFunction::T, 1, ChordType::Triad);
    const Chord chord(pn(NoteName::C, 5), pn(NoteName::A, 3),
                      pn(NoteName::C, 4), pn(NoteName::C, 3), tmpl);

    ActiveRuleSet rules = ActiveRuleSet::allEnabled();
    rules.largeIntervalSaAt = false;
    CHECK(HarmonyRules::isValidChord(chord, rules));
}

// ── CheckSolutionRuleChecker: parallel_fifths toggle ─────────────────────────
//
// ic1: E5/A4/G3/C3  (T-B = G3-C3 = P5)
// ic2: F5/B4/A3/D3  (T-B = A3-D3 = P5, both T and B ascend → parallel P5)
// AllVoicesSameDirection may also fire when the rule is active, so we filter.

TEST_CASE("Checker: parallel_fifths enabled → ParallelFifths error fires",
          "[ForbiddenRules][checker]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::E, 5), pn(NoteName::A, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));
    const auto ic2 = knownIC(pn(NoteName::F, 5), pn(NoteName::B, 4),
                              pn(NoteName::A, 3), pn(NoteName::D, 3));

    ActiveRuleSet rules = ActiveRuleSet::allEnabled();
    const auto errors = checker.check({ic1, ic2}, rules);

    bool found = false;
    for (const auto& e : errors)
        if (e.code == CheckErrorCode::ParallelFifths) found = true;
    CHECK(found);
}

TEST_CASE("Checker: parallel_fifths disabled → no ParallelFifths error",
          "[ForbiddenRules][checker]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::E, 5), pn(NoteName::A, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));
    const auto ic2 = knownIC(pn(NoteName::F, 5), pn(NoteName::B, 4),
                              pn(NoteName::A, 3), pn(NoteName::D, 3));

    ActiveRuleSet rules = ActiveRuleSet::allEnabled();
    rules.parallelFifths = false;
    const auto errors = checker.check({ic1, ic2}, rules);

    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::ParallelFifths);
}

// ── CheckSolutionRuleChecker: s_after_d (functionalRules) toggle ──────────────
//
// ic1: D53  B4/G4/D4/G3 (D, degree=5, Triad)
// ic2: S53  C5/A4/F4/F3 (S, degree=4, Triad)
//
// D→S is blocked by checkAfterDominant → FunctionalProgressionError.
// All other voice-leading checks pass for this transition (see HarmonyRules
// tests above for the same chord pair).

TEST_CASE("Checker: s_after_d disabled → no FunctionalProgressionError",
          "[ForbiddenRules][checker]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::B, 4), pn(NoteName::G, 4),
                              pn(NoteName::D, 4), pn(NoteName::G, 3), D53);
    const auto ic2 = knownIC(pn(NoteName::C, 5), pn(NoteName::A, 4),
                              pn(NoteName::F, 4), pn(NoteName::F, 3), S53);

    ActiveRuleSet rules = ActiveRuleSet::allEnabled();
    rules.functionalRules = false;
    const auto errors = checker.check({ic1, ic2}, rules);

    for (const auto& e : errors)
        CHECK(e.code != CheckErrorCode::FunctionalProgressionError);
}

// ── CheckSolutionRuleChecker: backwards-compat overload ───────────────────────
//
// The parameterless check() must behave identically to check(allEnabled()).

TEST_CASE("Checker: parameterless check() equals check(allEnabled)",
          "[ForbiddenRules][checker]") {
    CheckSolutionRuleChecker checker;

    const auto ic1 = knownIC(pn(NoteName::E, 5), pn(NoteName::A, 4),
                              pn(NoteName::G, 3), pn(NoteName::C, 3));
    const auto ic2 = knownIC(pn(NoteName::F, 5), pn(NoteName::B, 4),
                              pn(NoteName::A, 3), pn(NoteName::D, 3));

    const auto e1 = checker.check({ic1, ic2});
    const auto e2 = checker.check({ic1, ic2}, ActiveRuleSet::allEnabled());

    REQUIRE(e1.size() == e2.size());
    for (std::size_t i = 0; i < e1.size(); ++i)
        CHECK(e1[i].code == e2[i].code);
}
