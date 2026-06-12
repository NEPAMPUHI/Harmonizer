#ifndef HARM_SCORESERIALIZER_H
#define HARM_SCORESERIALIZER_H

#include "infrastructure/FrontendScore.h"
#include "domain/Score.h"
#include "domain/HarmonizationSettings.h"
#include "domain/Note.h"

class ScoreSerializer {
public:
    FrontendScore serialize(const Score& score, const HarmonizationSettings& settings) const;

private:
    FrontendNote toFrontendNote(const Note& note, int durationSixteenths) const;
    std::string noteNameToStep(NoteName name) const;
};

#endif
