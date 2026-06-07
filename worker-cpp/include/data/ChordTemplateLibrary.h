#ifndef HARM_CHORDTEMPLATELIBRARY_H
#define HARM_CHORDTEMPLATELIBRARY_H

#include <vector>

#include "ChordTemplate.h"
#include "ChordSelection.h"

using TemplatesBySoprano = std::array<std::vector<ChordTemplate>, 8>;
using TemplatesByBass = std::array<std::vector<ChordTemplate>, 8>;

class ChordTemplateLibrary {
public:
    static std::vector<ChordTemplate> createTemplates(const ChordSelection& selection);
    static std::vector<ChordTemplate> createAllTemplates();

    static TemplatesBySoprano createIndexBySoprano(const std::vector<ChordTemplate>& templates);
    static TemplatesByBass createIndexByBass(const std::vector<ChordTemplate>& templates);

private:
    static void addT53(std::vector<ChordTemplate>& templates);
    static void addS53(std::vector<ChordTemplate>& templates);
    static void addD53(std::vector<ChordTemplate>& templates);
    static void addII53(std::vector<ChordTemplate>& templates);
    static void addIII53(std::vector<ChordTemplate>& templates);
    static void addVI53(std::vector<ChordTemplate>& templates);

    static void addT6(std::vector<ChordTemplate>& templates);
    static void addS6(std::vector<ChordTemplate>& templates);
    static void addD6(std::vector<ChordTemplate>& templates);
    static void addII6(std::vector<ChordTemplate>& templates);
    static void addVII6(std::vector<ChordTemplate>& templates);

    static void addK64(std::vector<ChordTemplate>& templates);
    static void addT64(std::vector<ChordTemplate>& templates);
    static void addS64(std::vector<ChordTemplate>& templates);
    static void addD64(std::vector<ChordTemplate>& templates);

    static void addD7(std::vector<ChordTemplate>& templates);
    static void addD65(std::vector<ChordTemplate>& templates);
    static void addD43(std::vector<ChordTemplate>& templates);
    static void addD2(std::vector<ChordTemplate>& templates);
    static void addD9(std::vector<ChordTemplate>& templates);

    static void addII7(std::vector<ChordTemplate>& templates);
    static void addII65(std::vector<ChordTemplate>& templates);
    static void addII43(std::vector<ChordTemplate>& templates);
    static void addII2(std::vector<ChordTemplate>& templates);

    static void addVII7(std::vector<ChordTemplate>& templates);
    static void addVII65(std::vector<ChordTemplate>& templates);
    static void addVII43(std::vector<ChordTemplate>& templates);
    static void addVII2(std::vector<ChordTemplate>& templates);
};

#endif