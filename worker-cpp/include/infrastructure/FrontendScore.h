#ifndef HARM_FRONTENDSCORE_H
#define HARM_FRONTENDSCORE_H

#include <string>
#include <vector>

struct FrontendNote {
    std::string step;         // "C","D","E","F","G","A","B"
    int octave = 4;
    int alter = 0;            // -2,-1,0,1,2
    int durationSixteenths = 4;
    std::string durationType; // "16th","eighth","quarter","half","whole"
    bool tiedToNext = false;  // true when this note continues into the next segment
    bool isRest     = false;  // true when the fixed-voice source note is a rest
    bool dotted     = false;  // true for dotted durations (e.g. 12 sixteenths = dotted half)
};

struct FrontendVoices {
    std::vector<FrontendNote> soprano;
    std::vector<FrontendNote> alto;
    std::vector<FrontendNote> tenor;
    std::vector<FrontendNote> bass;
};

struct FrontendMeasure {
    int number = 1; // 0 = anacrusis pickup, 1..N = regular measures
    FrontendVoices voices;
    std::vector<std::string> chordNames;        // one entry per harmonic position
    std::vector<int>         chordTicks;        // within-measure start (sixteenths) per harmonic position
    std::vector<bool>        chordLabelVisible; // true when this position introduces a new chord
};

struct FrontendScore {
    std::string key;          // e.g. "C", "g", "F#", "bb"
    std::string mode;         // "major" or "minor"
    int beats = 4;
    int beatType = 4;
    int anacrusisSixteenths = 0;
    std::vector<FrontendMeasure> measures;
};

#endif
