#ifndef HARM_CHORDBUILDER_H
#define HARM_CHORDBUILDER_H

#include <string>
#include <vector>
#include "domain/Chord.h"
#include "domain/ChordTemplate.h"
#include "domain/HarmonizationJob.h"
#include "domain/HarmonizationSettings.h"
#include "domain/Note.h"

class ChordBuilder {
public:
    std::vector<Chord> buildForFixedMelodyNote(const Note& melodyNote, const HarmonizationSettings& settings);
    std::vector<Chord> buildForFixedBassNote(const Note& bassNote, const HarmonizationSettings& settings);
    std::vector<Chord> buildAllValid(const HarmonizationSettings& settings);

    bool canUseTemplate(const ChordTemplate& chordTemplate, const Note& fixedNote,
                        HarmonizationMode mode, const HarmonizationSettings& settings) const;

    // Returns all valid chord voicings for this template+fixed note combination.
    // Melody mode: multiple bass-octave variants; Bass mode: multiple tenor-octave variants.
    std::vector<Chord> createChordsFromTemplate(const Note& fixedNote,
                                                const ChordTemplate& chordTemplate,
                                                HarmonizationMode mode,
                                                const HarmonizationSettings& settings) const;

    // Convenience wrapper — returns the first valid chord from createChordsFromTemplate.
    Chord createChordFromTemplate(const Note& fixedNote, const ChordTemplate& chordTemplate,
                                  HarmonizationMode mode,
                                  const HarmonizationSettings& settings) const;

private:
    std::vector<ChordTemplate> getAllowedTemplates(const HarmonizationSettings& settings) const;
};

#endif
