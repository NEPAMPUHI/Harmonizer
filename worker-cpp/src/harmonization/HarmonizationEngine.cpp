#include "HarmonizationEngine.h"

HarmonyGraph HarmonizationEngine::harmonizeMelody(const std::vector<Note> &melody) const {
    return harmonizeLine(melody, HarmonizationMode::Melody);
}

HarmonyGraph HarmonizationEngine::harmonizeBass(const std::vector<Note> &bassLine) const {
    return harmonizeLine(bassLine, HarmonizationMode::Bass);
}

std::vector<Chord> HarmonizationEngine::createChordsForNote(const Note &note, HarmonizationMode mode) const {
    int degree = note.getDegree();
    const auto &possibleTemplates = mode == HarmonizationMode::Melody
                                        ? templatesBySoprano[degree]
                                        : templatesByBass[degree];
    std::vector<Chord> chords;
    for (const ChordTemplate &chordTemplate: possibleTemplates) {
        chords.push_back(createChordFromTemplate(note, chordTemplate, mode));
    }
    return chords;
}

Chord HarmonizationEngine::createChordFromTemplate(const Note& fixedNote, const ChordTemplate& chordTemplate, HarmonizationMode mode) const {
    int sopranoDegree = chordTemplate.degreesInSatbOrder[0];
    int altoDegree = chordTemplate.degreesInSatbOrder[1];
    int tenorDegree = chordTemplate.degreesInSatbOrder[2];
    int bassDegree = chordTemplate.degreesInSatbOrder[3];

    Note soprano;
    Note alto;
    Note tenor;
    Note bass;

    if (mode == HarmonizationMode::Melody) {
        soprano = fixedNote;

        alto = createNoteByDegreeNear(altoDegree, fixedNote, VoiceType::Alto);

        tenor = createNoteByDegreeNear(
            tenorDegree,
            fixedNote,
            VoiceType::Tenor
        );

        bass = createNoteByDegreeNear(
            bassDegree,
            fixedNote,
            VoiceType::Bass
        );
    }
    else {
        bass = fixedNote;

        tenor = createNoteByDegreeNear(tenorDegree, fixedNote, VoiceType::Tenor
        );

        alto = createNoteByDegreeNear(
            altoDegree,
            fixedNote,
            VoiceType::Alto
        );

        soprano = createNoteByDegreeNear(
            sopranoDegree,
            fixedNote,
            VoiceType::Soprano
        );
    }

    return Chord(soprano, alto, tenor, bass, chordTemplate);
}
