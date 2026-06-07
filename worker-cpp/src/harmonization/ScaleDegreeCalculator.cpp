#include "harmonization/ScaleDegreeCalculator.h"
#include <cctype>
#include <unordered_map>

DegreeInfo ScaleDegreeCalculator::calculateDetailed(const Note& note,
                                                     const HarmonizationSettings& settings) const {
    static const int noteNameToSemitone[] = { 0, 2, 4, 5, 7, 9, 11 };

    static const std::unordered_map<std::string, int> keyToRootSemitone = {
        {"C",0},{"G",7},{"D",2},{"A",9},{"E",4},{"B",11},{"F#",6},{"C#",1},
        {"F",5},{"Bb",10},{"Eb",3},{"Ab",8},{"Db",1},{"Gb",6},{"Cb",11},
        {"G#",8},{"D#",3},{"A#",10}
    };

    // Interval from root (0-11) → degree (1-7), -1 = not in this scale variant.
    // Major: 7 diatonic tones; scaleMode is ignored for major.
    static const int majorDegrees[12]         = { 1,-1, 2,-1, 3, 4,-1, 5,-1, 6,-1, 7 };
    // Three minor variants — differ only at intervals 8 (♭6/♮6) and 10-11 (♭7/♮7).
    static const int naturalMinorDegrees[12]  = { 1,-1, 2, 3,-1, 4,-1, 5, 6,-1, 7,-1 };
    static const int harmonicMinorDegrees[12] = { 1,-1, 2, 3,-1, 4,-1, 5, 6,-1,-1, 7 };
    static const int melodicMinorDegrees[12]  = { 1,-1, 2, 3,-1, 4,-1, 5,-1, 6,-1, 7 };

    auto keyIt = keyToRootSemitone.find(normalizeKeyRoot(settings.key));
    if (keyIt == keyToRootSemitone.end())
        return {-1, ScaleRelation::Chromatic};
    int rootSemitone = keyIt->second;

    int baseSemitone = noteNameToSemitone[static_cast<int>(note.getName()) - 1];
    int noteSemitone = ((baseSemitone + note.getAlter()) % 12 + 12) % 12;
    int interval     = (noteSemitone - rootSemitone + 12) % 12;

    if (isMajorKey(settings.key)) {
        int d = majorDegrees[interval];
        if (d != -1) return {d, ScaleRelation::Natural};

        // ♭VI / ♭VII: the semitone above is a natural major VI or VII,
        // and the note's letter name must match the natural degree's letter
        // (distinguishes Ab=♭VI from G#=chromatic in the same key).
        int intervalAbove = (interval + 1) % 12;
        int dAbove = majorDegrees[intervalAbove];
        if (dAbove == 6 || dAbove == 7) {
            static const NoteName letterOrder[] = {
                NoteName::C, NoteName::D, NoteName::E, NoteName::F,
                NoteName::G, NoteName::A, NoteName::B
            };
            // A-G → diatonic index 5,6,0,1,2,3,4
            static const int charToIdx[] = {5, 6, 0, 1, 2, 3, 4};
            char rootChar = (char)std::toupper((unsigned char)settings.key[0]);
            int rootLetterIdx = charToIdx[rootChar - 'A'];
            int degreeLetterIdx = (rootLetterIdx + dAbove - 1) % 7;
            if (note.getName() == letterOrder[degreeLetterIdx])
                return {dAbove, ScaleRelation::Lowered};
        }
        return {-1, ScaleRelation::Chromatic};
    }

    // Minor: check each requested mode; return the first non-(-1) degree found.
    // Empty scaleModes defaults to all three variants to preserve backward compatibility.
    auto degreeForMode = [&](const std::string& m) -> int {
        if (m == "natural")  return naturalMinorDegrees[interval];
        if (m == "harmonic") return harmonicMinorDegrees[interval];
        if (m == "melodic")  return melodicMinorDegrees[interval];
        return -1;
    };

    int degree = -1;
    if (settings.scaleModes.empty()) {
        for (const char* m : {"natural", "harmonic", "melodic"}) {
            int d = degreeForMode(m);
            if (d != -1) { degree = d; break; }
        }
    } else {
        for (const auto& m : settings.scaleModes) {
            int d = degreeForMode(m);
            if (d != -1) { degree = d; break; }
        }
    }

    if (degree == -1)
        return {-1, ScaleRelation::Chromatic};

    // Natural minor is the base form. Notes found only in harmonic or melodic minor
    // are raised alterations of the natural-minor degree.
    ScaleRelation rel = (naturalMinorDegrees[interval] != -1)
                        ? ScaleRelation::Natural
                        : ScaleRelation::Raised;
    return {degree, rel};
}

int ScaleDegreeCalculator::calculate(const Note& note,
                                      const HarmonizationSettings& settings) const {
    return calculateDetailed(note, settings).degree;
}

bool ScaleDegreeCalculator::isMajorKey(const std::string& key) const {
    return !key.empty() && std::isupper((unsigned char)key[0]);
}

std::string ScaleDegreeCalculator::normalizeKeyRoot(const std::string& key) const {
    if (key.empty()) return key;
    std::string normalized = key;
    normalized[0] = (char)std::toupper((unsigned char)normalized[0]);
    return normalized;
}
