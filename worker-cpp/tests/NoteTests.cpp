#include <catch2/catch_test_macros.hpp>
#include "domain/Note.h"

static Note makeNote(NoteName name, int octave, int alter = 0) {
    return Note(name, octave, alter, 1, Duration{}, true);
}

TEST_CASE("Note::getSemitone returns correct values for naturals", "[Note]") {
    REQUIRE(makeNote(NoteName::C, 4).getSemitone() == 48);
    REQUIRE(makeNote(NoteName::D, 4).getSemitone() == 50);
    REQUIRE(makeNote(NoteName::E, 4).getSemitone() == 52);
    REQUIRE(makeNote(NoteName::F, 4).getSemitone() == 53);
    REQUIRE(makeNote(NoteName::G, 4).getSemitone() == 55);
    REQUIRE(makeNote(NoteName::A, 4).getSemitone() == 57);
    REQUIRE(makeNote(NoteName::B, 4).getSemitone() == 59);
}

TEST_CASE("Note::getSemitone applies accidentals correctly", "[Note]") {
    REQUIRE(makeNote(NoteName::C, 4,  1).getSemitone() == 49);  // C#4
    REQUIRE(makeNote(NoteName::E, 4, -1).getSemitone() == 51);  // Eb4
    REQUIRE(makeNote(NoteName::B, 4,  1).getSemitone() == 60);  // B#4
    REQUIRE(makeNote(NoteName::C, 4, -1).getSemitone() == 47);  // Cb4
}

TEST_CASE("Note::getSemitone differs by 12 between adjacent octaves", "[Note]") {
    REQUIRE(makeNote(NoteName::C, 5).getSemitone() - makeNote(NoteName::C, 4).getSemitone() == 12);
    REQUIRE(makeNote(NoteName::G, 5).getSemitone() - makeNote(NoteName::G, 4).getSemitone() == 12);
}

TEST_CASE("Note comparison operators", "[Note]") {
    Note c4 = makeNote(NoteName::C, 4);
    Note e4 = makeNote(NoteName::E, 4);
    Note c5 = makeNote(NoteName::C, 5);
    Note c4b = makeNote(NoteName::C, 4);

    SECTION("operator<") {
        REQUIRE(c4 < e4);
        REQUIRE(c4 < c5);
        REQUIRE_FALSE(e4 < c4);
        REQUIRE_FALSE(c4 < c4b);
    }

    SECTION("operator>") {
        REQUIRE(e4 > c4);
        REQUIRE(c5 > c4);
        REQUIRE_FALSE(c4 > e4);
        REQUIRE_FALSE(c4 > c4b);
    }

    SECTION("operator==") {
        REQUIRE(c4 == c4b);
        REQUIRE_FALSE(c4 == e4);
        REQUIRE_FALSE(c4 == c5);
    }

    SECTION("operator!=") {
        REQUIRE(c4 != e4);
        REQUIRE(c4 != c5);
        REQUIRE_FALSE(c4 != c4b);
    }

    SECTION("operator<=") {
        REQUIRE(c4 <= e4);
        REQUIRE(c4 <= c4b);
        REQUIRE_FALSE(e4 <= c4);
    }

    SECTION("operator>=") {
        REQUIRE(e4 >= c4);
        REQUIRE(c4 >= c4b);
        REQUIRE_FALSE(c4 >= e4);
    }
}

TEST_CASE("Note enharmonic equality: C#4 == Db4", "[Note]") {
    Note cSharp = makeNote(NoteName::C, 4,  1);
    Note dFlat  = makeNote(NoteName::D, 4, -1);
    REQUIRE(cSharp == dFlat);
}
