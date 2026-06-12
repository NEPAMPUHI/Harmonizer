#include "checking/CheckSolutionRuleChecker.h"
#include "domain/HarmonyRules.h"
#include <cstdlib>
#include <iostream>

// ── anonymous helpers ─────────────────────────────────────────────────────────
namespace {

static const VoiceType V[4] = {
    VoiceType::Soprano, VoiceType::Alto, VoiceType::Tenor, VoiceType::Bass
};

bool allVoicesPitched(const CheckHarmonicPosition& pos) {
    return pos.hasSoprano && pos.hasAlto && pos.hasTenor && pos.hasBass
        && !pos.soprano.isRest() && !pos.alto.isRest()
        && !pos.tenor.isRest()   && !pos.bass.isRest();
}

// ── single-chord helpers ──────────────────────────────────────────────────────

// Returns the voices (by index 0-3) that are outside their standard SATB range.
// Takes individual notes directly so it works for unknown chords too.
struct VoicePair { int a; int b; };

std::vector<int> outOfRangeVoices(const Note& s, const Note& a,
                                   const Note& t, const Note& b) {
    std::vector<int> bad;
    if (!SOPRANO_RANGE.contains(s)) bad.push_back(0);
    if (!ALTO_RANGE   .contains(a)) bad.push_back(1);
    if (!TENOR_RANGE  .contains(t)) bad.push_back(2);
    if (!BASS_RANGE   .contains(b)) bad.push_back(3);
    return bad;
}

// Returns pairs (by index) where adjacent voices exceed the allowed diatonic gap:
//   S-A or A-T > 8th, T-B > 15th (mirrors HarmonyRules::hasNoMoreThanOctave).
// Takes individual notes directly so it works for unknown chords too.
std::vector<VoicePair> wideSpacingPairs(const Note& s, const Note& a,
                                         const Note& t, const Note& b) {
    std::vector<VoicePair> bad;
    if (s.getInterval(a).number > 8)  bad.push_back({0, 1});
    if (a.getInterval(t).number > 8)  bad.push_back({1, 2});
    if (t.getInterval(b).number > 15) bad.push_back({2, 3});
    return bad;
}

// ── pairwise helpers ──────────────────────────────────────────────────────────

// Returns false if either voice stands still; true when both move in the same
// direction. Used by the legacy semitone-based parallel checks.
bool bothMoveInSameDir(const Note& p1, const Note& p2,
                       const Note& c1, const Note& c2) {
    if (c1 == p1 || c2 == p2) return false;
    return (c1 > p1) == (c2 > p2);
}

bool isParallelPerfectFifth(const Note& p1, const Note& p2,
                             const Note& c1, const Note& c2) {
    if (!bothMoveInSameDir(p1, p2, c1, c2)) return false;
    const Interval pi = p1.getSimpleInterval(p2);
    const Interval ci = c1.getSimpleInterval(c2);
    return pi.number == 5 && ci.number == 5
        && pi.quality == IntervalQuality::Perfect
        && ci.quality == IntervalQuality::Perfect;
}

bool isParallelPerfectOctave(const Note& p1, const Note& p2,
                              const Note& c1, const Note& c2) {
    if (!bothMoveInSameDir(p1, p2, c1, c2)) return false;
    const int pd = std::abs(p1.getSemitone() - p2.getSemitone());
    const int cd = std::abs(c1.getSemitone() - c2.getSemitone());
    return pd > 0 && pd % 12 == 0 && cd > 0 && cd % 12 == 0;
}

// HarmonyRules-style parallel-interval check (diatonic interval number).
// Mirrors hasParallelSimpleIntervalPair with the P5→dim5 exception for n==5.
bool isParallelDiatonicInterval(const Note& p1, const Note& p2,
                                 const Note& c1, const Note& c2, int n) {
    bool firstUp   = c1 > p1, firstDown  = c1 < p1;
    bool secondUp  = c2 > p2, secondDown = c2 < p2;
    bool bothUp    = firstUp   && secondUp;
    bool bothDown  = firstDown && secondDown;
    if (!bothUp && !bothDown) return false;

    Interval pi = p1.getSimpleInterval(p2);
    Interval ci = c1.getSimpleInterval(c2);
    if (pi.number != n || ci.number != n) return false;

    // Exception: P5 → dim5 is not a parallel fifth
    if (n == 5 && pi.quality == IntervalQuality::Perfect
               && ci.quality == IntervalQuality::Diminished)
        return false;

    return true;
}

// Finds all voice pairs (a < b) where the diatonic parallel condition holds.
std::vector<VoicePair> findParallelDiatonicPairs(const Note pN[4], const Note cN[4],
                                                   int n) {
    std::vector<VoicePair> pairs;
    for (int a = 0; a < 4; ++a)
        for (int b = a + 1; b < 4; ++b)
            if (isParallelDiatonicInterval(pN[a], pN[b], cN[a], cN[b], n))
                pairs.push_back({a, b});
    return pairs;
}

// Actual semitone interval between two notes (absolute, not diatonic).
int semitoneDist(const Note& a, const Note& b) {
    return std::abs(a.getSemitone() - b.getSemitone());
}

// True when all 4 voices move and all move in the same direction.
bool isAllVoicesSameDirection(const Note pN[4], const Note cN[4]) {
    int dir = 0;
    for (int v = 0; v < 4; ++v) {
        const int d = cN[v].getSemitone() - pN[v].getSemitone();
        if (d == 0) return false;
        const int vd = (d > 0) ? 1 : -1;
        if (v == 0) dir = vd;
        else if (vd != dir) return false;
    }
    return true;
}

// Returns pairs (i, j) where prev[i] chromatically transfers to curr[j]:
// same note name, different alteration.
std::vector<VoicePair> findChromaticTransferPairs(const Note pN[4], const Note cN[4]) {
    std::vector<VoicePair> pairs;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (i != j
                && pN[i].getName()  == cN[j].getName()
                && pN[i].getAlter() != cN[j].getAlter())
                pairs.push_back({i, j});
    return pairs;
}

// True when soprano and bass both move in the same direction.
bool bassSopranSameDir(const Note& pS, const Note& pB,
                       const Note& cS, const Note& cB) {
    bool sUp   = cS > pS, sDown = cS < pS;
    bool bUp   = cB > pB, bDown = cB < pB;
    return (sUp && bUp) || (sDown && bDown);
}

// ── triple-chord helpers ──────────────────────────────────────────────────────

bool strictlyAscending(const Note& a, const Note& b, const Note& c) {
    return a < b && b < c;
}
bool strictlyDescending(const Note& a, const Note& b, const Note& c) {
    return a > b && b > c;
}

bool hasConsecutiveBassInterval(const Note& b0, const Note& b1, const Note& b2,
                                 int intervalNumber) {
    bool first  = b0.getInterval(b1).number == intervalNumber;
    bool second = b1.getInterval(b2).number == intervalNumber;
    if (!first || !second) return false;
    return strictlyAscending(b0, b1, b2) || strictlyDescending(b0, b1, b2);
}

} // namespace

// ── checker ───────────────────────────────────────────────────────────────────

std::vector<CheckError>
CheckSolutionRuleChecker::check(const std::vector<IdentifiedCheckChord>& chords) const {
    return check(chords, ActiveRuleSet::allEnabled());
}

std::vector<CheckError>
CheckSolutionRuleChecker::check(const std::vector<IdentifiedCheckChord>& chords,
                                 const ActiveRuleSet& rules) const {
    std::vector<CheckError> errors;
    const int n = static_cast<int>(chords.size());

    // ── Single-chord loop ─────────────────────────────────────────────────────
    for (int i = 0; i < n; ++i) {
        const auto& ic  = chords[i];
        const auto& pos = ic.position;

        if (!allVoicesPitched(pos)) continue;

        // UnknownChord — always enforced; does NOT skip note-only checks below
        if (!ic.isKnownChord) {
            CheckError e;
            e.code          = CheckErrorCode::UnknownChord;
            e.message       = "Unknown chord";
            e.positionIndex = i;
            e.renderType    = CheckRenderType::ChordMarker;
            errors.push_back(e);
        }

        // VoiceRangeViolation — note-only, works for unknown chords
        if (rules.voiceRanges) {
            for (int v : outOfRangeVoices(pos.soprano, pos.alto, pos.tenor, pos.bass)) {
                CheckError e;
                e.code          = CheckErrorCode::VoiceRangeViolation;
                e.message       = "Voice out of range";
                e.positionIndex = i;
                e.voices        = {V[v]};
                e.renderType    = CheckRenderType::ChordMarker;
                errors.push_back(e);
            }
        }

        // MoreThanOctaveBetweenAdjacentVoices — note-only, works for unknown chords
        // S-A and A-T pairs: conditional on largeIntervalSaAt.
        // T-B pair: always enforced.
        for (auto [a, b] : wideSpacingPairs(pos.soprano, pos.alto, pos.tenor, pos.bass)) {
            if (a == 2 && b == 3) {
                if (!rules.voiceSpacingOctave) continue;
            } else if (!rules.largeIntervalSaAt) {
                continue;
            }
            CheckError e;
            e.code          = CheckErrorCode::MoreThanOctaveBetweenAdjacentVoices;
            e.message       = "More than an octave between adjacent voices";
            e.positionIndex = i;
            e.voices        = {V[a], V[b]};
            e.renderType    = CheckRenderType::VerticalBracket;
            errors.push_back(e);
        }
    }

    // ── Pairwise loop ─────────────────────────────────────────────────────────
    for (int i = 0; i + 1 < n; ++i) {
        const auto& prev = chords[i];
        const auto& curr = chords[i + 1];

        if (!allVoicesPitched(prev.position) || !allVoicesPitched(curr.position)) continue;

        // Use position notes directly — works for both known and unknown chords
        const Note pN[4] = {
            prev.position.soprano, prev.position.alto,
            prev.position.tenor,   prev.position.bass
        };
        const Note cN[4] = {
            curr.position.soprano, curr.position.alto,
            curr.position.tenor,   curr.position.bass
        };

        // VoiceCrossing
        if (rules.voiceCrossing) {
            const struct { int upper; int lower; } crossPairs[3] = {{0,1},{1,2},{2,3}};
            for (auto [up, lo] : crossPairs) {
                if (cN[up] < pN[lo]) {
                    CheckError e;
                    e.code              = CheckErrorCode::VoiceCrossing;
                    e.message           = "Voice crossing";
                    e.positionIndex     = i;
                    e.nextPositionIndex = i + 1;
                    e.voices            = {V[up], V[lo]};
                    e.renderType        = CheckRenderType::CrossingLines;
                    errors.push_back(e);
                }
            }
        }

        // AllVoicesSameDirection
        if (rules.allVoicesSameDir && isAllVoicesSameDirection(pN, cN)) {
            CheckError e;
            e.code              = CheckErrorCode::AllVoicesSameDirection;
            e.message           = "All voices move in the same direction";
            e.positionIndex     = i;
            e.nextPositionIndex = i + 1;
            e.voices            = {VoiceType::Soprano, VoiceType::Alto,
                                   VoiceType::Tenor,   VoiceType::Bass};
            e.renderType        = CheckRenderType::MotionLines;
            errors.push_back(e);
        }

        // ChromaticSemitoneTransfer
        if (rules.chromaticTransfer) {
            for (auto [pi, ci] : findChromaticTransferPairs(pN, cN)) {
                CheckError e;
                e.code              = CheckErrorCode::ChromaticSemitoneTransfer;
                e.message           = "Chromatic semitone transfer between voices";
                e.positionIndex     = i;
                e.nextPositionIndex = i + 1;
                e.voices            = {V[pi], V[ci]};
                e.renderType        = CheckRenderType::ChromaticTransfer;
                errors.push_back(e);
            }
        }

        // HiddenOctaves and HiddenFifths
        if (rules.hiddenIntervals) {
            const int sn = curr.position.bass.getSimpleInterval(curr.position.soprano).number;
            if (sn == 1 && bassSopranSameDir(pN[0], pN[3], cN[0], cN[3])) {
                CheckError e;
                e.code              = CheckErrorCode::HiddenOctaves;
                e.message           = "Hidden octaves between soprano and bass";
                e.positionIndex     = i;
                e.nextPositionIndex = i + 1;
                e.voices            = {VoiceType::Soprano, VoiceType::Bass};
                e.interval          = 8;
                e.renderType        = CheckRenderType::HiddenInterval;
                errors.push_back(e);
            }
            if (sn == 5 && bassSopranSameDir(pN[0], pN[3], cN[0], cN[3])) {
                CheckError e;
                e.code              = CheckErrorCode::HiddenFifths;
                e.message           = "Hidden fifths between soprano and bass";
                e.positionIndex     = i;
                e.nextPositionIndex = i + 1;
                e.voices            = {VoiceType::Soprano, VoiceType::Bass};
                e.interval          = 5;
                e.renderType        = CheckRenderType::HiddenInterval;
                errors.push_back(e);
            }
        }

        // ParallelFifths (semitone/quality-based)
        if (rules.parallelFifths) {
            for (int a = 0; a < 4; ++a) {
                for (int b = a + 1; b < 4; ++b) {
                    if (isParallelPerfectFifth(pN[a], pN[b], cN[a], cN[b])) {
                        std::cerr << "[check_solution] ParallelFifths pair("
                                  << i << "," << i + 1 << ") voices="
                                  << a << "/" << b << '\n';
                        CheckError e;
                        e.code              = CheckErrorCode::ParallelFifths;
                        e.message           = "Parallel fifths";
                        e.positionIndex     = i;
                        e.nextPositionIndex = i + 1;
                        e.voices            = {V[a], V[b]};
                        e.interval          = 5;
                        e.renderType        = CheckRenderType::VerticalBracketPair;
                        errors.push_back(e);
                    }
                }
            }
        }

        // ParallelOctaves (semitone-based)
        if (rules.parallelOctaves) {
            for (int a = 0; a < 4; ++a) {
                for (int b = a + 1; b < 4; ++b) {
                    if (isParallelPerfectOctave(pN[a], pN[b], cN[a], cN[b])) {
                        std::cerr << "[check_solution] ParallelOctaves pair("
                                  << i << "," << i + 1 << ") voices="
                                  << a << "/" << b << '\n';
                        CheckError e;
                        e.code              = CheckErrorCode::ParallelOctaves;
                        e.message           = "Parallel octaves";
                        e.positionIndex     = i;
                        e.nextPositionIndex = i + 1;
                        e.voices            = {V[a], V[b]};
                        e.interval          = 8;
                        e.renderType        = CheckRenderType::VerticalBracketPair;
                        errors.push_back(e);
                    }
                }
            }
        }

        // ParallelOctavesOrUnisons / ParallelSeconds / ParallelSevenths (diatonic)
        // Avoids vector allocation when the corresponding rule is disabled.
        const struct { int n; CheckErrorCode code; const char* msg; int interval; } diatonicRules[] = {
            {1, CheckErrorCode::ParallelOctavesOrUnisons, "Parallel octaves or unisons", 8},
            {2, CheckErrorCode::ParallelSeconds,          "Parallel seconds",            2},
            {7, CheckErrorCode::ParallelSevenths,         "Parallel sevenths",           7},
        };
        for (auto& rule : diatonicRules) {
            const bool ruleActive =
                (rule.n == 1) ? rules.parallelOctaves : rules.parallelSeconds;
            if (!ruleActive) continue;

            for (auto [a, b] : findParallelDiatonicPairs(pN, cN, rule.n)) {
                int reportInterval = rule.interval;
                if (rule.n == 1) {
                    int dist = semitoneDist(cN[a], cN[b]) % 12;
                    reportInterval = (dist == 0) ? 1 : 8;
                }
                CheckError e;
                e.code              = rule.code;
                e.message           = rule.msg;
                e.positionIndex     = i;
                e.nextPositionIndex = i + 1;
                e.voices            = {V[a], V[b]};
                e.interval          = reportInterval;
                e.renderType        = CheckRenderType::VerticalBracketPair;
                errors.push_back(e);
            }
        }

        // AugmentedIntervalInBass
        if (rules.augmentedBass) {
            Interval bassInterval = prev.position.bass.getInterval(curr.position.bass);
            if (bassInterval.quality == IntervalQuality::Augmented) {
                CheckError e;
                e.code              = CheckErrorCode::AugmentedIntervalInBass;
                e.message           = "Augmented interval in bass";
                e.positionIndex     = i;
                e.nextPositionIndex = i + 1;
                e.voices            = {VoiceType::Bass};
                e.renderType        = CheckRenderType::BassLineMarker;
                errors.push_back(e);
            }
        }

        // VoiceLeapGreaterThanCan: S and B > octave, A and T > fourth
        if (rules.voiceLeapLimits) {
            const int semitoneMax[4] = {12, 5, 5, 12};
            for (int v = 0; v < 4; ++v) {
                if (std::abs(pN[v].getSemitone() - cN[v].getSemitone()) > semitoneMax[v]) {
                    CheckError e;
                    e.code              = CheckErrorCode::VoiceLeapGreaterThanOctave;
                    e.message           = "Voice leap greater than allowed interval";
                    e.positionIndex     = i;
                    e.nextPositionIndex = i + 1;
                    e.voices            = {V[v]};
                    e.renderType        = CheckRenderType::MotionLines;
                    errors.push_back(e);
                }
            }
        }

        // FunctionalProgressionError — requires chord template, skip unknown chords
        if (rules.functionalRules
            && prev.isKnownChord && curr.isKnownChord
            && !HarmonyRules::isValidFunctionalProgression(prev.chord, curr.chord)) {
            CheckError e;
            e.code              = CheckErrorCode::FunctionalProgressionError;
            e.message           = "Invalid functional progression";
            e.positionIndex     = i;
            e.nextPositionIndex = i + 1;
            e.renderType        = CheckRenderType::FunctionalRelationMarker;
            errors.push_back(e);
        }
    }

    // ── Triple-chord loop (bass leap sequence) ────────────────────────────────
    if (rules.bassLeapSequence) {
        for (int i = 2; i < n; ++i) {
            const auto& pp = chords[i - 2];
            const auto& pv = chords[i - 1];
            const auto& cu = chords[i];

            if (!allVoicesPitched(pp.position)
                || !allVoicesPitched(pv.position)
                || !allVoicesPitched(cu.position)) continue;

            const Note b0 = pp.position.bass;
            const Note b1 = pv.position.bass;
            const Note b2 = cu.position.bass;

            if (hasConsecutiveBassInterval(b0, b1, b2, 4)) {
                CheckError e;
                e.code              = CheckErrorCode::ConsecutiveFourthsInBass;
                e.message           = "Two consecutive fourths in bass";
                e.positionIndex     = i - 2;
                e.nextPositionIndex = i;
                e.voices            = {VoiceType::Bass};
                e.renderType        = CheckRenderType::BassLineMarker;
                errors.push_back(e);
            }

            if (hasConsecutiveBassInterval(b0, b1, b2, 5)) {
                CheckError e;
                e.code              = CheckErrorCode::ConsecutiveFifthsInBass;
                e.message           = "Two consecutive fifths in bass";
                e.positionIndex     = i - 2;
                e.nextPositionIndex = i;
                e.voices            = {VoiceType::Bass};
                e.renderType        = CheckRenderType::BassLineMarker;
                errors.push_back(e);
            }
        }
    }

    return errors;
}
