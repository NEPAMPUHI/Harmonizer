#include "infrastructure/ScoreSerializer.h"
#include <cctype>
#include <map>

namespace {

void applyDurationToFrontendNote(int s, FrontendNote& fn) {
    switch (s) {
        case 16: fn.durationType = "whole";   fn.dotted = false; return;
        case 12: fn.durationType = "half";    fn.dotted = true;  return;
        case  8: fn.durationType = "half";    fn.dotted = false; return;
        case  6: fn.durationType = "quarter"; fn.dotted = true;  return;
        case  4: fn.durationType = "quarter"; fn.dotted = false; return;
        case  3: fn.durationType = "eighth";  fn.dotted = true;  return;
        case  2: fn.durationType = "eighth";  fn.dotted = false; return;
        case  1: fn.durationType = "16th";    fn.dotted = false; return;
        default: fn.durationType = "quarter"; fn.dotted = false; return;
    }
}

bool pitchEqual(const FrontendNote& a, const FrontendNote& b) {
    if (a.isRest || b.isRest) return false;
    return a.step == b.step && a.octave == b.octave && a.alter == b.alter;
}

bool noteIdentical(const FrontendNote& a, const FrontendNote& b) {
    if (a.isRest && b.isRest) return true;
    if (a.isRest || b.isRest) return false;
    return a.step == b.step && a.octave == b.octave && a.alter == b.alter;
}

// Merge consecutive notes that share the same sourceNoteIndex (>= 0).
// Used for the fixed voice (soprano or bass).
std::vector<FrontendNote> mergeFixed(
    const std::vector<FrontendNote>& notes,
    const std::vector<int>&          srcIdx)
{
    std::vector<FrontendNote> result;
    size_t i = 0;
    while (i < notes.size()) {
        FrontendNote merged = notes[i];
        merged.tiedToNext = false;
        if (srcIdx[i] >= 0) {
            size_t j = i + 1;
            while (j < notes.size() && srcIdx[j] == srcIdx[i])
                merged.durationSixteenths += notes[j++].durationSixteenths;
            applyDurationToFrontendNote(merged.durationSixteenths, merged);
            result.push_back(merged);
            i = j;
        } else {
            result.push_back(merged);
            ++i;
        }
    }
    return result;
}

// Merge consecutive notes with identical pitch (non-rests only).
// chordTicks[i] = within-measure start (sixteenths) of notes[i]; used to verify adjacency.
std::vector<FrontendNote> mergeByPitch(
    const std::vector<FrontendNote>& notes,
    const std::vector<int>&          chordTicks)
{
    std::vector<FrontendNote> result;
    size_t i = 0;
    while (i < notes.size()) {
        FrontendNote merged = notes[i];
        size_t j = i + 1;
        int expectedTick = chordTicks[i] + notes[i].durationSixteenths;
        while (j < notes.size()
            && pitchEqual(notes[i], notes[j])
            && chordTicks[j] == expectedTick)
        {
            merged.durationSixteenths += notes[j].durationSixteenths;
            expectedTick             += notes[j].durationSixteenths;
            ++j;
        }
        applyDurationToFrontendNote(merged.durationSixteenths, merged);
        result.push_back(merged);
        i = j;
    }
    return result;
}

} // namespace

FrontendScore ScoreSerializer::serialize(const Score& score,
                                         const HarmonizationSettings& settings) const {
    FrontendScore result;
    result.key  = settings.key;
    result.mode = (!settings.key.empty() && std::isupper((unsigned char)settings.key[0]))
                      ? "major"
                      : "minor";
    result.beats               = settings.timeSignature.beats;
    result.beatType            = settings.timeSignature.beatType;
    result.anacrusisSixteenths = settings.anacrusisSixteenths;

    if (score.chords.empty())
        return result;

    const bool hasPositions = (score.positions.size() == score.chords.size());
    const int  fvi          = score.fixedVoiceIndex;  // 0=soprano, 3=bass, -1=none

    // Per-measure accumulation before merge, auto-sorted by measureIndex.
    struct MeasureData {
        int number = 0;
        std::vector<FrontendNote> soprano, alto, tenor, bass;
        std::vector<std::string>  chordNames;
        std::vector<int>          chordTicks;
        std::vector<bool>         chordLabelVisible;
        std::vector<int>          srcIdx;   // sourceNoteIndex per position
    };
    std::map<int, MeasureData> mdata;

    for (size_t i = 0; i < score.chords.size(); ++i) {
        const int mi   = hasPositions ? score.positions[i].measureIndex       : 0;
        const int dur  = hasPositions ? score.positions[i].durationSixteenths : 4;
        const int tick = hasPositions ? score.positions[i].startSixteenth      : 0;
        const int src  = hasPositions ? score.positions[i].sourceNoteIndex     : -1;

        MeasureData& md = mdata[mi];
        md.number = mi;

        const Chord& chord = score.chords[i];

        bool fixedIsRest = false;
        if (hasPositions && fvi >= 0)
            fixedIsRest = score.positions[i].fixedNote.isRest();

        FrontendNote sn = toFrontendNote(chord.getSoprano(), dur);
        FrontendNote an = toFrontendNote(chord.getAlto(),    dur);
        FrontendNote tn = toFrontendNote(chord.getTenor(),   dur);
        FrontendNote bn = toFrontendNote(chord.getBass(),    dur);

        if (fvi == 0) sn.isRest = fixedIsRest;
        if (fvi == 3) bn.isRest = fixedIsRest;

        md.soprano.push_back(sn);
        md.alto.push_back(an);
        md.tenor.push_back(tn);
        md.bass.push_back(bn);

        try { md.chordNames.push_back(chord.getName()); }
        catch (...) { md.chordNames.push_back("?"); }

        md.chordTicks.push_back(tick);
        md.srcIdx.push_back(src);
    }

    for (auto& [idx, md] : mdata) {
        // Compute label visibility before merge (per-position FrontendNotes still intact).
        md.chordLabelVisible.resize(md.chordNames.size());
        for (size_t i = 0; i < md.chordNames.size(); ++i) {
            if (i == 0) { md.chordLabelVisible[i] = true; continue; }
            bool differs =
                md.chordNames[i] != md.chordNames[i - 1]
                || !noteIdentical(md.soprano[i], md.soprano[i - 1])
                || !noteIdentical(md.alto[i],    md.alto[i - 1])
                || !noteIdentical(md.tenor[i],   md.tenor[i - 1])
                || !noteIdentical(md.bass[i],     md.bass[i - 1]);
            md.chordLabelVisible[i] = differs;
        }

        FrontendMeasure m;
        m.number            = md.number;
        m.chordNames        = md.chordNames;
        m.chordTicks        = md.chordTicks;
        m.chordLabelVisible = md.chordLabelVisible;

        if (fvi == 0) {
            m.voices.soprano = mergeFixed(md.soprano, md.srcIdx);
            m.voices.alto    = mergeByPitch(md.alto,   md.chordTicks);
            m.voices.tenor   = mergeByPitch(md.tenor,  md.chordTicks);
            m.voices.bass    = mergeByPitch(md.bass,   md.chordTicks);
        } else if (fvi == 3) {
            m.voices.soprano = mergeByPitch(md.soprano, md.chordTicks);
            m.voices.alto    = mergeByPitch(md.alto,    md.chordTicks);
            m.voices.tenor   = mergeByPitch(md.tenor,   md.chordTicks);
            m.voices.bass    = mergeFixed(md.bass, md.srcIdx);
        } else {
            m.voices.soprano = mergeByPitch(md.soprano, md.chordTicks);
            m.voices.alto    = mergeByPitch(md.alto,    md.chordTicks);
            m.voices.tenor   = mergeByPitch(md.tenor,   md.chordTicks);
            m.voices.bass    = mergeByPitch(md.bass,    md.chordTicks);
        }

        result.measures.push_back(std::move(m));
    }

    return result;
}

FrontendNote ScoreSerializer::toFrontendNote(const Note& note, int durationSixteenths) const {
    FrontendNote fn;
    fn.step               = noteNameToStep(note.getName());
    fn.octave             = note.getOctave();
    fn.alter              = note.getAlter();
    fn.durationSixteenths = durationSixteenths;
    applyDurationToFrontendNote(durationSixteenths, fn);
    return fn;
}

std::string ScoreSerializer::noteNameToStep(NoteName name) const {
    switch (name) {
        case NoteName::C: return "C";
        case NoteName::D: return "D";
        case NoteName::E: return "E";
        case NoteName::F: return "F";
        case NoteName::G: return "G";
        case NoteName::A: return "A";
        case NoteName::B: return "B";
    }
    return "C";
}

