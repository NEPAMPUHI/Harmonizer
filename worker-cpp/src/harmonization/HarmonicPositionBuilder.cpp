#include "HarmonicPositionBuilder.h"

namespace {

int getMeasureCapacitySixteenths(const HarmonizationSettings& settings) {
    return settings.timeSignature.beats * (16 / settings.timeSignature.beatType);
}

bool computeIsStrongBeat(int startInMeasure, const HarmonizationSettings& settings) {
    if (startInMeasure == 0) return true;
    int beats    = settings.timeSignature.beats;
    int beatType = settings.timeSignature.beatType;
    if (beatType == 4 && beats == 4 && startInMeasure == 8) return true;
    if (beatType == 8 && beats == 12 && startInMeasure == 12) return true;
    return false;
}

bool computeIsMediumBeat(int startInMeasure, const HarmonizationSettings& settings) {
    int beats    = settings.timeSignature.beats;
    int beatType = settings.timeSignature.beatType;
    if (beatType == 4 && beats == 3 && startInMeasure == 4) return true;
    if (beatType == 8 && beats == 6 && startInMeasure == 6) return true;
    if (beatType == 8 && beats == 9  && (startInMeasure == 6 || startInMeasure == 12)) return true;
    if (beatType == 8 && beats == 12 && (startInMeasure == 6 || startInMeasure == 18)) return true;
    return false;
}

} // namespace

std::vector<HarmonicPosition> HarmonicPositionBuilder::build(
    const std::vector<HarmonicSegment>& segments,
    const HarmonizationSettings& settings,
    HarmonizationMode mode)
{
    (void)mode;

    const int measureCapacity = getMeasureCapacitySixteenths(settings);
    const int anacrusisOffset = settings.anacrusisSixteenths;

    std::vector<HarmonicPosition> result;
    result.reserve(segments.size());

    int absoluteStart = 0;
    for (const HarmonicSegment& seg : segments) {
        int measIdx, startInMeasure;
        if (anacrusisOffset > 0 && absoluteStart < anacrusisOffset) {
            measIdx       = 0;
            startInMeasure = absoluteStart;
        } else {
            const int rel = absoluteStart - anacrusisOffset;
            measIdx       = rel / measureCapacity + 1;
            startInMeasure = rel % measureCapacity;
        }

        HarmonicPosition pos;
        pos.measureIndex                = measIdx;
        pos.startSixteenth              = startInMeasure;
        pos.durationSixteenths          = seg.durationSixteenths;
        pos.fixedNote                   = seg.fixedNote;
        pos.melodyNotes.push_back(seg.fixedNote);
        pos.sourceNoteIndex             = seg.sourceNoteIndex;
        pos.offsetInFixedNoteSixteenths = seg.offsetInFixedNoteSixteenths;

        const bool strong = computeIsStrongBeat(startInMeasure, settings);
        const bool medium = !strong && computeIsMediumBeat(startInMeasure, settings);
        pos.isStrongBeat = strong;
        pos.isMediumBeat = medium;
        pos.isWeakBeat   = !strong && !medium;

        result.push_back(std::move(pos));
        absoluteStart += seg.durationSixteenths;
    }

    const int total = static_cast<int>(result.size());
    for (int i = 0; i < total; ++i) {
        result[i].index           = i;
        result[i].isBeginningZone = (i * 3 < total);
        result[i].isEndingZone    = (i * 3 >= 2 * total);
        result[i].isMiddleZone    = !result[i].isBeginningZone && !result[i].isEndingZone;
        result[i].isCadentialZone = false;
    }

    return result;
}
