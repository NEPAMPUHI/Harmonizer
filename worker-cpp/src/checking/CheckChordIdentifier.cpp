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
        for (const auto& tmpl : allTemplates) {
            auto cands = builder.createChordsFromTemplate(
                pos.bass, tmpl, HarmonizationMode::HarmonizeBass, settings);
            if (tryMatch(cands, pos, item, tmpl)) break;
        }

        result.push_back(item);
    }

    return result;
}
