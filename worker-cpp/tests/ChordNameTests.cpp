#include <catch2/catch_test_macros.hpp>
#include "domain/Chord.h"

static ChordTemplate makeTemplate(HarmonicFunction function, int degree, ChordType type) {
    return ChordTemplate{function, degree, type, Inversion::I, ChordPosition::Close, {1, 3, 5, 1}};
}

static Chord makeChord(HarmonicFunction function, int degree, ChordType type) {
    Note note;
    return Chord(note, note, note, note, makeTemplate(function, degree, type));
}

TEST_CASE("Chord::getName for tonic chords", "[Chord]") {
    REQUIRE(makeChord(HarmonicFunction::T, 1, ChordType::Triad).getName()   == "T53");
    REQUIRE(makeChord(HarmonicFunction::T, 1, ChordType::Six).getName()     == "T6");
    REQUIRE(makeChord(HarmonicFunction::T, 1, ChordType::SixFour).getName() == "T64");
    REQUIRE(makeChord(HarmonicFunction::T, 1, ChordType::Seventh).getName() == "T7");
}

TEST_CASE("Chord::getName for subdominant chords", "[Chord]") {
    REQUIRE(makeChord(HarmonicFunction::S, 4, ChordType::Triad).getName()   == "S53");
    REQUIRE(makeChord(HarmonicFunction::S, 4, ChordType::SixFour).getName() == "S64");
    REQUIRE(makeChord(HarmonicFunction::S, 4, ChordType::Seventh).getName() == "S7");
}

TEST_CASE("Chord::getName for dominant chords", "[Chord]") {
    REQUIRE(makeChord(HarmonicFunction::D, 5, ChordType::Triad).getName()   == "D53");
    REQUIRE(makeChord(HarmonicFunction::D, 5, ChordType::Seventh).getName() == "D7");
    REQUIRE(makeChord(HarmonicFunction::D, 5, ChordType::SixFive).getName() == "D65");
    REQUIRE(makeChord(HarmonicFunction::D, 5, ChordType::FourThree).getName() == "D43");
    REQUIRE(makeChord(HarmonicFunction::D, 5, ChordType::Two).getName()     == "D2");
}

TEST_CASE("Chord::getName for second degree", "[Chord]") {
    REQUIRE(makeChord(HarmonicFunction::S, 2, ChordType::Triad).getName() == "II53");
    REQUIRE(makeChord(HarmonicFunction::S, 2, ChordType::Six).getName()   == "II6");
}

TEST_CASE("Chord::getName for CadentialSixFour always returns K64", "[Chord]") {
    REQUIRE(makeChord(HarmonicFunction::T, 1, ChordType::CadentialSixFour).getName() == "K64");
    REQUIRE(makeChord(HarmonicFunction::D, 5, ChordType::CadentialSixFour).getName() == "K64");
}
