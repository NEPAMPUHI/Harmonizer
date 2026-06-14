#include "harmonization/ChordBuilder.h"
#include "data/ChordTemplateLibrary.h"
#include "domain/HarmonyRules.h"
#include <cctype>
#include <unordered_map>

// ── pitch-class helpers ───────────────────────────────────────────────────────

namespace {

struct PitchClass { NoteName name; int alter; };

// Compute (NoteName, alter) for a scale degree in the given key and scale mode.
// If fixedNote is provided with the same degree and a Lowered/Raised ScaleRelation,
// its alteration is propagated to keep all voices on the same chromatic form.
PitchClass degreeToNote(int degree, const std::string& key,
                        const std::vector<std::string>& scaleModes,
                        const Note* fixedNote = nullptr, int fixedDegree = -1) {
    if (degree < 1 || degree > 7) return {NoteName::C, 0};

    static const int noteBase[] = {0, 2, 4, 5, 7, 9, 11}; // C D E F G A B
    static const std::unordered_map<std::string, int> keyToRoot = {
        {"C",0},{"G",7},{"D",2},{"A",9},{"E",4},{"B",11},{"F#",6},{"C#",1},
        {"F",5},{"Bb",10},{"Eb",3},{"Ab",8},{"Db",1},{"Gb",6},{"Cb",11},
        {"G#",8},{"D#",3},{"A#",10}
    };
    static const int majorInt[8]   = {0, 0, 2, 4, 5, 7, 9,11};
    static const int natMinInt[8]  = {0, 0, 2, 3, 5, 7, 8,10};
    static const int harmMinInt[8] = {0, 0, 2, 3, 5, 7, 8,11};
    static const int melMinInt[8]  = {0, 0, 2, 3, 5, 7, 9,11};

    std::string normKey = key;
    if (!normKey.empty()) normKey[0] = (char)std::toupper((unsigned char)normKey[0]);
    auto it = keyToRoot.find(normKey);
    int rootSemitone = (it != keyToRoot.end()) ? it->second : 0;

    bool isMajor = !key.empty() && std::isupper((unsigned char)key[0]);

    char rc = (char)std::toupper((unsigned char)key[0]);
    static const int charIdx[] = {5, 6, 0, 1, 2, 3, 4}; // A-G → 5,6,0,1,2,3,4
    int rootIdx = charIdx[rc - 'A'];

    int noteIdx = (rootIdx + degree - 1) % 7;
    static const NoteName names[] = {
        NoteName::C, NoteName::D, NoteName::E, NoteName::F,
        NoteName::G, NoteName::A, NoteName::B
    };
    NoteName name = names[noteIdx];
    int naturalSemitone = noteBase[noteIdx];

    int interval;
    if (isMajor) {
        interval = majorInt[degree];
    } else {
        const std::string& mode = scaleModes.empty() ? "natural" : scaleModes[0];
        if (mode == "harmonic")     interval = harmMinInt[degree];
        else if (mode == "melodic") interval = melMinInt[degree];
        else                        interval = natMinInt[degree];
    }

    int actualSemitone = (rootSemitone + interval) % 12;
    int alter = actualSemitone - naturalSemitone;
    if (alter >  6) alter -= 12;
    if (alter < -6) alter += 12;

    // Propagate the fixed voice's alteration when building another voice at the same degree.
    // This prevents mixing, e.g., Ab (soprano) with A natural (another voice) in the same chord.
    if (fixedNote && degree == fixedDegree) {
        ScaleRelation rel = fixedNote->getScaleRelation();
        if (rel == ScaleRelation::Lowered || rel == ScaleRelation::Raised)
            alter = fixedNote->getAlter();
    }

    return {name, alter};
}

// ── single-note placement helpers ────────────────────────────────────────────

// Highest note of (name,alter,degree) at or below ceilingSemitone.
// Prefers notes within range; falls back to out-of-range if nothing fits in range.
Note fitInRange(NoteName name, int alter, int degree,
                const VoiceRange& range, int ceilingSemitone) {
    for (int oct = 9; oct >= 0; --oct) {
        Note candidate(name, oct, alter, degree, 4, false);
        if (candidate.getSemitone() <= ceilingSemitone && range.contains(candidate))
            return candidate;
    }
    for (int oct = 9; oct >= 0; --oct) {
        Note candidate(name, oct, alter, degree, 4, false);
        if (candidate.getSemitone() <= ceilingSemitone)
            return candidate;
    }
    return Note(name, range.min.getOctave(), alter, degree, 4, false);
}

// Lowest note of (name,alter,degree) at or above floorSemitone.
// Prefers notes within range; falls back to out-of-range if nothing fits in range.
Note fitAbove(NoteName name, int alter, int degree,
              const VoiceRange& range, int floorSemitone) {
    for (int oct = 0; oct <= 9; ++oct) {
        Note candidate(name, oct, alter, degree, 4, false);
        if (candidate.getSemitone() >= floorSemitone && range.contains(candidate))
            return candidate;
    }
    for (int oct = 0; oct <= 9; ++oct) {
        Note candidate(name, oct, alter, degree, 4, false);
        if (candidate.getSemitone() >= floorSemitone)
            return candidate;
    }
    return Note(name, range.max.getOctave(), alter, degree, 4, false);
}

// ── multi-note placement helpers ─────────────────────────────────────────────

// All notes at or below ceiling, descending (highest first).
// Prefers in-range notes; if none, falls back to out-of-range candidates.
std::vector<Note> fitAllInRange(NoteName name, int alter, int degree,
                                const VoiceRange& range, int ceilingSemitone) {
    std::vector<Note> result;
    for (int oct = 9; oct >= 0; --oct) {
        Note candidate(name, oct, alter, degree, 4, false);
        if (candidate.getSemitone() <= ceilingSemitone && range.contains(candidate))
            result.push_back(candidate);
    }
    if (result.empty()) {
        for (int oct = 9; oct >= 0; --oct) {
            Note candidate(name, oct, alter, degree, 4, false);
            if (candidate.getSemitone() <= ceilingSemitone)
                result.push_back(candidate);
        }
    }
    return result;
}

// All notes at or above floor, ascending (lowest/nearest first).
// Prefers in-range notes; if none, falls back to out-of-range candidates.
std::vector<Note> fitAllAbove(NoteName name, int alter, int degree,
                               const VoiceRange& range, int floorSemitone) {
    std::vector<Note> result;
    for (int oct = 0; oct <= 9; ++oct) {
        Note candidate(name, oct, alter, degree, 4, false);
        if (candidate.getSemitone() >= floorSemitone && range.contains(candidate))
            result.push_back(candidate);
    }
    if (result.empty()) {
        for (int oct = 0; oct <= 9; ++oct) {
            Note candidate(name, oct, alter, degree, 4, false);
            if (candidate.getSemitone() >= floorSemitone)
                result.push_back(candidate);
        }
    }
    return result;
}

// Rules used when validating chords during building: all checks except voice ranges,
// which are deferred to graph-level validation where the active rule set is known.
const ActiveRuleSet& buildTimeRules() {
    static const ActiveRuleSet r = []{
        auto x = ActiveRuleSet::allEnabled();
        x.voiceRanges = false;
        return x;
    }();
    return r;
}

// ── position-aware adjacency rules ───────────────────────────────────────────

// Ceiling for a voice built below its upper neighbour.
// Wide + same degree: force one semitone below upper → same pitch class lands one octave lower.
// Close / Mixed: allow unison (same semitone as upper).
int adjacentCeiling(int upperSemitone, int upperDeg, int thisDeg, ChordPosition pos) {
    if (pos == ChordPosition::Wide && upperDeg == thisDeg)
        return upperSemitone - 1;
    return upperSemitone;
}

// Floor for a voice built above its lower neighbour.
// Wide + same degree: force one semitone above lower → same pitch class lands one octave higher.
// Close / Mixed: allow unison (same semitone as lower).
int adjacentFloor(int lowerSemitone, int lowerDeg, int thisDeg, ChordPosition pos) {
    if (pos == ChordPosition::Wide && lowerDeg == thisDeg)
        return lowerSemitone + 1;
    return lowerSemitone;
}

// ── voicing algorithms ────────────────────────────────────────────────────────

// Melody mode: soprano is fixed; alto/tenor built downward from soprano;
// bass iterates all valid octave positions at or below tenor → multiple chords.
std::vector<Chord> voiceMelodyChords(const Note& soprano, const ChordTemplate& tmpl,
                                     const std::string& key,
                                     const std::vector<std::string>& modes) {
    const auto& d = tmpl.degreesInSatbOrder;
    ChordPosition pos = tmpl.position;

    auto apc = degreeToNote(d[1], key, modes, &soprano, d[0]);
    auto tpc = degreeToNote(d[2], key, modes, &soprano, d[0]);
    auto bpc = degreeToNote(d[3], key, modes, &soprano, d[0]);

    int altoCeiling = adjacentCeiling(soprano.getSemitone(), d[0], d[1], pos);
    Note alto = fitInRange(apc.name, apc.alter, d[1], ALTO_RANGE, altoCeiling);
    if (alto.getSemitone() > altoCeiling) return {};  // no valid alto ≤ soprano: skip template

    int tenorCeiling = adjacentCeiling(alto.getSemitone(), d[1], d[2], pos);
    Note tenor = fitInRange(tpc.name, tpc.alter, d[2], TENOR_RANGE, tenorCeiling);
    if (tenor.getSemitone() > tenorCeiling) return {};  // no valid tenor ≤ alto: skip template

    auto bassCandidates = fitAllInRange(bpc.name, bpc.alter, d[3], BASS_RANGE,
                                        tenor.getSemitone());

    std::vector<Chord> result;
    for (const Note& bass : bassCandidates) {
        Chord chord(soprano, alto, tenor, bass, tmpl);
        if (HarmonyRules::isValidChord(chord, buildTimeRules()))
            result.push_back(chord);
    }
    return result;
}

// Bass mode: bass is fixed; tenor iterates up to 3 octave positions above bass;
// for each tenor, alto and soprano are built upward → multiple chords.
std::vector<Chord> voiceBassChords(const Note& bass, const ChordTemplate& tmpl,
                                   const std::string& key,
                                   const std::vector<std::string>& modes) {
    const auto& d = tmpl.degreesInSatbOrder;
    ChordPosition pos = tmpl.position;

    auto tpc = degreeToNote(d[2], key, modes, &bass, d[3]);
    auto apc = degreeToNote(d[1], key, modes, &bass, d[3]);
    auto spc = degreeToNote(d[0], key, modes, &bass, d[3]);

    // Up to 3 tenor candidates above (or at) bass, nearest first.
    auto tenorAll = fitAllAbove(tpc.name, tpc.alter, d[2], TENOR_RANGE, bass.getSemitone());
    if (tenorAll.size() > 3) tenorAll.resize(3);

    std::vector<Chord> result;
    for (const Note& tenor : tenorAll) {
        int tb = tenor.getSemitone() - bass.getSemitone();
        if (tb < 0 || tb > 24) continue; // skip if tenor-bass > two octaves

        int altoFloor   = adjacentFloor(tenor.getSemitone(), d[2], d[1], pos);
        Note alto       = fitAbove(apc.name, apc.alter, d[1], ALTO_RANGE, altoFloor);
        if (alto.getSemitone() < altoFloor) continue;  // no valid alto ≥ tenor: skip this tenor

        int sopranoFloor = adjacentFloor(alto.getSemitone(), d[1], d[0], pos);
        Note soprano     = fitAbove(spc.name, spc.alter, d[0], SOPRANO_RANGE, sopranoFloor);
        if (soprano.getSemitone() < sopranoFloor) continue;  // no valid soprano ≥ alto: skip this tenor

        Chord chord(soprano, alto, tenor, bass, tmpl);
        if (HarmonyRules::isValidChord(chord, buildTimeRules()))
            result.push_back(chord);
    }
    return result;
}

// Legacy single-chord voicings kept as fallbacks for createChordFromTemplate.
Chord voiceMelody(const Note& soprano, const ChordTemplate& tmpl,
                  const std::string& key, const std::vector<std::string>& modes) {
    auto apc = degreeToNote(tmpl.degreesInSatbOrder[1], key, modes, &soprano, tmpl.degreesInSatbOrder[0]);
    auto tpc = degreeToNote(tmpl.degreesInSatbOrder[2], key, modes, &soprano, tmpl.degreesInSatbOrder[0]);
    auto bpc = degreeToNote(tmpl.degreesInSatbOrder[3], key, modes, &soprano, tmpl.degreesInSatbOrder[0]);
    Note alto  = fitInRange(apc.name, apc.alter, tmpl.degreesInSatbOrder[1],
                            ALTO_RANGE,  soprano.getSemitone());
    Note tenor = fitInRange(tpc.name, tpc.alter, tmpl.degreesInSatbOrder[2],
                            TENOR_RANGE, alto.getSemitone());
    Note bass  = fitInRange(bpc.name, bpc.alter, tmpl.degreesInSatbOrder[3],
                            BASS_RANGE,  tenor.getSemitone());
    return Chord(soprano, alto, tenor, bass, tmpl);
}

Chord voiceBass(const Note& bass, const ChordTemplate& tmpl,
                const std::string& key, const std::vector<std::string>& modes) {
    auto tpc = degreeToNote(tmpl.degreesInSatbOrder[2], key, modes, &bass, tmpl.degreesInSatbOrder[3]);
    auto apc = degreeToNote(tmpl.degreesInSatbOrder[1], key, modes, &bass, tmpl.degreesInSatbOrder[3]);
    auto spc = degreeToNote(tmpl.degreesInSatbOrder[0], key, modes, &bass, tmpl.degreesInSatbOrder[3]);
    Note tenor   = fitAbove(tpc.name, tpc.alter, tmpl.degreesInSatbOrder[2],
                            TENOR_RANGE,   bass.getSemitone());
    Note alto    = fitAbove(apc.name, apc.alter, tmpl.degreesInSatbOrder[1],
                            ALTO_RANGE,    tenor.getSemitone());
    Note soprano = fitAbove(spc.name, spc.alter, tmpl.degreesInSatbOrder[0],
                            SOPRANO_RANGE, alto.getSemitone());
    return Chord(soprano, alto, tenor, bass, tmpl);
}

// ── canonical template name ───────────────────────────────────────────────────

// Mirrors Chord::getName() but operates on ChordTemplate directly.
// Returns "" for unrecognised degree or type.
std::string getTemplateName(const ChordTemplate& tmpl) {
    if (tmpl.type == ChordType::CadentialSixFour) return "K64";

    std::string typeStr;
    switch (tmpl.type) {
        case ChordType::Triad:      typeStr = "53"; break;
        case ChordType::Six:        typeStr = "6";  break;
        case ChordType::SixFour:    typeStr = "64"; break;
        case ChordType::Seventh:    typeStr = "7";  break;
        case ChordType::SixFive:    typeStr = "65"; break;
        case ChordType::FourThree:  typeStr = "43"; break;
        case ChordType::Two:        typeStr = "2";  break;
        case ChordType::Ninth:      typeStr = "9";  break;
        default: return "";
    }

    switch (tmpl.degree) {
        case 1: return "T"   + typeStr;
        case 2: return "II"  + typeStr;
        case 3: return "III" + typeStr;
        case 4: return "S"   + typeStr;
        case 5: return "D"   + typeStr;
        case 6: return "VI"  + typeStr;
        case 7: return "VII" + typeStr;
        default: return "";
    }
}

} // namespace

// ── public ────────────────────────────────────────────────────────────────────

std::vector<Chord> ChordBuilder::buildForFixedMelodyNote(const Note& melodyNote,
                                                         const HarmonizationSettings& settings) {
    auto templates = getAllowedTemplates(settings);
    std::vector<Chord> chords;
    for (const auto& tmpl : templates) {
        if (canUseTemplate(tmpl, melodyNote, HarmonizationMode::HarmonizeMelody, settings)) {
            auto variants = createChordsFromTemplate(melodyNote, tmpl,
                                                     HarmonizationMode::HarmonizeMelody, settings);
            chords.insert(chords.end(), variants.begin(), variants.end());
        }
    }
    return chords;
}

std::vector<Chord> ChordBuilder::buildForFixedBassNote(const Note& bassNote,
                                                        const HarmonizationSettings& settings) {
    auto templates = getAllowedTemplates(settings);
    std::vector<Chord> chords;
    for (const auto& tmpl : templates) {
        if (canUseTemplate(tmpl, bassNote, HarmonizationMode::HarmonizeBass, settings)) {
            auto variants = createChordsFromTemplate(bassNote, tmpl,
                                                     HarmonizationMode::HarmonizeBass, settings);
            chords.insert(chords.end(), variants.begin(), variants.end());
        }
    }
    return chords;
}

std::vector<Chord> ChordBuilder::buildAllValid(const HarmonizationSettings& settings) {
    auto templates = getAllowedTemplates(settings);
    std::vector<Chord> result;
    for (const auto& tmpl : templates) {
        const int bassDeg = tmpl.degreesInSatbOrder[3];
        auto bpc = degreeToNote(bassDeg, settings.key, settings.scaleModes);
        auto bassNotes = fitAllInRange(bpc.name, bpc.alter, bassDeg,
                                       BASS_RANGE, BASS_RANGE.max.getSemitone());
        for (const Note& bass : bassNotes) {
            auto variants = voiceBassChords(bass, tmpl, settings.key, settings.scaleModes);
            result.insert(result.end(), variants.begin(), variants.end());
        }
    }
    return result;
}

bool ChordBuilder::canUseTemplate(const ChordTemplate& chordTemplate,
                                  const Note& fixedNote,
                                  HarmonizationMode mode,
                                  const HarmonizationSettings& /*settings*/) const {
    int degree = fixedNote.getDegree();
    if (degree <= 0) return false;
    if (mode == HarmonizationMode::HarmonizeMelody)
        return chordTemplate.degreesInSatbOrder[0] == degree; // soprano must match
    else
        return chordTemplate.degreesInSatbOrder[3] == degree; // bass must match
}

std::vector<Chord> ChordBuilder::createChordsFromTemplate(
    const Note& fixedNote,
    const ChordTemplate& chordTemplate,
    HarmonizationMode mode,
    const HarmonizationSettings& settings) const
{
    if (mode == HarmonizationMode::HarmonizeMelody)
        return voiceMelodyChords(fixedNote, chordTemplate, settings.key, settings.scaleModes);
    else
        return voiceBassChords(fixedNote, chordTemplate, settings.key, settings.scaleModes);
}

Chord ChordBuilder::createChordFromTemplate(const Note& fixedNote,
                                            const ChordTemplate& chordTemplate,
                                            HarmonizationMode mode,
                                            const HarmonizationSettings& settings) const {
    auto chords = createChordsFromTemplate(fixedNote, chordTemplate, mode, settings);
    if (!chords.empty()) return chords.front();
    // Pathological fallback: return single unvalidated voicing.
    if (mode == HarmonizationMode::HarmonizeMelody)
        return voiceMelody(fixedNote, chordTemplate, settings.key, settings.scaleModes);
    return voiceBass(fixedNote, chordTemplate, settings.key, settings.scaleModes);
}

// ── private ───────────────────────────────────────────────────────────────────

std::vector<ChordTemplate> ChordBuilder::getAllowedTemplates(
    const HarmonizationSettings& settings) const
{
    auto allTemplates = ChordTemplateLibrary::createAllTemplates();
    std::vector<ChordTemplate> allowed;
    for (const auto& tmpl : allTemplates) {
        const std::string name = getTemplateName(tmpl);
        if (name.empty()) continue;
        for (const auto& a : settings.allowedChords) {
            if (name == a) { allowed.push_back(tmpl); break; }
        }
    }
    return allowed;
}
