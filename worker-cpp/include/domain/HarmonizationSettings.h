#ifndef HARM_HARMONIZATIONSETTINGS_H
#define HARM_HARMONIZATIONSETTINGS_H

#include <string>
#include <vector>

struct TimeSignature {
    int beats;
    int beatType;
};

struct HarmonizationSettings {
    std::string key;
    std::vector<std::string> scaleModes;
    int measureCount;
    TimeSignature timeSignature;
    int anacrusisSixteenths;
    std::vector<std::string> forbiddenRules;
    std::vector<std::string> allowedChords;
    bool splitLongNotesByBasePulse  = false;
};

#endif
