#include "application/JobValidator.h"
#include <cctype>
#include <stdexcept>

void JobValidator::validate(const HarmonizationJob& job) const {
    validateCommonFields(job);
    validateSettings(job.settings, job.mode);
    validateInput(job);
    validateAllowedChords(job.settings.allowedChords);
}

void JobValidator::validateCommonFields(const HarmonizationJob& job) const {
    if (job.jobId.empty())
        throw std::runtime_error("Job id is empty");
}

void JobValidator::validateSettings(const HarmonizationSettings& settings, HarmonizationMode mode) const {
    if (settings.key.empty())
        throw std::runtime_error("Key is empty");
    validateKeyAndScaleModes(settings, mode);
    if (settings.measureCount <= 0)
        throw std::runtime_error("Measure count must be greater than 0");
    if (settings.timeSignature.beats <= 0)
        throw std::runtime_error("Time signature beats must be greater than 0");
    if (settings.timeSignature.beatType <= 0)
        throw std::runtime_error("Time signature beat type must be greater than 0");
    if (settings.anacrusisSixteenths < 0)
        throw std::runtime_error("Anacrusis sixteenths must be >= 0");
}

void JobValidator::validateKeyAndScaleModes(const HarmonizationSettings& settings, HarmonizationMode mode) const {
    const std::string& key = settings.key;
    bool keyIsMajor = !key.empty() && std::isupper((unsigned char)key[0]);

    if (keyIsMajor) {
        if (!isSupportedMajorKey(key))
            throw std::runtime_error("Unsupported key");
    } else {
        if (!isSupportedMinorKey(key))
            throw std::runtime_error("Unsupported key");
    }

    // scaleModes is only required for harmonization modes.
    if (mode == HarmonizationMode::CheckSolution)
        return;

    if (settings.scaleModes.empty())
        throw std::runtime_error("Scale modes are empty");

    static const std::unordered_set<std::string> supportedModes = {
        "natural", "harmonic", "melodic"
    };
    for (const auto& m : settings.scaleModes) {
        if (supportedModes.count(m) == 0)
            throw std::runtime_error("Unsupported scale mode: " + m);
    }
}

bool JobValidator::isSupportedMajorKey(const std::string& key) const {
    static const std::unordered_set<std::string> majorKeys = {
        "C", "G", "D", "A", "E", "B", "F#", "C#",
        "F", "Bb", "Eb", "Ab", "Db", "Gb", "Cb"
    };
    return majorKeys.count(key) > 0;
}

bool JobValidator::isSupportedMinorKey(const std::string& key) const {
    static const std::unordered_set<std::string> minorKeys = {
        "a", "e", "b", "f#", "c#", "g#", "d#", "a#",
        "d", "g", "c", "f", "bb", "eb", "ab"
    };
    return minorKeys.count(key) > 0;
}

void JobValidator::validateInput(const HarmonizationJob& job) const {
    if (job.mode == HarmonizationMode::HarmonizeMelody ||
        job.mode == HarmonizationMode::HarmonizeBass) {
        if (job.input.notes.empty())
            throw std::runtime_error("Input notes are empty");
        validateNotes(job.input);
    }
}

void JobValidator::validateNotes(const ScoreInput& input) const {
    for (const auto& note : input.notes) {
        if (note.getOctave() < 0 || note.getOctave() > 8)
            throw std::runtime_error("Invalid note octave");

        const Duration d = note.getDuration();
        if (d.denominator <= 0 || d.numerator <= 0)
            throw std::runtime_error("Invalid note duration");
        int sixteenths = d.numerator * 16 / d.denominator;
        if (sixteenths <= 0 || sixteenths > 64)
            throw std::runtime_error("Invalid note duration");

        if (note.getAlter() < -2 || note.getAlter() > 2)
            throw std::runtime_error("Invalid note alteration");
    }
}

void JobValidator::validateAllowedChords(const std::vector<std::string>& allowedChords) const {
    if (allowedChords.empty())
        throw std::runtime_error("Allowed chords list is empty");
    for (const auto& chordName : allowedChords) {
        if (chordName.empty())
            throw std::runtime_error("Chord name is empty");
        if (!isSupportedChordName(chordName))
            throw std::runtime_error("Unsupported chord name");
    }
}

bool JobValidator::isSupportedChordName(const std::string& chordName) const {
    static const std::unordered_set<std::string> supported = {
        "T53", "T6", "T64",
        "S53", "S6", "S64",
        "D53", "D6", "D64", "D7"
    };
    return supported.count(chordName) > 0;
}
