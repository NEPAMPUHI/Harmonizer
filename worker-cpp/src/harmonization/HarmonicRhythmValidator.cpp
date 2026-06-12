#include "harmonization/HarmonicRhythmValidator.h"
#include <algorithm>
#include <map>
#include <sstream>

namespace {

bool samePitch(const Note& a, const Note& b) {
    return a.getName()   == b.getName()
        && a.getOctave() == b.getOctave()
        && a.getAlter()  == b.getAlter();
}

// Mirrors the tie-span logic in HarmonicRhythmPlanner so we can compute the
// expected total duration for each source note (by firstNoteIndex).
std::map<int, int> buildSourceTotals(const std::vector<Note>& notes) {
    std::map<int, int> result;
    int i = 0;
    const int n = static_cast<int>(notes.size());
    while (i < n) {
        const int first = i;
        int total = notes[i].getDurationSixteenths();
        while (!notes[i].isRest()
            && notes[i].isTiedToNext()
            && (i + 1) < n
            && !notes[i + 1].isRest()
            && samePitch(notes[i], notes[i + 1]))
        {
            ++i;
            total += notes[i].getDurationSixteenths();
        }
        result[first] = total;
        ++i;
    }
    return result;
}

RhythmValidationResult checkPositiveDurations(const HarmonicRhythmPlan& plan) {
    for (int i = 0; i < static_cast<int>(plan.segments.size()); ++i) {
        if (plan.segments[i].durationSixteenths <= 0) {
            std::ostringstream msg;
            msg << "Segment " << i << " has non-positive duration: "
                << plan.segments[i].durationSixteenths;
            return RhythmValidationResult::invalid(msg.str());
        }
    }
    return RhythmValidationResult::valid();
}

RhythmValidationResult checkTotalDuration(const HarmonicRhythmPlan& plan,
                                           const std::vector<Note>&  notes) {
    int planTotal = 0;
    for (const auto& seg : plan.segments)
        planTotal += seg.durationSixteenths;

    int inputTotal = 0;
    for (const auto& note : notes)
        inputTotal += note.getDurationSixteenths();

    if (planTotal != inputTotal) {
        std::ostringstream msg;
        msg << "Plan total " << planTotal
            << " sixteenths does not match input total " << inputTotal;
        return RhythmValidationResult::invalid(msg.str());
    }
    return RhythmValidationResult::valid();
}

RhythmValidationResult checkSourceNoteOffsets(const HarmonicRhythmPlan& plan,
                                               const std::vector<Note>&  notes) {
    if (plan.segments.empty())
        return RhythmValidationResult::valid();

    const auto sourceTotals = buildSourceTotals(notes);

    // Group segment pointers by sourceNoteIndex.
    std::map<int, std::vector<const HarmonicSegment*>> groups;
    for (const auto& seg : plan.segments)
        groups[seg.sourceNoteIndex].push_back(&seg);

    for (const auto& [srcIdx, segs] : groups) {
        // Sort within the group by offset so we can walk them in order.
        auto sorted = segs;
        std::sort(sorted.begin(), sorted.end(),
            [](const HarmonicSegment* a, const HarmonicSegment* b) {
                return a->offsetInFixedNoteSixteenths < b->offsetInFixedNoteSixteenths;
            });

        if (sorted[0]->offsetInFixedNoteSixteenths != 0) {
            std::ostringstream msg;
            msg << "Source note " << srcIdx
                << ": first segment has offset "
                << sorted[0]->offsetInFixedNoteSixteenths
                << ", expected 0";
            return RhythmValidationResult::invalid(msg.str());
        }

        int running = 0;
        for (const auto* s : sorted) {
            if (s->offsetInFixedNoteSixteenths != running) {
                std::ostringstream msg;
                msg << "Source note " << srcIdx
                    << ": expected offset " << running
                    << " but got " << s->offsetInFixedNoteSixteenths
                    << " (gap or overlap)";
                return RhythmValidationResult::invalid(msg.str());
            }
            running += s->durationSixteenths;
        }

        const auto it = sourceTotals.find(srcIdx);
        if (it == sourceTotals.end()) {
            std::ostringstream msg;
            msg << "Source note index " << srcIdx
                << " not found in input notes";
            return RhythmValidationResult::invalid(msg.str());
        }
        if (running != it->second) {
            std::ostringstream msg;
            msg << "Source note " << srcIdx
                << ": segments cover " << running
                << " sixteenths but source note spans " << it->second;
            return RhythmValidationResult::invalid(msg.str());
        }
    }
    return RhythmValidationResult::valid();
}

RhythmValidationResult checkMeasureIntegrity(const HarmonicRhythmPlan&    plan,
                                              const HarmonizationSettings& settings) {
    const int beatType = settings.timeSignature.beatType;
    if (beatType <= 0 || 16 % beatType != 0)
        return RhythmValidationResult::valid();  // unsupported meter — skip

    const int measureCap = settings.timeSignature.beats * (16 / beatType);
    if (measureCap <= 0)
        return RhythmValidationResult::valid();

    const int anacrusis = settings.anacrusisSixteenths;
    int absolutePos = 0;

    for (int i = 0; i < static_cast<int>(plan.segments.size()); ++i) {
        const int dur = plan.segments[i].durationSixteenths;

        // Capacity remaining in the current measure from absolutePos.
        int remaining;
        if (anacrusis > 0 && absolutePos < anacrusis) {
            remaining = anacrusis - absolutePos;
        } else {
            const int rel = absolutePos - anacrusis;
            remaining = measureCap - (rel % measureCap);
        }

        if (dur > remaining) {
            std::ostringstream msg;
            msg << "Segment " << i << " (duration " << dur
                << ") overflows its measure by " << (dur - remaining)
                << " sixteenths at absolute position " << absolutePos;
            return RhythmValidationResult::invalid(msg.str());
        }

        absolutePos += dur;
    }
    return RhythmValidationResult::valid();
}

} // namespace

RhythmValidationResult HarmonicRhythmValidator::validate(
    const HarmonicRhythmPlan&    plan,
    const std::vector<Note>&     notes,
    const HarmonizationSettings& settings) const
{
    {
        auto r = checkPositiveDurations(plan);
        if (!r.ok) return r;
    }
    {
        auto r = checkTotalDuration(plan, notes);
        if (!r.ok) return r;
    }
    {
        auto r = checkSourceNoteOffsets(plan, notes);
        if (!r.ok) return r;
    }
    {
        auto r = checkMeasureIntegrity(plan, settings);
        if (!r.ok) return r;
    }
    return RhythmValidationResult::valid();
}
