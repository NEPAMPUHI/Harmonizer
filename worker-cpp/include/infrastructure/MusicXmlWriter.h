#ifndef HARM_MUSICXMLWRITER_H
#define HARM_MUSICXMLWRITER_H

#include <string>
#include "domain/HarmonizationJob.h"
#include "domain/HarmonizationSettings.h"
#include "domain/Note.h"
#include "domain/Score.h"

class MusicXmlWriter {
public:
    std::string writePlaceholderScoreToString(const std::string& jobId,
                                              const std::string& variantId) const;
    std::string writeInputMelodyToString(const HarmonizationJob& job,
                                         const std::string& variantId) const;
    std::string writeScoreToString(const Score& score,
                                   const HarmonizationSettings& settings,
                                   const std::string& jobId,
                                   const std::string& variantId) const;

private:
    std::string writeNote(const Note& note) const;
    std::string writeNoteWithDuration(const Note& note, int durationSixteenths) const;
    std::string writeSatbNote(const Note& note, int durationSixteenths,
                              int voice, const std::string& stem,
                              bool tieStop = false, bool tieStart = false) const;
    std::string noteNameToMusicXmlStep(NoteName name) const;
    int getMeasureCapacitySixteenths(const TimeSignature& timeSignature) const;
    int getNoteDurationSixteenths(const Note& note) const;
    int getMusicXmlDurationFromSixteenths(int durationSixteenths) const;
    std::string getMusicXmlTypeFromSixteenths(int durationSixteenths) const;
    int getKeyFifths(const HarmonizationSettings& settings) const;
};

#endif
