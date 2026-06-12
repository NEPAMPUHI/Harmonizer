#include "checking/CheckHarmonicPositionBuilder.h"
#include <set>

namespace {

struct VoiceEvent {
    int         startTick;
    int         endTick;
    std::size_t noteIndex;
};

std::vector<VoiceEvent> buildTimeline(const std::vector<Note>& notes) {
    std::vector<VoiceEvent> events;
    events.reserve(notes.size());
    int cursor = 0;
    for (std::size_t i = 0; i < notes.size(); ++i) {
        const int dur = notes[i].getDurationSixteenths();
        events.push_back({cursor, cursor + dur, i});
        cursor += dur;
    }
    return events;
}

const Note* activeAt(const std::vector<VoiceEvent>& tl,
                     const std::vector<Note>&        notes,
                     int tick) {
    for (const auto& ev : tl)
        if (ev.startTick <= tick && tick < ev.endTick)
            return &notes[ev.noteIndex];
    return nullptr;
}

} // namespace

std::vector<CheckHarmonicPosition>
CheckHarmonicPositionBuilder::buildMeasure(const CheckMeasureInput& m,
                                           int measureIndex) const {
    const auto stl = buildTimeline(m.soprano);
    const auto atl = buildTimeline(m.alto);
    const auto ttl = buildTimeline(m.tenor);
    const auto btl = buildTimeline(m.bass);

    // Union of all tick boundaries across all voices
    std::set<int> bs;
    for (const auto* tl : {&stl, &atl, &ttl, &btl})
        for (const auto& ev : *tl) {
            bs.insert(ev.startTick);
            bs.insert(ev.endTick);
        }

    const std::vector<int> bounds(bs.begin(), bs.end());
    if (bounds.size() < 2) return {};

    std::vector<CheckHarmonicPosition> result;
    result.reserve(bounds.size() - 1);

    for (std::size_t i = 0; i + 1 < bounds.size(); ++i) {
        const int segStart = bounds[i];
        const int segEnd   = bounds[i + 1];

        CheckHarmonicPosition pos;
        pos.measureIndex                = measureIndex;
        pos.positionInMeasureSixteenths = segStart;
        pos.durationSixteenths          = segEnd - segStart;

        if (const Note* n = activeAt(stl, m.soprano, segStart)) { pos.soprano = *n; pos.hasSoprano = true; }
        if (const Note* n = activeAt(atl, m.alto,    segStart)) { pos.alto    = *n; pos.hasAlto    = true; }
        if (const Note* n = activeAt(ttl, m.tenor,   segStart)) { pos.tenor   = *n; pos.hasTenor   = true; }
        if (const Note* n = activeAt(btl, m.bass,    segStart)) { pos.bass    = *n; pos.hasBass    = true; }

        result.push_back(pos);
    }
    return result;
}

std::vector<CheckHarmonicPosition>
CheckHarmonicPositionBuilder::build(const CheckSolutionInput& input) const {
    std::vector<CheckHarmonicPosition> all;
    for (std::size_t i = 0; i < input.measures.size(); ++i) {
        auto mp = buildMeasure(input.measures[i], static_cast<int>(i) + 1);
        for (auto& p : mp)
            all.push_back(std::move(p));
    }
    return all;
}
