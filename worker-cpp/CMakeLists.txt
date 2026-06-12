cmake_minimum_required(VERSION 3.20)
project(HarmonizerWorker)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

include_directories(
    include
    include/domain
    include/harmonization
    include/application
    include/infrastructure
    include/data
)

add_executable(harmonizer_worker
    src/main.cpp

    src/domain/Note.cpp
    src/domain/Chord.cpp
    src/domain/Score.cpp
    src/domain/HarmonyRules.cpp
    src/domain/ActiveRuleSet.cpp

    src/harmonization/ScaleDegreeCalculator.cpp
    src/harmonization/ChordBuilder.cpp
    src/harmonization/NoteDegreesResolver.cpp
    src/harmonization/HarmonicRhythmPlanner.cpp
    src/harmonization/HarmonicRhythmValidator.cpp
    src/harmonization/HarmonicPositionBuilder.cpp
    src/harmonization/MelodyHarmonizer.cpp
    src/harmonization/BassHarmonizer.cpp
    src/harmonization/VariantScorer.cpp
    src/harmonization/HarmonyGraph.cpp
    src/harmonization/HarmonyVariantPlanner.cpp

    src/checking/CheckHarmonicPositionBuilder.cpp
    src/checking/CheckChordIdentifier.cpp
    src/checking/CheckSolutionRuleChecker.cpp

    src/application/Worker.cpp
    src/application/TaskDispatcher.cpp
    src/application/HarmonizationEngine.cpp
    src/application/JobValidator.cpp

    src/infrastructure/InOut.cpp
    src/infrastructure/JobParser.cpp
    src/infrastructure/Results.cpp
    src/infrastructure/ResultStorage.cpp
    src/infrastructure/MusicXmlWriter.cpp
    src/infrastructure/ScoreSerializer.cpp

    src/data/ChordTemplateLibrary.cpp
)

target_include_directories(harmonizer_worker PRIVATE include)
target_link_libraries(harmonizer_worker PRIVATE nlohmann_json::nlohmann_json)

add_executable(harmonizer_tests
    src/domain/Note.cpp
    src/domain/Chord.cpp
    src/domain/Score.cpp
    src/domain/HarmonyRules.cpp
    src/domain/ActiveRuleSet.cpp
    src/harmonization/HarmonyGraph.cpp
    src/harmonization/ScaleDegreeCalculator.cpp
    src/harmonization/ChordBuilder.cpp
    src/harmonization/NoteDegreesResolver.cpp
    src/harmonization/HarmonicRhythmPlanner.cpp
    src/harmonization/HarmonicRhythmValidator.cpp
    src/harmonization/HarmonicPositionBuilder.cpp
    src/harmonization/MelodyHarmonizer.cpp
    src/harmonization/BassHarmonizer.cpp
    src/harmonization/VariantScorer.cpp
    src/harmonization/HarmonyVariantPlanner.cpp
    src/data/ChordTemplateLibrary.cpp
    src/checking/CheckHarmonicPositionBuilder.cpp
    src/checking/CheckChordIdentifier.cpp
    src/checking/CheckSolutionRuleChecker.cpp
    src/application/TaskDispatcher.cpp
    src/application/HarmonizationEngine.cpp
    src/application/JobValidator.cpp
    src/infrastructure/JobParser.cpp
    src/infrastructure/Results.cpp
    src/infrastructure/ResultStorage.cpp
    src/infrastructure/MusicXmlWriter.cpp
    src/infrastructure/ScoreSerializer.cpp

    tests/HarmonicRhythmPlannerTests.cpp
    tests/TiedNotesAndRestsTests.cpp
    tests/SplitLongNotesTests.cpp
    tests/CadenceSplitPatternTests.cpp
    tests/HarmonicRhythmPlanTests.cpp
    tests/MultiPlanIntegrationTests.cpp
    tests/RhythmAwareRankingTests.cpp
    tests/SegmentOutputTests.cpp
    tests/HarmonicRhythmValidatorTests.cpp
    tests/NoteTests.cpp
    tests/ChordNameTests.cpp
    tests/HarmonyGraphTests.cpp
    tests/ScaleDegreeTests.cpp
    tests/NoteDegreesResolverTests.cpp
    tests/ChordBuilderTests.cpp
    tests/HarmonyRulesTests.cpp
    tests/PipelineIntegrationTests.cpp
    tests/ScoreSerializerTests.cpp
    tests/VariantScorerTests.cpp
    tests/CheckSolutionParserTests.cpp
    tests/CheckHarmonicPositionBuilderTests.cpp
    tests/CheckChordIdentifierTests.cpp
    tests/CheckSolutionRuleCheckerTests.cpp
    tests/CheckParallelIntervalTests.cpp
    tests/CheckAllVoicesSameDirectionTests.cpp
    tests/CheckNewDiagnosticsTests.cpp
    tests/ForbiddenRulesTests.cpp
)

target_include_directories(harmonizer_tests PRIVATE include)
target_link_libraries(harmonizer_tests PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json)

enable_testing()
add_test(NAME harmonizer_tests COMMAND harmonizer_tests)