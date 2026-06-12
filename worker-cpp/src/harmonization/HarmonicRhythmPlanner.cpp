#include "harmonization/HarmonicRhythmPlanner.h"
#include <algorithm>

namespace {

struct TieSpan {
    Note leadNote;
    int  totalDuration;
    int  firstNoteIndex;
};

bool samePitch(const Note& a, const Note& b) {
    return a.getName()   == b.getName()
        && a.getOctave() == b.getOctave()
        && a.getAlter()  == b.getAlter();
}

std::vector<TieSpan> buildTieSpans(const std::vector<Note>& notes) {
    std::vector<TieSpan> spans;
    int i = 0;
    const int n = static_cast<int>(notes.size());
    while (i < n) {
        TieSpan span;
        span.leadNote       = notes[i];
        span.firstNoteIndex = i;
        span.totalDuration  = notes[i].getDurationSixteenths();

        // Rests cannot participate in a tie chain.
        while (!notes[i].isRest()
            && notes[i].isTiedToNext()
            && (i + 1) < n
            && !notes[i + 1].isRest()
            && samePitch(notes[i], notes[i + 1]))
        {
            ++i;
            span.totalDuration += notes[i].getDurationSixteenths();
        }

        spans.push_back(std::move(span));
        ++i;
    }
    return spans;
}

// Beat unit in sixteenths derived from the time-signature denominator.
// Returns 0 for denominators that don't map cleanly (e.g. beatType 3).
int computeBasePulseSixteenths(const TimeSignature& ts) {
    if (ts.beatType <= 0)      return 0;
    if (16 % ts.beatType != 0) return 0;
    return 16 / ts.beatType;
}

HarmonicSplitPattern makeEqualSplitPattern(int duration, int pulse) {
    HarmonicSplitPattern p;
    const int count = duration / pulse;
    p.durationsSixteenths.reserve(count);
    for (int i = 0; i < count; ++i)
        p.durationsSixteenths.push_back(pulse);
    return p;
}

// Converts a span + a chosen split pattern into a flat list of HarmonicSegments.
std::vector<HarmonicSegment> makeSegments(const TieSpan& span,
                                          const HarmonicSplitPattern& pattern)
{
    std::vector<HarmonicSegment> segs;
    segs.reserve(pattern.durationsSixteenths.size());
    int offset = 0;
    for (int dur : pattern.durationsSixteenths) {
        HarmonicSegment seg;
        seg.fixedNote                   = span.leadNote;
        seg.durationSixteenths          = dur;
        seg.offsetInFixedNoteSixteenths = offset;
        seg.sourceNoteIndex             = span.firstNoteIndex;
        seg.isRest                      = span.leadNote.isRest();
        segs.push_back(std::move(seg));
        offset += dur;
    }
    return segs;
}

} // namespace

// ── Public static helper ──────────────────────────────────────────────────────

std::vector<HarmonicSplitPattern> HarmonicRhythmPlanner::getAllowedSplitPatterns(
    int durationSixteenths,
    int absoluteStartSixteenths,
    const HarmonizationSettings& settings)
{
    const int basePulse = computeBasePulseSixteenths(settings.timeSignature);

    // ── Cadence split (4/4 whole note at measure boundary) — always active ────
    // This subdivision is unconditional: the cadence-oriented rhythm [4,4,8] is
    // the musically correct default for a whole note in 4/4 regardless of the
    // generic splitLongNotesByBasePulse flag.
    if (basePulse > 0) {
        const bool is4_4 = settings.timeSignature.beats    == 4
                        && settings.timeSignature.beatType == 4;
        const int  measureCapacity   = settings.timeSignature.beats * basePulse;
        const bool atMeasureBoundary = (absoluteStartSixteenths % measureCapacity) == 0;

        if (is4_4 && durationSixteenths == 16 && atMeasureBoundary) {
            return {
                HarmonicSplitPattern{{4, 4, 8}},
                makeEqualSplitPattern(16, basePulse),
            };
        }
    }

    // ── Generic equal split (gated by splitLongNotesByBasePulse) ──────────────
    if (!settings.splitLongNotesByBasePulse)
        return {{{durationSixteenths}}};

    if (basePulse <= 0 || durationSixteenths <= basePulse)
        return {{{durationSixteenths}}};

    if (durationSixteenths % basePulse == 0)
        return {makeEqualSplitPattern(durationSixteenths, basePulse)};

    return {{{durationSixteenths}}};
}

// ── buildPlans ────────────────────────────────────────────────────────────────

std::vector<HarmonicRhythmPlan> HarmonicRhythmPlanner::buildPlans(
    const std::vector<Note>& notes,
    const HarmonizationSettings& settings)
{
    const auto spans = buildTieSpans(notes);
    const int  limit = std::max(1, settings.maxHarmonicRhythmPlans);

    std::vector<HarmonicRhythmPlan> plans = { HarmonicRhythmPlan{} };

    int absoluteStart = 0;
    for (const TieSpan& span : spans) {
        auto patterns = getAllowedSplitPatterns(
            span.totalDuration, absoluteStart, settings);
        absoluteStart += span.totalDuration;

        if (patterns.size() == 1) {
            // Fast path: no branching — extend all existing plans in-place.
            auto segs = makeSegments(span, patterns.front());
            for (auto& plan : plans)
                plan.segments.insert(plan.segments.end(), segs.begin(), segs.end());
        } else {
            // Branching: for each existing plan create one copy per pattern.
            // Plans are enumerated depth-first so earlier (higher-priority) patterns
            // come first in the output, respecting the priority order from
            // getAllowedSplitPatterns.
            std::vector<HarmonicRhythmPlan> next;
            next.reserve(std::min<size_t>(
                static_cast<size_t>(plans.size()) * patterns.size(), limit));

            for (const auto& plan : plans) {
                for (int pi = 0; pi < static_cast<int>(patterns.size()); ++pi) {
                    if (static_cast<int>(next.size()) >= limit) break;
                    HarmonicRhythmPlan branch = plan;
                    branch.priorityScore += pi * 10;
                    auto segs = makeSegments(span, patterns[pi]);
                    branch.segments.insert(branch.segments.end(),
                                           segs.begin(), segs.end());
                    next.push_back(std::move(branch));
                }
                if (static_cast<int>(next.size()) >= limit) break;
            }
            plans = std::move(next);
        }
    }

    return plans;
}

// ── buildSegments ─────────────────────────────────────────────────────────────

std::vector<HarmonicSegment> HarmonicRhythmPlanner::buildSegments(
    const std::vector<Note>& notes,
    const HarmonizationSettings& settings)
{
    auto plans = buildPlans(notes, settings);
    return plans.empty() ? std::vector<HarmonicSegment>{} : std::move(plans.front().segments);
}
