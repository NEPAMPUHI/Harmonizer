#include <catch2/catch_test_macros.hpp>
#include "harmonization/HarmonyGraph.h"
#include "domain/Chord.h"

static ChordTemplate makeTemplate(HarmonicFunction function, int degree, ChordType type) {
    return ChordTemplate{function, degree, type, Inversion::I, ChordPosition::Close, {1, 3, 5, 1}};
}

static Chord makeChord(HarmonicFunction function, int degree, ChordType type) {
    Note note;
    return Chord(note, note, note, note, makeTemplate(function, degree, type));
}

TEST_CASE("HarmonyGraph is empty when freshly constructed", "[HarmonyGraph]") {
    HarmonyGraph graph;
    REQUIRE(graph.empty());
    REQUIRE(graph.getLevels().empty());
    REQUIRE(graph.getEdges().empty());
}

TEST_CASE("HarmonyGraph::buildNodes creates correct level count", "[HarmonyGraph]") {
    HarmonyGraph graph;
    Chord c = makeChord(HarmonicFunction::T, 1, ChordType::Triad);

    graph.buildNodes({{c, c}, {c, c, c}, {c}});

    REQUIRE(graph.getLevels().size() == 3);
}

TEST_CASE("HarmonyGraph::buildNodes stores correct node counts per level", "[HarmonyGraph]") {
    HarmonyGraph graph;
    Chord c = makeChord(HarmonicFunction::T, 1, ChordType::Triad);

    graph.buildNodes({{c, c}, {c, c, c}, {c}});

    REQUIRE(graph.getLevels()[0].size() == 2);
    REQUIRE(graph.getLevels()[1].size() == 3);
    REQUIRE(graph.getLevels()[2].size() == 1);
}

TEST_CASE("HarmonyGraph::buildNodes assigns sequential nodeIds", "[HarmonyGraph]") {
    HarmonyGraph graph;
    Chord c = makeChord(HarmonicFunction::T, 1, ChordType::Triad);

    graph.buildNodes({{c, c}, {c, c, c}, {c}});

    int expectedId = 0;
    for (const auto& level : graph.getLevels()) {
        for (const auto& node : level) {
            REQUIRE(node.nodeId == expectedId++);
        }
    }
}

TEST_CASE("HarmonyGraph::buildNodes assigns correct position per level", "[HarmonyGraph]") {
    HarmonyGraph graph;
    Chord c = makeChord(HarmonicFunction::T, 1, ChordType::Triad);

    graph.buildNodes({{c}, {c}, {c}});

    for (int i = 0; i < 3; ++i) {
        REQUIRE(graph.getLevels()[i][0].position == i);
    }
}

TEST_CASE("HarmonyGraph is not empty after buildNodes", "[HarmonyGraph]") {
    HarmonyGraph graph;
    Chord c = makeChord(HarmonicFunction::T, 1, ChordType::Triad);

    graph.buildNodes({{c}});

    REQUIRE_FALSE(graph.empty());
}

TEST_CASE("HarmonyGraph::buildNodes with empty input stays empty", "[HarmonyGraph]") {
    HarmonyGraph graph;
    graph.buildNodes({});
    REQUIRE(graph.empty());
}

TEST_CASE("HarmonyGraph resets on repeated buildNodes calls", "[HarmonyGraph]") {
    HarmonyGraph graph;
    Chord c = makeChord(HarmonicFunction::T, 1, ChordType::Triad);

    graph.buildNodes({{c, c, c}});
    REQUIRE(graph.getLevels().size() == 1);

    graph.buildNodes({{c}, {c}});
    REQUIRE(graph.getLevels().size() == 2);

    int expectedId = 0;
    for (const auto& level : graph.getLevels()) {
        for (const auto& node : level) {
            REQUIRE(node.nodeId == expectedId++);
        }
    }
}
