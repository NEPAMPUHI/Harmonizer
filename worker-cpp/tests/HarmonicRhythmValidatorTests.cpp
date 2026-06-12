#include <catch2/catch_test_macros.hpp>
#include "harmonization/HarmonicRhythmValidator.h"
#include "domain/HarmonicSegment.h"
#include "domain/HarmonizationSettings.h"
#include "domain/Note.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static Note qNote(int durationSixteenths = 4,
                  bool tiedToNext = false,
                  bool rest       = false) {
    return Note(NoteName::C, 4, 0, 1, durationSixteenths, true, tiedToNext, rest);
}

static HarmonicSegment makeSeg(int sourceNoteIndex,
                                int offsetInFixed,
                                int durationSixteenths,
                                bool isRest = false) {
    HarmonicSegment s;
    s.sourceNoteIndex             = sourceNoteIndex;
    s.offsetInFixedNoteSixteenths = offsetInFixed;
    s.durationSixteenths          = durationSixteenths;
    s.isRest                      = isRest;
    return s;
}

static HarmonicRhythmPlan makePlan(std::vector<HarmonicSegment> segs) {
    HarmonicRhythmPlan p;
    p.segments = std::move(segs);
    return p;
}

static HarmonizationSettings settings44(int anacrusis = 0, int measureCount = 1) {
    HarmonizationSettings s;
    s.timeSignature          = {4, 4};
    s.anacrusisSixteenths    = anacrusis;
    s.measureCount           = measureCount;
    s.key                    = "C";
    s.scaleModes             = {"major"};
    s.maxHarmonicRhythmPlans = 16;
    return s;
}

// ── Valid cases ───────────────────────────────────────────────────────────────

TEST_CASE("HarmonicRhythmValidator: empty plan with empty notes is valid", "[validator]") {
    HarmonicRhythmValidator v;
    REQUIRE(v.validate({}, {}, settings44()).ok);
}

TEST_CASE("HarmonicRhythmValidator: single quarter note passes", "[validator]") {
    HarmonicRhythmValidator v;
    auto r = v.validate(makePlan({makeSeg(0, 0, 4)}), {qNote(4)}, settings44());
    REQUIRE(r.ok);
}

TEST_CASE("HarmonicRhythmValidator: multiple quarter notes pass", "[validator]") {
    HarmonicRhythmValidator v;
    auto plan  = makePlan({makeSeg(0, 0, 4), makeSeg(1, 0, 4), makeSeg(2, 0, 4), makeSeg(3, 0, 4)});
    auto notes = std::vector<Note>{qNote(4), qNote(4), qNote(4), qNote(4)};
    REQUIRE(v.validate(plan, notes, settings44()).ok);
}

TEST_CASE("HarmonicRhythmValidator: whole note split [4,4,8] passes", "[validator]") {
    HarmonicRhythmValidator v;
    auto plan  = makePlan({makeSeg(0, 0, 4), makeSeg(0, 4, 4), makeSeg(0, 8, 8)});
    auto notes = std::vector<Note>{qNote(16)};
    REQUIRE(v.validate(plan, notes, settings44()).ok);
}

TEST_CASE("HarmonicRhythmValidator: whole note split [4,4,4,4] passes", "[validator]") {
    HarmonicRhythmValidator v;
    auto plan  = makePlan({makeSeg(0, 0, 4), makeSeg(0, 4, 4), makeSeg(0, 8, 4), makeSeg(0, 12, 4)});
    auto notes = std::vector<Note>{qNote(16)};
    REQUIRE(v.validate(plan, notes, settings44()).ok);
}

TEST_CASE("HarmonicRhythmValidator: tied half notes treated as one span", "[validator]") {
    HarmonicRhythmValidator v;
    // Two tied half notes → tie span totalDuration=16, firstNoteIndex=0.
    Note n1(NoteName::C, 4, 0, 1, 8, true, /*tiedToNext=*/true);
    Note n2(NoteName::C, 4, 0, 1, 8, true, /*tiedToNext=*/false);
    auto plan = makePlan({makeSeg(0, 0, 8), makeSeg(0, 8, 8)});
    REQUIRE(v.validate(plan, {n1, n2}, settings44()).ok);
}

TEST_CASE("HarmonicRhythmValidator: rest segment is valid", "[validator]") {
    HarmonicRhythmValidator v;
    auto plan  = makePlan({makeSeg(0, 0, 4, /*isRest=*/true)});
    auto notes = std::vector<Note>{qNote(4, false, /*rest=*/true)};
    REQUIRE(v.validate(plan, notes, settings44()).ok);
}

TEST_CASE("HarmonicRhythmValidator: anacrusis + full 4/4 measure is valid", "[validator]") {
    HarmonicRhythmValidator v;
    Note n1(NoteName::C, 4, 0, 1, 4,  true);  // anacrusis quarter
    Note n2(NoteName::D, 4, 0, 2, 16, true);  // full-measure whole note
    auto plan = makePlan({makeSeg(0, 0, 4), makeSeg(1, 0, 16)});
    REQUIRE(v.validate(plan, {n1, n2}, settings44(/*anacrusis=*/4, /*measures=*/2)).ok);
}

TEST_CASE("HarmonicRhythmValidator: unsupported beat type skips measure check", "[validator]") {
    HarmonicRhythmValidator v;
    HarmonizationSettings s;
    s.timeSignature          = {4, 3};  // beatType=3 does not divide 16
    s.anacrusisSixteenths    = 0;
    s.measureCount           = 1;
    s.key                    = "C";
    s.scaleModes             = {"major"};
    s.maxHarmonicRhythmPlans = 16;
    // Measure check is skipped; other checks pass.
    REQUIRE(v.validate(makePlan({makeSeg(0, 0, 4)}), {qNote(4)}, s).ok);
}

// ── Invalid: positive durations ───────────────────────────────────────────────

TEST_CASE("HarmonicRhythmValidator: segment duration zero fails", "[validator]") {
    HarmonicRhythmValidator v;
    auto r = v.validate(makePlan({makeSeg(0, 0, 0)}), {qNote(4)}, settings44());
    REQUIRE(!r.ok);
    REQUIRE_FALSE(r.message.empty());
}

TEST_CASE("HarmonicRhythmValidator: segment duration negative fails", "[validator]") {
    HarmonicRhythmValidator v;
    auto r = v.validate(makePlan({makeSeg(0, 0, -2)}), {qNote(4)}, settings44());
    REQUIRE(!r.ok);
}

// ── Invalid: total duration ───────────────────────────────────────────────────

TEST_CASE("HarmonicRhythmValidator: plan total longer than input fails", "[validator]") {
    HarmonicRhythmValidator v;
    // Plan = 8 sxths, input = 4 sxths.
    auto r = v.validate(makePlan({makeSeg(0, 0, 8)}), {qNote(4)}, settings44());
    REQUIRE(!r.ok);
}

TEST_CASE("HarmonicRhythmValidator: plan total shorter than input fails", "[validator]") {
    HarmonicRhythmValidator v;
    // Plan = 4 sxths, input = 8 sxths.
    auto r = v.validate(makePlan({makeSeg(0, 0, 4)}), {qNote(8)}, settings44());
    REQUIRE(!r.ok);
}

// ── Invalid: source note offsets ──────────────────────────────────────────────

TEST_CASE("HarmonicRhythmValidator: first segment offset non-zero fails", "[validator]") {
    HarmonicRhythmValidator v;
    // Source note = 8 sxths; segment starts at offset 4 instead of 0.
    // Total check: 8 == 8, so it passes; offset check catches this.
    auto r = v.validate(makePlan({makeSeg(0, 4, 8)}), {qNote(8)}, settings44());
    REQUIRE(!r.ok);
}

TEST_CASE("HarmonicRhythmValidator: gap between segment offsets fails", "[validator]") {
    HarmonicRhythmValidator v;
    // Whole note (16): split into [4, 4, 8] but with a gap — offsets [0, 8, 12].
    // Total: 4+4+8=16 == 16 ✓, but offset 8 != running 4.
    auto plan  = makePlan({makeSeg(0, 0, 4), makeSeg(0, 8, 4), makeSeg(0, 12, 8)});
    auto notes = std::vector<Note>{qNote(16)};
    auto r = v.validate(plan, notes, settings44());
    REQUIRE(!r.ok);
}

TEST_CASE("HarmonicRhythmValidator: overlapping segment offsets fail", "[validator]") {
    HarmonicRhythmValidator v;
    // Whole note (16): offsets [0, 4, 4] — offset 4 appears twice (overlap).
    // Total: 8+4+4=16 ✓, but sorted: [{0,8},{4,4},{4,4}] → running=8, off=4 < 8.
    auto plan  = makePlan({makeSeg(0, 0, 8), makeSeg(0, 4, 4), makeSeg(0, 4, 4)});
    auto notes = std::vector<Note>{qNote(16)};
    auto r = v.validate(plan, notes, settings44());
    REQUIRE(!r.ok);
}

TEST_CASE("HarmonicRhythmValidator: segments cover wrong amount per source note fails", "[validator]") {
    HarmonicRhythmValidator v;
    // Two quarter notes [dur=4, dur=4]; but segments give [dur=3, dur=5].
    // Total: 8 == 8 ✓, but source 0 expected 4 gets 3.
    auto plan  = makePlan({makeSeg(0, 0, 3), makeSeg(1, 0, 5)});
    auto notes = std::vector<Note>{qNote(4), qNote(4)};
    auto r = v.validate(plan, notes, settings44());
    REQUIRE(!r.ok);
}

// ── Invalid: measure integrity ────────────────────────────────────────────────

TEST_CASE("HarmonicRhythmValidator: segment overflows 4/4 measure fails", "[validator]") {
    HarmonicRhythmValidator v;
    // Dotted-half (12) + half (8) in 4/4 (capacity=16).
    // Segment 0 lands at position 0, fits (12 <= 16).
    // Segment 1 starts at position 12, remaining = 4, but duration = 8 > 4 → overflow.
    auto plan  = makePlan({makeSeg(0, 0, 12), makeSeg(1, 0, 8)});
    auto notes = std::vector<Note>{qNote(12), qNote(8)};
    auto r = v.validate(plan, notes, settings44(/*anacrusis=*/0, /*measures=*/2));
    REQUIRE(!r.ok);
}

TEST_CASE("HarmonicRhythmValidator: segment filling exact measure boundary is valid", "[validator]") {
    HarmonicRhythmValidator v;
    // Two consecutive whole notes: each fills exactly one 4/4 measure.
    auto plan  = makePlan({makeSeg(0, 0, 16), makeSeg(1, 0, 16)});
    auto notes = std::vector<Note>{qNote(16), qNote(16)};
    auto r = v.validate(plan, notes, settings44(/*anacrusis=*/0, /*measures=*/2));
    REQUIRE(r.ok);
}

// ── Error messages ────────────────────────────────────────────────────────────

TEST_CASE("HarmonicRhythmValidator: invalid result carries non-empty message", "[validator]") {
    HarmonicRhythmValidator v;
    auto r = v.validate(makePlan({makeSeg(0, 0, 8)}), {qNote(4)}, settings44());
    REQUIRE(!r.ok);
    REQUIRE(!r.message.empty());
}

TEST_CASE("HarmonicRhythmValidator: valid result has empty message", "[validator]") {
    HarmonicRhythmValidator v;
    auto r = v.validate(makePlan({makeSeg(0, 0, 4)}), {qNote(4)}, settings44());
    REQUIRE(r.ok);
    REQUIRE(r.message.empty());
}
