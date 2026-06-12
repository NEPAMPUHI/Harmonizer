#include "domain/ActiveRuleSet.h"
#include <algorithm>

namespace {
bool contains(const std::vector<std::string>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}
} // namespace

ActiveRuleSet ActiveRuleSet::fromSettings(const HarmonizationSettings& settings) {
    const auto& fr = settings.forbiddenRules;
    ActiveRuleSet rs;
    rs.parallelFifths    = contains(fr, "parallel_fifths");
    rs.parallelOctaves   = contains(fr, "parallel_octaves");
    rs.parallelSeconds   = contains(fr, "parallel_seconds");
    rs.allVoicesSameDir  = contains(fr, "all_voices_same_dir");
    rs.voiceCrossing     = contains(fr, "voice_crossing");
    rs.chromaticTransfer = contains(fr, "chromatic_transfer");
    rs.hiddenIntervals   = contains(fr, "hidden_octaves");
    rs.bassLeapSequence  = contains(fr, "bass_leap_sequence");
    rs.largeIntervalSaAt = contains(fr, "large_interval_sa_at");
    rs.functionalRules   = contains(fr, "s_after_d");
    return rs;
}

ActiveRuleSet ActiveRuleSet::allEnabled() {
    return {};  // all fields default to true
}

ActiveRuleSet ActiveRuleSet::allDisabled() {
    ActiveRuleSet rs;
    rs.parallelFifths    = false;
    rs.parallelOctaves   = false;
    rs.parallelSeconds   = false;
    rs.allVoicesSameDir  = false;
    rs.voiceCrossing     = false;
    rs.chromaticTransfer = false;
    rs.hiddenIntervals   = false;
    rs.bassLeapSequence  = false;
    rs.largeIntervalSaAt = false;
    rs.functionalRules   = false;
    return rs;
}
