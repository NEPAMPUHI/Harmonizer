#include "checking/CheckChordIdentifier.h"
#include "data/ChordTemplateLibrary.h"
#include "harmonization/ChordBuilder.h"
#include "domain/HarmonizationJob.h"

namespace {

// Try to find a chord in the given candidate list whose SATB semitones all match pos.
bool tryMatch(const std::vector<Chord>& candidates,
              const CheckHarmonicPosition& pos,
              IdentifiedCheckChord& out,
              const ChordTemplate& tmpl) {
    for (const auto& chord : candidates) {
        if (chord.getSoprano() == pos.soprano &&
            chord.getAlto()    == pos.alto    &&
            chord.getTenor()   == pos.tenor   &&
            chord.getBass()    == pos.bass) {
            out.isKnownChord   = true;
            out.chord          = chord;
            out.matchedTemplate = tmpl;
            return true;
        }
    }
    return false;
}

// Beat classification helpers — mirrors HarmonicPositionBuilder logic.
bool isStrongBeat(int posInMeasure, const HarmonizationSettings& settings) {
    if (posInMeasure == 0) return true;
    const int beats    = settings.timeSignature.beats;
    const int beatType = settings.timeSignature.beatType;
    if (beatType == 4 && beats == 4  && posInMeasure == 8 ) return true;
    if (beatType == 8 && beats == 12 && posInMeasure == 12) return true;
    return false;
}

bool isMediumBeat(int posInMeasure, const HarmonizationSettings& settings) {
    const int beats    = settings.timeSignature.beats;
    const int beatType = settings.timeSignature.beatType;
    if (beatType == 4 && beats == 3  && posInMeasure == 4 ) return true;
    if (beatType == 8 && beats == 6  && posInMeasure == 6 ) return true;
    if (beatType == 8 && beats == 9  && (posInMeasure == 6  || posInMeasure == 12)) return true;
    if (beatType == 8 && beats == 12 && (posInMeasure == 6  || posInMeasure == 18)) return true;
    return false;
}

} // namespace

std::vector<IdentifiedCheckChord>
CheckChordIdentifier::identify(const std::vector<CheckHarmonicPosition>& positions,
                               const HarmonizationSettings& settings) const {
    const auto allTemplates = ChordTemplateLibrary::createAllTemplates();
    ChordBuilder builder;

    std::vector<IdentifiedCheckChord> result;
    result.reserve(positions.size());

    for (const auto& pos : positions) {
        IdentifiedCheckChord item;
        item.position = pos;

        // Any missing or resting voice → cannot identify
        if (!pos.hasSoprano || !pos.hasAlto || !pos.hasTenor || !pos.hasBass ||
             pos.soprano.isRest() || pos.alto.isRest() ||
             pos.tenor.isRest()   || pos.bass.isRest()) {
            result.push_back(item);
            continue;
        }

        // Bass-anchored: voiceBassChords fixes the bass and tries up to 3 tenor
        // positions, building alto/soprano upward.  This is the same algorithm
        // that ChordBuilder::buildAllValid() uses, so every chord that the
        // template library can produce is reachable from here.
        // A soprano-anchored pass is intentionally omitted: voiceMelodyChords
        // does not verify that the soprano is in the key, so it would accept
        // chromatic notes and produce false-positive identifications.
        //
        // K64 and T64 share identical SATB degree patterns. K64 is only valid on
        // strong/medium beats; T64 only on weak beats. When K64 matches but the
        // position is a weak beat, skip it so the loop continues to T64.
        for (const auto& tmpl : allTemplates) {
            // Skip templates whose bass degree doesn't match the actual bass note.
            // Without this, templates with matching upper-voice degrees but wrong bass
            // degree would steal the match (voiceBassChords passes the actual bass
            // note directly, so the bass always matches in tryMatch regardless of d[3]).
            if (pos.bass.getDegree() > 0
                && tmpl.degreesInSatbOrder[3] != pos.bass.getDegree())
                continue;

            auto cands = builder.createChordsFromTemplate(
                pos.bass, tmpl, HarmonizationMode::HarmonizeBass, settings);
            if (tryMatch(cands, pos, item, tmpl)) {
                if (tmpl.type == ChordType::CadentialSixFour) {
                    const bool strong = isStrongBeat(pos.positionInMeasureSixteenths, settings);
                    const bool medium = !strong && isMediumBeat(pos.positionInMeasureSixteenths, settings);
                    if (!strong && !medium) {
                        // Weak beat: this is T64, not K64. Reset and keep searching.
                        item = IdentifiedCheckChord{};
                        item.position = pos;
                        continue;
                    }
                }
                break;
            }
        }

        result.push_back(item);
    }

    return result;
}
