#include "infrastructure/MusicXmlWriter.h"
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

// ── helpers ──────────────────────────────────────────────────────────────────

int MusicXmlWriter::getKeyFifths(const HarmonizationSettings& settings) const {
    static const std::unordered_map<std::string, int> majorFifths = {
        {"C", 0}, {"G", 1},  {"D", 2},  {"A", 3},  {"E", 4},  {"B", 5},  {"F#", 6}, {"C#", 7},
        {"F",-1}, {"Bb",-2}, {"Eb",-3}, {"Ab",-4}, {"Db",-5}, {"Gb",-6}, {"Cb",-7}
    };
    static const std::unordered_map<std::string, int> minorFifths = {
        {"A", 0}, {"E", 1},  {"B", 2},  {"F#", 3}, {"C#", 4}, {"G#", 5}, {"D#", 6}, {"A#", 7},
        {"D",-1}, {"G",-2},  {"C",-3},  {"F",-4},  {"Bb",-5}, {"Eb",-6}, {"Ab",-7}
    };

    const std::string& key = settings.key;
    bool isMajor = !key.empty() && std::isupper((unsigned char)key[0]);

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
    if (ts.beatType <= 0) return 16;
    return ts.beats * (16 / ts.beatType);
}

int MusicXmlWriter::getNoteDurationSixteenths(const Note& note) const {
    const int s = note.getDurationSixteenths();
    return (s > 0) ? s : 4;
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

std::string MusicXmlWriter::writeSatbNote(const Note& note, int dur,
                                           int voice, const std::string& stem,
                                           bool tieStop, bool tieStart) const {
    std::ostringstream n;
    n << "      <note>\n";
    n << "        <pitch><step>" << noteNameToMusicXmlStep(note.getName()) << "</step>";
    if (note.getAlter() != 0)
        n << "<alter>" << note.getAlter() << "</alter>";
    n << "<octave>" << note.getOctave() << "</octave></pitch>\n";
    n << "        <duration>" << dur << "</duration>\n";
    if (tieStop)  n << "        <tie type=\"stop\"/>\n";
    if (tieStart) n << "        <tie type=\"start\"/>\n";
    n << "        <voice>" << voice << "</voice>\n";
    n << "        <type>" << getMusicXmlTypeFromSixteenths(dur) << "</type>\n";
    n << "        <stem>" << stem << "</stem>\n";
    if (tieStop || tieStart) {
        n << "        <notations>\n";
        if (tieStop)  n << "          <tied type=\"stop\"/>\n";
        if (tieStart) n << "          <tied type=\"start\"/>\n";
        n << "        </notations>\n";
    }
    n << "      </note>\n";
    return n.str();
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

        int measureNumber   = hasAnacrusis ? 0 : 1;
        int currentCapacity = hasAnacrusis ? job.settings.anacrusisSixteenths : fullCapacity;
        int usedSixteenths  = 0;
        bool measureOpen    = false;
        bool firstMeasure   = true;

        for (const auto& note : job.input.notes) {
            int noteSixteenths = getNoteDurationSixteenths(note);

            if (!measureOpen) {
                xml << "    <measure number=\"" << measureNumber << "\">\n";
                measureOpen = true;
            } else if (usedSixteenths + noteSixteenths > currentCapacity) {
                xml << "    </measure>\n";
                measureNumber++;
                usedSixteenths  = 0;
                currentCapacity = fullCapacity;
                xml << "    <measure number=\"" << measureNumber << "\">\n";
            }

            if (firstMeasure) {
                xml << "      <attributes>\n";
                xml << "        <divisions>4</divisions>\n";
                xml << "        <key><fifths>" << getKeyFifths(job.settings) << "</fifths></key>\n";
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
                                                const HarmonizationSettings& settings,
                                                const std::string& jobId,
                                                const std::string& variantId) const {
    if (score.empty())
        return writePlaceholderScoreToString(jobId, variantId);

    const bool hasPositions = (score.positions.size() == score.chords.size());
    const bool hasAnacrusis = (settings.anacrusisSixteenths > 0);
    const auto& ts          = settings.timeSignature;
    const int fifths        = getKeyFifths(settings);
    const bool isMajor      = !settings.key.empty()
                              && std::isupper((unsigned char)settings.key[0]);

    // Group chord indices by measureIndex; std::map keeps keys sorted.
    std::map<int, std::vector<size_t>> groups;
    for (size_t i = 0; i < score.chords.size(); ++i)
        groups[hasPositions ? score.positions[i].measureIndex : 0].push_back(i);

    // Precompute tie flags for the fixed voice across all positions.
    // fixedVoiceIndex: 0=soprano (melody), 3=bass, -1=none.
    const int fvi = score.fixedVoiceIndex;
    const size_t nPos = score.positions.size();
    std::vector<bool> fixedTieStop (nPos, false);
    std::vector<bool> fixedTieStart(nPos, false);
    if (hasPositions && fvi >= 0) {
        for (size_t i = 0; i < nPos; ++i) {
            const HarmonicPosition& pos = score.positions[i];
            if (pos.fixedNote.isRest() || pos.sourceNoteIndex < 0) continue;
            if (pos.offsetInFixedNoteSixteenths > 0)
                fixedTieStop[i] = true;
            if (i + 1 < nPos) {
                const HarmonicPosition& nxt = score.positions[i + 1];
                if (!nxt.fixedNote.isRest() &&
                    nxt.sourceNoteIndex == pos.sourceNoteIndex)
                    fixedTieStart[i] = true;
            }
        }
    }

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<!DOCTYPE score-partwise PUBLIC\n";
    xml << "  \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\"\n";
    xml << "  \"http://www.musicxml.org/dtds/partwise.dtd\">\n";
    xml << "<!-- jobId: " << jobId << " | variantId: " << variantId << " -->\n";
    xml << "<score-partwise version=\"4.0\">\n";
    xml << "  <part-list>\n";
    xml << "    <score-part id=\"P1\"><part-name>Soprano/Alto</part-name></score-part>\n";
    xml << "    <score-part id=\"P2\"><part-name>Tenor/Bass</part-name></score-part>\n";
    xml << "  </part-list>\n";

    // Write two parts: P1 = treble (Soprano/Alto), P2 = bass (Tenor/Bass).
    for (int partIdx = 0; partIdx < 2; ++partIdx) {
        const bool        isTreble = (partIdx == 0);
        const std::string partId   = isTreble ? "P1" : "P2";
        const char        clefSign = isTreble ? 'G' : 'F';
        const int         clefLine = isTreble ? 2 : 4;

        xml << "  <part id=\"" << partId << "\">\n";
        bool firstMeasure = true;

        for (auto& [mi, indices] : groups) {
            const int mn = hasAnacrusis ? mi : mi + 1;
            xml << "    <measure number=\"" << mn << "\">\n";

            if (firstMeasure) {
                xml << "      <attributes>\n";
                xml << "        <divisions>4</divisions>\n";
                xml << "        <key><fifths>" << fifths << "</fifths>"
                    << "<mode>" << (isMajor ? "major" : "minor") << "</mode></key>\n";
                xml << "        <time><beats>" << ts.beats << "</beats>"
                    << "<beat-type>" << ts.beatType << "</beat-type></time>\n";
                xml << "        <clef><sign>" << clefSign << "</sign>"
                    << "<line>" << clefLine << "</line></clef>\n";
                xml << "      </attributes>\n";
                firstMeasure = false;
            }

            // Sum durations for the backup element.
            int totalDur = 0;
            for (size_t i : indices)
                totalDur += (hasPositions ? score.positions[i].durationSixteenths : 4);

            // Voice 1 — soprano (P1) or tenor (P2), stems up.
            // Soprano gets fixed-voice ties in melody mode (fvi==0).
            for (size_t i : indices) {
                const int dur = hasPositions ? score.positions[i].durationSixteenths : 4;
                const Note& v1 = isTreble ? score.chords[i].getSoprano()
                                          : score.chords[i].getTenor();
                const bool ts = isTreble && fvi == 0 && i < nPos ? fixedTieStop[i]  : false;
                const bool tk = isTreble && fvi == 0 && i < nPos ? fixedTieStart[i] : false;
                xml << writeSatbNote(v1, dur, 1, "up", ts, tk);
            }

            // Backup to the start of the measure.
            xml << "      <backup><duration>" << totalDur << "</duration></backup>\n";

            // Voice 2 — alto (P1) or bass (P2), stems down.
            // Bass gets fixed-voice ties in bass mode (fvi==3).
            for (size_t i : indices) {
                const int dur = hasPositions ? score.positions[i].durationSixteenths : 4;
                const Note& v2 = isTreble ? score.chords[i].getAlto()
                                          : score.chords[i].getBass();
                const bool ts = !isTreble && fvi == 3 && i < nPos ? fixedTieStop[i]  : false;
                const bool tk = !isTreble && fvi == 3 && i < nPos ? fixedTieStart[i] : false;
                xml << writeSatbNote(v2, dur, 2, "down", ts, tk);
            }

            xml << "    </measure>\n";
        }
        xml << "  </part>\n";
    }

    xml << "</score-partwise>\n";
    return xml.str();
}
