#include "HarmonicPositionBuilder.h"
#include <algorithm>

namespace {

int getNoteDurationSixteenths(const Note& note) {
    Duration d = note.getDuration();
    return (d.numerator * 16) / d.denominator;
}

int getMeasureCapacitySixteenths(const HarmonizationSettings& settings) {
    return settings.timeSignature.beats * (16 / settings.timeSignature.beatType);
}

int getDefaultHarmonicDurationSixteenths(const HarmonizationSettings& settings) {
    int beatType = settings.timeSignature.beatType;
    if (beatType == 4) return 4;
    if (beatType == 8) return 2;
    return 4;
}

bool computeIsStrongBeat(int startInMeasure, const HarmonizationSettings& settings) {
    if (startInMeasure == 0) return true;
    int beats = settings.timeSignature.beats;
    int beatType = settings.timeSignature.beatType;
    // 4/4: also strong on 3rd beat (sixteenth 8)
    if (beatType == 4 && beats == 4 && startInMeasure == 8) return true;
    // 12/8: also strong on 7th eighth (sixteenth 12)
    if (beatType == 8 && beats == 12 && startInMeasure == 12) return true;
    return false;
}

bool computeIsMediumBeat(int startInMeasure, const HarmonizationSettings& settings) {
    int beats = settings.timeSignature.beats;
    int beatType = settings.timeSignature.beatType;
    if (beatType == 4 && beats == 3 && startInMeasure == 4) return true;
    if (beatType == 8 && beats == 6 && startInMeasure == 6) return true;
    if (beatType == 8 && beats == 9 && (startInMeasure == 6 || startInMeasure == 12)) return true;
    if (beatType == 8 && beats == 12 && (startInMeasure == 6 || startInMeasure == 18)) return true;
    return false;
}

} // namespace

std::vector<HarmonicPosition> HarmonicPositionBuilder::build(
    const ScoreInput& input,
    const HarmonizationSettings& settings,
    HarmonizationMode mode)
{
    (void)mode; // mode determines fixedNote semantics (soprano vs bass), but ScoreInput already holds the relevant voice

    // Compute absolute start/end in sixteenths for each input note
    std::vector<int> noteStarts;
    std::vector<int> noteEnds;
    {
        int cursor = 0;
        for (const Note& note : input.notes) {
            noteStarts.push_back(cursor);
            cursor += getNoteDurationSixteenths(note);
            noteEnds.push_back(cursor);
        }
    }

    const int measureCapacity  = getMeasureCapacitySixteenths(settings);
    const int harmonicDuration = getDefaultHarmonicDurationSixteenths(settings);
    const int anacrusisOffset  = settings.anacrusisSixteenths;

    struct Candidate {
        int measureIndex;
        int startSixteenth;   // position within measure
        int durationSixteenths;
        int absoluteStart;
        int absoluteEnd;
    };

    std::vector<Candidate> candidates;

    // Anacrusis position (measure 0)
    if (anacrusisOffset > 0) {
        candidates.push_back({0, 0, anacrusisOffset, 0, anacrusisOffset});
    }

    // Main measures (1 .. measureCount)
    for (int m = 1; m <= settings.measureCount; ++m) {
        int measureAbsStart = anacrusisOffset + (m - 1) * measureCapacity;
        for (int s = 0; s < measureCapacity; s += harmonicDuration) {
            int dur = std::min(harmonicDuration, measureCapacity - s);
            candidates.push_back({m, s, dur,
                                   measureAbsStart + s,
                                   measureAbsStart + s + dur});
        }
    }

    std::vector<HarmonicPosition> result;
    result.reserve(candidates.size());

    const int noteCount = (int)input.notes.size();

    for (const Candidate& c : candidates) {
        // Fixed note: the note sounding at the start of this position
        int fixedIdx = -1;
        for (int ni = 0; ni < noteCount; ++ni) {
            if (noteStarts[ni] <= c.absoluteStart && c.absoluteStart < noteEnds[ni]) {
                fixedIdx = ni;
                break;
            }
        }
        if (fixedIdx == -1) continue; // no note covers this position — skip

        HarmonicPosition pos;
        pos.measureIndex      = c.measureIndex;
        pos.startSixteenth    = c.startSixteenth;
        pos.durationSixteenths = c.durationSixteenths;
        pos.fixedNote         = input.notes[fixedIdx];

        // Melody notes: all notes intersecting [absoluteStart, absoluteEnd)
        for (int ni = 0; ni < noteCount; ++ni) {
            if (noteStarts[ni] < c.absoluteEnd && noteEnds[ni] > c.absoluteStart) {
                pos.melodyNotes.push_back(input.notes[ni]);
            }
        }

        const bool strong = computeIsStrongBeat(c.startSixteenth, settings);
        const bool medium = !strong && computeIsMediumBeat(c.startSixteenth, settings);
        pos.isStrongBeat = strong;
        pos.isMediumBeat = medium;
        pos.isWeakBeat   = !strong && !medium;

        result.push_back(std::move(pos));
    }

    // Assign indices and form zones (thirds of total positions)
    const int total = (int)result.size();
    for (int i = 0; i < total; ++i) {
        result[i].index          = i;
        result[i].isBeginningZone = (i * 3 < total);
        result[i].isEndingZone    = (i * 3 >= 2 * total);
        result[i].isMiddleZone    = !result[i].isBeginningZone && !result[i].isEndingZone;
        result[i].isCadentialZone = false;
    }

    return result;
}
