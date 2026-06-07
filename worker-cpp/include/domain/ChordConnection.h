#ifndef HARM_CHORDCONNECTION_H
#define HARM_CHORDCONNECTION_H

struct ChordConnection {
    int fromLayer;
    int fromChordIndex;
    int toLayer;
    int toChordIndex;

    int score = 0;
};

#endif