#ifndef HARM_HARMONYENGINE_H
#define HARM_HARMONYENGINE_H

#include <array>
#include <vector>

#include "Note.h"
#include "Chord.h"
#include "ChordTemplate.h"
#include "ChordTemplateLibrary.h"
#include "HarmonyGraph.h"

// AlgoVoiceMode is local to the algorithm layer; not to be confused with
// domain/HarmonizationJob.h's HarmonizationMode enum.
enum class AlgoVoiceMode { Melody, Bass };

enum class VoiceType { Soprano, Alto, Tenor, Bass };

class HarmonyGraphBuilder {
private:
    std::vector<ChordTemplate> templates;
    TemplatesBySoprano templatesBySoprano;
    TemplatesByBass templatesByBass;

public:
    HarmonyGraphBuilder(const ChordSelection& selection);

    HarmonyGraph harmonizeMelody(const std::vector<Note>& melody) const;
    HarmonyGraph harmonizeBass(const std::vector<Note>& bassLine) const;

private:
    HarmonyGraph harmonizeLine(const std::vector<Note>& line, AlgoVoiceMode mode) const;
    std::vector<Chord> createChordsForNote(const Note& note, AlgoVoiceMode mode) const;
    Chord createChordFromTemplate(const Note& fixedNote, const ChordTemplate& tmpl, AlgoVoiceMode mode) const;
    Note createNoteByDegreeNear(int degree, const Note& fixedNote, VoiceType voiceType) const;
};

#endif

