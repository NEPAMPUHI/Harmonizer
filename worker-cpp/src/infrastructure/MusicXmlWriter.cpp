#include "infrastructure/MusicXmlWriter.h"
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

// ── helpers ──────────────────────────────────────────────────────────────────

int MusicXmlWriter::getKeyFifths(const HarmonizationSettings& settings) const {
    // Major keys (uppercase first char): fifths relative to C major
    static const std::unordered_map<std::string, int> majorFifths = {
        {"C", 0}, {"G", 1},  {"D", 2},  {"A", 3},  {"E", 4},  {"B", 5},  {"F#", 6}, {"C#", 7},
        {"F",-1}, {"Bb",-2}, {"Eb",-3}, {"Ab",-4}, {"Db",-5}, {"Gb",-6}, {"Cb",-7}
    };
    // Minor keys (lowercase first char), normalized to uppercase for lookup
    static const std::unordered_map<std::string, int> minorFifths = {
        {"A", 0}, {"E", 1},  {"B", 2},  {"F#", 3}, {"C#", 4}, {"G#", 5}, {"D#", 6}, {"A#", 7},
        {"D",-1}, {"G",-2},  {"C",-3},  {"F",-4},  {"Bb",-5}, {"Eb",-6}, {"Ab",-7}
    };

    const std::string& key = settings.key;
    bool isMajor = !key.empty() && std::isupper((unsigned char)key[0]);

    // Normalize first char to uppercase for table lookup
    std::string normalizedKey = key;
    if (!normalizedKey.empty())
        normalizedKey[0] = (char)std::toupper((unsigned char)normalizedKey[0]);

    const auto& table = isMajor ? majorFifths : minorFifths;
    auto it = table.find(normalizedKey);
    if (it == table.end())
        throw std::runtime_error("Unsupported key: " + key);
    return it->second;
}

int MusicXmlWriter::getMeasureCapacitySixteenths(const TimeSignature& ts) const {
    // beats * (16 / beatType): e.g. 4/4 = 16, 3/4 = 12, 6/8 = 12
    if (ts.beatType <= 0) return 16;
    return ts.beats * (16 / ts.beatType);
}

int MusicXmlWriter::getNoteDurationSixteenths(const Note& note) const {
    const Duration d = note.getDuration();
    if (d.denominator <= 0 || d.numerator <= 0) return 4; // fallback: quarter
    int sixteenths = d.numerator * 16 / d.denominator;
    return (sixteenths > 0) ? sixteenths : 4;
}

std::string MusicXmlWriter::noteNameToMusicXmlStep(NoteName name) const {
    switch (name) {
        case NoteName::C: return "C";
        case NoteName::D: return "D";
        case NoteName::E: return "E";
        case NoteName::F: return "F";
        case NoteName::G: return "G";
        case NoteName::A: return "A";
        case NoteName::B: return "B";
    }
    return "C";
}

int MusicXmlWriter::getMusicXmlDurationFromSixteenths(int durationSixteenths) const {
    if (durationSixteenths <= 0) return 4;
    return durationSixteenths;
}

std::string MusicXmlWriter::getMusicXmlTypeFromSixteenths(int durationSixteenths) const {
    switch (durationSixteenths) {
        case 16: return "whole";
        case  8: return "half";
        case  4: return "quarter";
        case  2: return "eighth";
        case  1: return "16th";
        default: return "quarter";
    }
}

std::string MusicXmlWriter::writeNoteWithDuration(const Note& note, int durationSixteenths) const {
    std::ostringstream xml;
    xml << "      <note>\n";
    xml << "        <pitch>\n";
    xml << "          <step>" << noteNameToMusicXmlStep(note.getName()) << "</step>\n";
    if (note.getAlter() != 0)
        xml << "          <alter>" << note.getAlter() << "</alter>\n";
    xml << "          <octave>" << note.getOctave() << "</octave>\n";
    xml << "        </pitch>\n";
    xml << "        <duration>" << getMusicXmlDurationFromSixteenths(durationSixteenths) << "</duration>\n";
    xml << "        <type>" << getMusicXmlTypeFromSixteenths(durationSixteenths) << "</type>\n";
    xml << "      </note>\n";
    return xml.str();
}

std::string MusicXmlWriter::writeNote(const Note& note) const {
    return writeNoteWithDuration(note, getNoteDurationSixteenths(note));
}

// ── public methods ────────────────────────────────────────────────────────────

std::string MusicXmlWriter::writePlaceholderScoreToString(const std::string& jobId,
                                                           const std::string& variantId) const {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<!DOCTYPE score-partwise PUBLIC\n";
    xml << "  \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\"\n";
    xml << "  \"http://www.musicxml.org/dtds/partwise.dtd\">\n";
    xml << "<!-- jobId: " << jobId << " | variantId: " << variantId << " -->\n";
    xml << "<score-partwise version=\"4.0\">\n";
    xml << "  <part-list>\n";
    xml << "    <score-part id=\"P1\"><part-name>Music</part-name></score-part>\n";
    xml << "  </part-list>\n";
    xml << "  <part id=\"P1\">\n";
    xml << "    <measure number=\"1\">\n";
    xml << "      <attributes>\n";
    xml << "        <divisions>1</divisions>\n";
    xml << "        <key><fifths>0</fifths></key>\n";
    xml << "        <time><beats>4</beats><beat-type>4</beat-type></time>\n";
    xml << "        <clef><sign>G</sign><line>2</line></clef>\n";
    xml << "      </attributes>\n";
    xml << "      <note><rest/><duration>4</duration><type>whole</type></note>\n";
    xml << "    </measure>\n";
    xml << "  </part>\n";
    xml << "</score-partwise>\n";
    return xml.str();
}

std::string MusicXmlWriter::writeInputMelodyToString(const HarmonizationJob& job,
                                                      const std::string& variantId) const {
    const auto& ts = job.settings.timeSignature;
    std::ostringstream xml;

    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<!DOCTYPE score-partwise PUBLIC\n";
    xml << "  \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\"\n";
    xml << "  \"http://www.musicxml.org/dtds/partwise.dtd\">\n";
    xml << "<!-- jobId: " << job.jobId << " | variantId: " << variantId
        << " | input melody (not yet harmonized) -->\n";
    xml << "<score-partwise version=\"4.0\">\n";
    xml << "  <part-list>\n";
    xml << "    <score-part id=\"P1\"><part-name>Melody</part-name></score-part>\n";
    xml << "  </part-list>\n";
    xml << "  <part id=\"P1\">\n";

    if (job.input.notes.empty()) {
        // Empty input — write a single measure with a whole rest
        xml << "    <measure number=\"1\">\n";
        xml << "      <attributes>\n";
        xml << "        <divisions>4</divisions>\n";
        xml << "        <key><fifths>" << getKeyFifths(job.settings) << "</fifths></key>\n";
        xml << "        <time><beats>" << ts.beats << "</beats>"
            << "<beat-type>" << ts.beatType << "</beat-type></time>\n";
        xml << "        <clef><sign>G</sign><line>2</line></clef>\n";
        xml << "      </attributes>\n";
        xml << "      <note><rest/><duration>16</duration><type>whole</type></note>\n";
        xml << "    </measure>\n";
    } else {
        const bool hasAnacrusis  = (job.settings.anacrusisSixteenths > 0);
        const int fullCapacity   = getMeasureCapacitySixteenths(ts);

        // Anacrusis uses measure number 0; normal start uses 1.
        int measureNumber    = hasAnacrusis ? 0 : 1;
        int currentCapacity  = hasAnacrusis ? job.settings.anacrusisSixteenths : fullCapacity;
        int usedSixteenths   = 0;
        bool measureOpen     = false;
        bool firstMeasure    = true;

        for (const auto& note : job.input.notes) {
            int noteSixteenths = getNoteDurationSixteenths(note);

            if (!measureOpen) {
                xml << "    <measure number=\"" << measureNumber << "\">\n";
                measureOpen = true;
            } else if (usedSixteenths + noteSixteenths > currentCapacity) {
                xml << "    </measure>\n";
                measureNumber++;
                usedSixteenths  = 0;
                currentCapacity = fullCapacity; // anacrusis is done after measure 0
                xml << "    <measure number=\"" << measureNumber << "\">\n";
                // TODO: support split/tie for notes longer than measure capacity (including anacrusis)
            }

            if (firstMeasure) {
                xml << "      <attributes>\n";
                xml << "        <divisions>4</divisions>\n";
                xml << "        <key><fifths>0</fifths></key>\n";
                xml << "        <time><beats>" << ts.beats << "</beats>"
                    << "<beat-type>" << ts.beatType << "</beat-type></time>\n";
                xml << "        <clef><sign>G</sign><line>2</line></clef>\n";
                xml << "      </attributes>\n";
                firstMeasure = false;
            }

            xml << writeNote(note);
            usedSixteenths += noteSixteenths;
        }

        if (measureOpen)
            xml << "    </measure>\n";
    }

    xml << "  </part>\n";
    xml << "</score-partwise>\n";
    return xml.str();
}

std::string MusicXmlWriter::writeScoreToString(const Score& score,
                                                const std::string& jobId,
                                                const std::string& variantId) const {
    if (score.empty())
        return writePlaceholderScoreToString(jobId, variantId);

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<!DOCTYPE score-partwise PUBLIC\n";
    xml << "  \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\"\n";
    xml << "  \"http://www.musicxml.org/dtds/partwise.dtd\">\n";
    xml << "<!-- jobId: " << jobId << " | variantId: " << variantId << " -->\n";
    xml << "<score-partwise version=\"4.0\">\n";
    xml << "  <part-list>\n";
    xml << "    <score-part id=\"P1\"><part-name>Soprano</part-name></score-part>\n";
    xml << "  </part-list>\n";
    xml << "  <part id=\"P1\">\n";
    xml << "    <measure number=\"1\">\n";
    xml << "      <attributes>\n";
    xml << "        <divisions>4</divisions>\n";
    xml << "        <key><fifths>0</fifths></key>\n";
    xml << "        <time><beats>4</beats><beat-type>4</beat-type></time>\n";
    xml << "        <clef><sign>G</sign><line>2</line></clef>\n";
    xml << "      </attributes>\n";
    const bool hasPositions = (score.positions.size() == score.chords.size());
    for (size_t i = 0; i < score.chords.size(); ++i) {
        const Note soprano = score.chords[i].getSoprano();
        if (hasPositions)
            xml << writeNoteWithDuration(soprano, score.positions[i].durationSixteenths);
        else
            xml << writeNote(soprano);
    }
    xml << "    </measure>\n";
    xml << "  </part>\n";
    xml << "</score-partwise>\n";
    return xml.str();
}
