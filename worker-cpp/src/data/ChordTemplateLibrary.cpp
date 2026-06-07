#include "ChordTemplateLibrary.h"

TemplatesBySoprano ChordTemplateLibrary::createIndexBySoprano(const std::vector<ChordTemplate>& templates) {
    TemplatesBySoprano index;
    for (const ChordTemplate& chordTemplate : templates) {
        int sopranoDegree = chordTemplate.degreesInSatbOrder[0];
        index[sopranoDegree].push_back(chordTemplate);
    }
    return index;
}

TemplatesByBass ChordTemplateLibrary::createIndexByBass(const std::vector<ChordTemplate>& templates) {
    TemplatesByBass index;
    for (const ChordTemplate& chordTemplate : templates) {
        int bassDegree = chordTemplate.degreesInSatbOrder[3];
        index[bassDegree].push_back(chordTemplate);
    }
    return index;
}

std::vector<ChordTemplate> ChordTemplateLibrary::createTemplates(
    const ChordSelection& selection
) {
    std::vector<ChordTemplate> templates;

    if (selection.t53) addT53(templates);
    if (selection.s53) addS53(templates);
    if (selection.d53) addD53(templates);
    if (selection.ii53) addII53(templates);
    if (selection.iii53) addIII53(templates);
    if (selection.vi53) addVI53(templates);

    if (selection.t6) addT6(templates);
    if (selection.s6) addS6(templates);
    if (selection.d6) addD6(templates);
    if (selection.ii6) addII6(templates);
    if (selection.vii6) addVII6(templates);

    if (selection.k64) addK64(templates);
    if (selection.t64) addT64(templates);
    if (selection.s64) addS64(templates);
    if (selection.d64) addD64(templates);

    if (selection.d7) addD7(templates);
    if (selection.d65) addD65(templates);
    if (selection.d43) addD43(templates);
    if (selection.d2) addD2(templates);
    if (selection.d9) addD9(templates);

    if (selection.ii7) addII7(templates);
    if (selection.ii65) addII65(templates);
    if (selection.ii43) addII43(templates);
    if (selection.ii2) addII2(templates);

    if (selection.vii7) addVII7(templates);
    if (selection.vii65) addVII65(templates);
    if (selection.vii43) addVII43(templates);
    if (selection.vii2) addVII2(templates);

    return templates;
}

std::vector<ChordTemplate> ChordTemplateLibrary::createAllTemplates() {
    return createTemplates(ChordSelection{});
}

void ChordTemplateLibrary::addT53(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // T53 I close
       {
            HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::I,
           ChordPosition::Close,
           {1, 5, 3, 1}
       },

       // T53 I wide
       {
           HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::I,
           ChordPosition::Wide,
           {1, 3, 5, 1}
       },

       // T53 III close
       {
           HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::III,
           ChordPosition::Close,
           {3, 1, 5, 1}
       },

       // T53 III wide
       {
           HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::III,
           ChordPosition::Wide,
           {3, 5, 1, 1}
       },

       // T53 V close
       {
           HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::V,
           ChordPosition::Close,
           {5, 3, 1, 1}
       },

       // T53 V wide
       {
           HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::V,
           ChordPosition::Wide,
           {5, 1, 3, 1}
       },
    });
}

void ChordTemplateLibrary::addS53(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // S53 I close
        {
            HarmonicFunction::S,
            4,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Close,
            {4, 1, 6, 4}
        },

        // S53 I wide
        {
            HarmonicFunction::S,
            4,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Wide,
            {4, 6, 1, 4}
        },

        // S53 III close
        {
            HarmonicFunction::S,
            4,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Close,
            {6, 4, 1, 4}
        },

        // S53 III wide
        {
            HarmonicFunction::S,
            4,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {6, 1, 4, 4}
        },

        // S53 V close
        {
            HarmonicFunction::S,
            4,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Close,
            {1, 6, 4, 1}
        },

        // S53 V wide
        {
            HarmonicFunction::S,
            4,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Wide,
            {1, 4, 6, 4}
        },
    });
}

void ChordTemplateLibrary::addD53(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // D53 I close
        {
            HarmonicFunction::D,
            5,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Close,
            {5, 2, 7, 5}
        },

        // D53 I wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Wide,
            {5, 7, 2, 5}
        },

        // D53 III close
        {
            HarmonicFunction::D,
            5,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Close,
            {7, 5, 2, 5}
        },

        // D53 III wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {7, 2, 5, 5}
        },

        // D53 V close
        {
            HarmonicFunction::D,
            5,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Close,
            {2, 7, 5, 5}
        },

        // D53 V wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Wide,
            {2, 5, 7, 5}
        },
    });
}

void ChordTemplateLibrary::addII53(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // II53 I close
        {
            HarmonicFunction::S,
            2,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Close,
            {2, 6, 4, 2}
        },

        // II53 I wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Wide,
            {2, 4, 6, 2}
        },

        // II53 III close
        {
            HarmonicFunction::S,
            2,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Close,
            {4, 2, 6, 2}
        },

        // II53 III wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {4, 6, 2, 2}
        },

        // II53 V close
        {
            HarmonicFunction::S,
            2,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Close,
            {6, 4, 2, 2}
        },

        // II53 V wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Wide,
            {6, 2, 4, 2}
        },
    });
}

void ChordTemplateLibrary::addIII53(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // III53 I close
        {
            HarmonicFunction::T,
            3,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Close,
            {3, 7, 5, 3}
        },

        // III53 I wide
        {
            HarmonicFunction::T,
            3,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Wide,
            {3, 5, 7, 3}
        },

        // III53 III close
        {
            HarmonicFunction::T,
            3,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Close,
            {5, 3, 7, 3}
        },

        // III53 III wide
        {
            HarmonicFunction::T,
            3,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {5, 7, 3, 3}
        },

        // III53 V close
        {
            HarmonicFunction::T,
            3,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Close,
            {7, 5, 3, 3}
        },

        // III53 V wide
        {
            HarmonicFunction::T,
            3,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Wide,
            {7, 3, 5, 3}
        },
    });
}

void ChordTemplateLibrary::addVI53(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // VI53 I close
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Close,
            {6, 3, 1, 6}
        },

        // VI53 I Wide
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::I,
            ChordPosition::Wide,
            {6, 1, 3, 6}
        },

        // VI53 III close
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Close,
            {1, 6, 3, 6}
        },

        // VI53 III wide
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {1, 3, 6, 6}
        },

        // VI53 V close
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Close,
            {3, 1, 6, 6}
        },

        // VI53 V wide
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Wide,
            {3, 6, 1, 6}
        },

        // VI53 III close (double III)
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Close,
            {1, 1, 3, 6}
        },

        // VI53 III wide (double III)
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {1, 3, 1, 6}
        },

        // VI53 III close (double III)
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {1, 1, 3, 6}
        },

        // VI53 V close (double III)
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Close,
            {3, 1, 1, 6}
        },

        // VI53 V wide (double III)
        {
            HarmonicFunction::T,
            6,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Wide,
            {3, 1, 1, 6}
        },
    });
}

void ChordTemplateLibrary::addT6(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // T6 I close (double I)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {1, 1, 5, 3}
        },

        // T6 I close (double V)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {1, 5, 5, 3}
        },

        // T6 V close (double V)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {5, 5, 1, 3}
        },

        // T6 V close (double I)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {5, 1, 1, 3}
        },

        // T6 I mixed
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Mixed,
            {1, 5, 1, 3}
        },

        // T6 V mixed
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Mixed,
            {5, 1, 5, 3}
        },

        // T6 I wide (double I)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {1, 1, 5, 3}
        },

        // T6 I wide (double V)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {1, 5, 5, 3}
        },

        // T6 V wide (double I)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {5, 1, 1, 3}
        },

        // T6 V wide (double V)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {5, 5, 1, 3}
        },

        // T6 I close (double III)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {1, 5, 3, 3}
        },

        // T6 I wide (double III)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {1, 3, 5, 3}
        },

        // T6 III close (double III)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Close,
            {3, 1, 5, 3}
        },

        // T6 III wide (double III)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Wide,
            {3, 5, 1, 3}
        },

        // T6 V close (double III)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {5, 3, 1, 3}
        },

        // T6 V close (double III)
        {
            HarmonicFunction::T,
            1,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {5, 1, 3, 3}
        },
    });
}

void ChordTemplateLibrary::addS6(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // S6 I close (double I)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {4, 4, 1, 6}
        },

        // S6 I close (double V)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {4, 1, 1, 6}
        },

        // S6 V close (double I)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {1, 4, 4, 6}
        },

        // S6 V close (double V)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {1, 1, 4, 6}
        },

        // S6 I mixed
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Mixed,
            {4, 1, 4, 6}
        },

        // S6 V mixed
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Mixed,
            {1, 4, 1, 6}
        },

        // S6 I wide (double I)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {4, 4, 1, 6}
        },

        // S6 I wide (double V)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {4, 1, 1, 6}
        },

        // S6 V wide (double I)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {1, 4, 4, 6}
        },

        // S6 V wide (double V)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {1, 1, 4, 6}
        },

        // S6 I close (double III)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {4, 1, 6, 6}
        },

        // S6 I wide (double III)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {4, 6, 1, 6}
        },

        // S6 III close (double III)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Close,
            {6, 4, 1, 6}
        },

        // S6 III wide (double III)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Wide,
            {6, 1, 4, 6}
        },

        // S6 V close (double III)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {1, 6, 4, 6}
        },

        // S6 V wide (double III)
        {
            HarmonicFunction::S,
            4,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {1, 4, 6, 6}
        },
    });
}

void ChordTemplateLibrary::addD6(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // D6 I close (double I)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {5, 5, 2, 7}
        },

        // D6 I close (double V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {5, 2, 2, 7}
        },

        // D6 V close (double I)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {2, 5, 5, 7}
        },

        // D6 V close (double V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {2, 2, 5, 7}
        },

        // D6 I mixed
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Mixed,
            {5, 2, 5, 7}
        },

        // D6 V mixed
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Mixed,
            {2, 5, 2, 7}
        },

        // D6 I wide (double I)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {5, 5, 2, 7}
        },

        // D6 I wide (double V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {5, 2, 2, 7}
        },

        // D6 V wide (double I)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {2, 5, 5, 7}
        },

        // D6 V wide (double V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {2, 2, 5, 7}
        },

        // D6 I close (double III)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {5, 2, 7, 7}
        },

        // D6 I wide (double III)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {5, 7, 2, 7}
        },

        // D6 III close (double III)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Close,
            {7, 5, 2, 7}
        },

        // D6 III wide (double III)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Wide,
            {7, 2, 5, 7}
        },

        // D6 V close (double III)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {2, 7, 5, 7}
        },

        // D6 V wide (double III)
        {
            HarmonicFunction::D,
            5,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {2, 5, 4, 7}
        },
    });
}

void ChordTemplateLibrary::addII6(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // II6 III close (double III)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Close,
            {4, 2, 6, 4}
        },

        // II6 III wide (double III)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Wide,
            {4, 6, 2, 4}
        },

        // II6 I close (double III)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {2, 6, 4, 4}
        },

        // II6 I wide (double III)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {2, 4, 6, 4}
        },

        // II6 V close (double III)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {6, 4, 2, 4}
        },

        // II6 V wide (double III)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {6, 2, 4, 4}
        },
        
        // II6 I close (double I)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {2, 2, 6, 4}
        },

        // II6 I close (double V)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {2, 6, 6, 4}
        },

        // II6 V close (double I)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {6, 2, 2, 4}
        },

        // II6 V close (double V)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {6, 6, 2, 4}
        },

        // II6 I mixed
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Mixed,
            {2, 6, 2, 4}
        },

        // II6 V mixed
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Mixed,
            {6, 2, 6, 4}
        },

        // II6 I wide (double I)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {2, 2, 6, 4}
        },

        // II6 I wide (double V)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {2, 6, 6, 4}
        },

        // II6 V wide (double I)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {6, 2, 2, 4}
        },

        // II6 V wide (double V)
        {
            HarmonicFunction::S,
            2,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {6, 6, 2, 4}
        },
    });
}

void ChordTemplateLibrary::addVII6(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // VII6 I close (double III)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {7, 4, 2, 2}
        },

        // VII6 I wide (double III)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {7, 2, 4, 2}
        },

        // VII6 III close (double III)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Close,
            {2, 7, 4, 2}
        },

        // VII6 III wide (double III)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::III,
            ChordPosition::Wide,
            {2, 4, 7, 2}
        },

        // VII6 V close (double III)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {4, 2, 7, 2}
        },

        // VII6 V wide (double III)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {4, 7, 2, 2}
        },
        
        // VII6 I close (double V)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {7, 4, 4, 2}
        },
        
        // VII6 I close (double V)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Close,
            {7, 4, 4, 2}
        },

        // VII6 V close (double V)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Close,
            {4, 4, 7, 2}
        },

        // VII6 V mixed
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Mixed,
            {4, 7, 4, 2}
        },

        // VII6 I wide (double V)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::I,
            ChordPosition::Wide,
            {7, 4, 4, 2}
        },

        // VII6 V wide (double V)
        {
            HarmonicFunction::D,
            7,
            ChordType::Six,
            Inversion::V,
            ChordPosition::Wide,
            {4, 4, 7, 2}
        },
    });
}

void ChordTemplateLibrary::addK64(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // K64 I close
        {
            HarmonicFunction::D,
            5,
            ChordType::CadentialSixFour,
            Inversion::I,
            ChordPosition::Close,
            {1, 5, 3, 5}
        },

        // K64 I wide
        {
            HarmonicFunction::D,
            5,
            ChordType::CadentialSixFour,
            Inversion::I,
            ChordPosition::Wide,
            {1, 3, 5, 5}
        },

        // K64 III close
        {
            HarmonicFunction::D,
            5,
            ChordType::CadentialSixFour,
            Inversion::III,
            ChordPosition::Close,
            {3, 1, 5, 5}
        },

        // K64 III wide
        {
            HarmonicFunction::D,
            5,
            ChordType::CadentialSixFour,
            Inversion::III,
            ChordPosition::Wide,
            {3, 5, 1, 5}
        },

        // K64 V close
        {
            HarmonicFunction::D,
            5,
            ChordType::CadentialSixFour,
            Inversion::V,
            ChordPosition::Close,
            {5, 3, 1, 5}
        },

        // K64 V wide
        {
            HarmonicFunction::D,
            5,
            ChordType::CadentialSixFour,
            Inversion::V,
            ChordPosition::Wide,
            {5, 1, 3, 5}
        },
    });
}

void ChordTemplateLibrary::addT64(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // T64 I close
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::I,
            ChordPosition::Close,
            {1, 5, 3, 5}
        },

        // T64 I wide
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::I,
            ChordPosition::Wide,
            {1, 3, 5, 5}
        },

        // T64 III close
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Close,
            {3, 1, 5, 5}
        },

        // T64 III wide
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Wide,
            {3, 5, 1, 5}
        },

        // T64 V close
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::V,
            ChordPosition::Close,
            {5, 3, 1, 5}
        },

        // T64 V wide
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::V,
            ChordPosition::Wide,
            {5, 1, 3, 5}
        },
    });
}

void ChordTemplateLibrary::addS64(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // S64 I close
        {
            HarmonicFunction::S,
            4,
            ChordType::SixFour,
            Inversion::I,
            ChordPosition::Close,
            {4, 1, 6, 1}
        },

        // S64 I wide
        {
            HarmonicFunction::S,
            4,
            ChordType::SixFour,
            Inversion::I,
            ChordPosition::Wide,
            {4, 6, 1, 1}
        },

        // S64 III close
        {
            HarmonicFunction::S,
            4,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Close,
            {6, 4, 1, 1}
        },

        // S64 III wide
        {
            HarmonicFunction::S,
            4,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Wide,
            {6, 1, 4, 1}
        },

        // S64 V close
        {
            HarmonicFunction::S,
            4,
            ChordType::SixFour,
            Inversion::V,
            ChordPosition::Close,
            {1, 6, 4, 1}
        },

        // S64 V wide
        {
            HarmonicFunction::S,
            4,
            ChordType::SixFour,
            Inversion::V,
            ChordPosition::Wide,
            {1, 4, 6, 1}
        },
    });
}

void ChordTemplateLibrary::addD64(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        // D64 I close
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFour,
            Inversion::I,
            ChordPosition::Close,
            {5, 2, 7, 2}
        },

        // D64 I wide
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFour,
            Inversion::I,
            ChordPosition::Wide,
            {5, 7, 2, 2}
        },

        // D64 III close
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Close,
            {7, 5, 2, 2}
        },

        // D64 III wide
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Wide,
            {7, 2, 5, 2}
        },

        // D64 V close
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFour,
            Inversion::V,
            ChordPosition::Close,
            {2, 7, 5, 2}
        },

        // D64 V wide
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFour,
            Inversion::V,
            ChordPosition::Wide,
            {2, 5, 7, 2}
        },
    });
}

void ChordTemplateLibrary::addD7(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //D7 I close (without V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::I,
            ChordPosition::Close,
            {5, 4, 7, 5}
        },

        //D7 I wide (without V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::I,
            ChordPosition::Wide,
            {5, 7, 4, 5}
        },

        //D7 III close
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::III,
            ChordPosition::Close,
            {7, 4, 2, 5}
        },

         // T53 I close
       {HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::I,
           ChordPosition::Close,
           {1, 3, 1, 1}
       },

        //D7 III wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::III,
            ChordPosition::Wide,
            {7, 2, 4, 5}
        },

         // T53 I close
       {HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::I,
           ChordPosition::Wide,
           {1, 1, 3, 1}
       },

        //D7 III close (without V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::III,
            ChordPosition::Close,
            {7, 5, 4, 5}
        },

        //D7 III wide (without V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::III,
            ChordPosition::Wide,
            {7, 4, 5, 5}
        },

        //D7 V close
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::V,
            ChordPosition::Close,
            {2, 7, 4, 5}
        },

         // T53 I close
       {HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::I,
           ChordPosition::Close,
           {1, 1, 3, 1}
       },

        //D7 V wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::V,
            ChordPosition::Wide,
            {2, 4, 7, 5}
        },

         // T53 I wide
       {HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::I,
           ChordPosition::Wide,
           {1, 3, 1, 1}
       },

        //D7 VII close
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::VII,
            ChordPosition::Close,
            {4, 2, 7, 5}
        },

         // T53 III close
       {HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::III,
           ChordPosition::Close,
           {3, 1, 1, 1}
       },

        //D7 VII wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::VII,
            ChordPosition::Wide,
            {4, 7, 2, 5}
        },

         // T53 III wide
       {HarmonicFunction::T,
           1,
           ChordType::Triad,
           Inversion::III,
           ChordPosition::Wide,
           {3, 1, 1, 1}
       },

        //D7 VII close (without V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::VII,
            ChordPosition::Close,
            {4, 7, 5, 5}
        },

        //D7 VII wide (without V)
        {
            HarmonicFunction::D,
            5,
            ChordType::Seventh,
            Inversion::VII,
            ChordPosition::Wide,
            {4, 5, 7, 5}
        },
    });
}

void ChordTemplateLibrary::addD65(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //D65 I close
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFive,
            Inversion::I,
            ChordPosition::Close,
            {5, 4, 2, 7}
        },

        //D65 I wide
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFive,
            Inversion::I,
            ChordPosition::Wide,
            {5, 2, 4, 7}
        },

        //D65 V close
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFive,
            Inversion::V,
            ChordPosition::Close,
            {2, 5, 4, 7}
        },

        //D65 V wide
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFive,
            Inversion::V,
            ChordPosition::Wide,
            {2, 4, 5, 7}
        },

        //D65 VII close
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFive,
            Inversion::VII,
            ChordPosition::Close,
            {4, 2, 5, 7}
        },

        //D65 VII wide
        {
            HarmonicFunction::D,
            5,
            ChordType::SixFive,
            Inversion::VII,
            ChordPosition::Wide,
            {4, 5, 2, 7}
        },
    });
}

void ChordTemplateLibrary::addD43(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //D43 I close
        {
            HarmonicFunction::D,
            5,
            ChordType::FourThree,
            Inversion::I,
            ChordPosition::Close,
            {5, 4, 7, 2}
        },

        //D43 I wide
        {
            HarmonicFunction::D,
            5,
            ChordType::FourThree,
            Inversion::I,
            ChordPosition::Wide,
            {5, 7, 4, 2}
        },

        //D43 III close
        {
            HarmonicFunction::D,
            5,
            ChordType::FourThree,
            Inversion::III,
            ChordPosition::Close,
            {7, 5, 4, 2}
        },

        //D43 III wide
        {
            HarmonicFunction::D,
            5,
            ChordType::FourThree,
            Inversion::III,
            ChordPosition::Wide,
            {7, 4, 5, 2}
        },

        //D43 VII close
        {
            HarmonicFunction::D,
            5,
            ChordType::FourThree,
            Inversion::VII,
            ChordPosition::Close,
            {4, 7, 5, 2}
        },

        //D43 VII close
        {
            HarmonicFunction::D,
            5,
            ChordType::FourThree,
            Inversion::VII,
            ChordPosition::Close,
            {4, 7, 5, 2}
        },

        //D43 VII wide
        {
            HarmonicFunction::D,
            5,
            ChordType::FourThree,
            Inversion::VII,
            ChordPosition::Wide,
            {4, 5, 7, 2}
        },
    });
}

void ChordTemplateLibrary::addD2(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //D2 I close
        {
            HarmonicFunction::D,
            5,
            ChordType::Two,
            Inversion::I,
            ChordPosition::Close,
            {5, 2, 7, 4}
        },

        //D2 I wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Two,
            Inversion::I,
            ChordPosition::Wide,
            {5, 7, 2, 4}
        },

        //D2 III close
        {
            HarmonicFunction::D,
            5,
            ChordType::Two,
            Inversion::III,
            ChordPosition::Close,
            {7, 5, 2, 4}
        },

        //D2 III wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Two,
            Inversion::III,
            ChordPosition::Wide,
            {7, 2, 5, 4}
        },

        //D2 V close
        {
            HarmonicFunction::D,
            5,
            ChordType::Two,
            Inversion::V,
            ChordPosition::Close,
            {2, 7, 5, 4}
        },

        //D2 V wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Two,
            Inversion::V,
            ChordPosition::Wide,
            {2, 5, 7, 4}
        },
    });
}

void ChordTemplateLibrary::addD9(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //D9 III close
        {
            HarmonicFunction::D,
            5,
            ChordType::Ninth,
            Inversion::III,
            ChordPosition::Close,
            {7, 6, 4, 5}
        },

        //D9 III wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Ninth,
            Inversion::III,
            ChordPosition::Wide,
            {7, 4, 6, 5}
        },

        //D9 VII close
        {
            HarmonicFunction::D,
            5,
            ChordType::Ninth,
            Inversion::VII,
            ChordPosition::Close,
            {4, 7, 6, 5}
        },

        //D9 VII wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Ninth,
            Inversion::VII,
            ChordPosition::Wide,
            {4, 6, 7, 5}
        },

        //D9 IX close
        {
            HarmonicFunction::D,
            5,
            ChordType::Ninth,
            Inversion::IX,
            ChordPosition::Close,
            {6, 4, 7, 5}
        },

        //D9 IX wide
        {
            HarmonicFunction::D,
            5,
            ChordType::Ninth,
            Inversion::IX,
            ChordPosition::Wide,
            {6, 7, 4, 5}
        },
    });
}

void ChordTemplateLibrary::addII7(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //II7 III close
        {
            HarmonicFunction::S,
            2,
            ChordType::Seventh,
            Inversion::III,
            ChordPosition::Close,
            {4, 1, 6, 2}
        },

        //II7 III wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Seventh,
            Inversion::III,
            ChordPosition::Wide,
            {4, 6, 1, 2}
        },

        //II7 V close
        {
            HarmonicFunction::S,
            2,
            ChordType::Seventh,
            Inversion::V,
            ChordPosition::Close,
            {6, 4, 1, 2}
        },

        //II7 V wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Seventh,
            Inversion::V,
            ChordPosition::Wide,
            {6, 1, 4, 2}
        },

        //II7 VII close
        {
            HarmonicFunction::S,
            2,
            ChordType::Seventh,
            Inversion::VII,
            ChordPosition::Close,
            {1, 6, 4, 2}
        },

        //II7 VII wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Seventh,
            Inversion::VII,
            ChordPosition::Wide,
            {1, 4, 6, 2}
        },
    });
}

void ChordTemplateLibrary::addII65(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //II65 I close
        {
            HarmonicFunction::S,
            2,
            ChordType::SixFive,
            Inversion::I,
            ChordPosition::Close,
            {2, 1, 6, 4}
        },

        //II65 I wide
        {
            HarmonicFunction::S,
            2,
            ChordType::SixFive,
            Inversion::I,
            ChordPosition::Wide,
            {2, 6, 1, 4}
        },

        //II65 V close
        {
            HarmonicFunction::S,
            2,
            ChordType::SixFive,
            Inversion::V,
            ChordPosition::Close,
            {6, 2, 1, 4}
        },

        //II65 V wide
        {
            HarmonicFunction::S,
            2,
            ChordType::SixFive,
            Inversion::V,
            ChordPosition::Wide,
            {6, 1, 2, 4}
        },

        //II65 VII close
        {
            HarmonicFunction::S,
            2,
            ChordType::SixFive,
            Inversion::VII,
            ChordPosition::Close,
            {1, 6, 2, 4}
        },

        //II65 VII wide
        {
            HarmonicFunction::S,
            2,
            ChordType::SixFive,
            Inversion::VII,
            ChordPosition::Wide,
            {1, 2, 6, 4}
        },
    });
}

void ChordTemplateLibrary::addII43(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //II43 I close
        {
            HarmonicFunction::S,
            2,
            ChordType::FourThree,
            Inversion::I,
            ChordPosition::Close,
            {2, 1, 4, 6}
        },

        //II43 I wide
        {
            HarmonicFunction::S,
            2,
            ChordType::FourThree,
            Inversion::I,
            ChordPosition::Wide,
            {2, 4, 1, 6}
        },

        //II43 III close
        {
            HarmonicFunction::S,
            2,
            ChordType::FourThree,
            Inversion::III,
            ChordPosition::Close,
            {4, 2, 1, 6}
        },

        //II43 III wide
        {
            HarmonicFunction::S,
            2,
            ChordType::FourThree,
            Inversion::III,
            ChordPosition::Wide,
            {4, 1, 2, 6}
        },

        //II43 VII close
        {
            HarmonicFunction::S,
            2,
            ChordType::FourThree,
            Inversion::VII,
            ChordPosition::Close,
            {1, 4, 2, 6}
        },

        //II43 VII wide
        {
            HarmonicFunction::S,
            2,
            ChordType::FourThree,
            Inversion::VII,
            ChordPosition::Wide,
            {1, 2, 4, 6}
        },
    });
}

void ChordTemplateLibrary::addII2(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //II2 I close
        {
            HarmonicFunction::S,
            2,
            ChordType::Two,
            Inversion::I,
            ChordPosition::Close,
            {2, 6, 4, 1}
        },

        //II2 I wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Two,
            Inversion::I,
            ChordPosition::Wide,
            {2, 4, 6, 1}
        },

        //II2 III close
        {
            HarmonicFunction::S,
            2,
            ChordType::Two,
            Inversion::III,
            ChordPosition::Close,
            {4, 2, 6, 1}
        },

        //II2 III wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Two,
            Inversion::III,
            ChordPosition::Wide,
            {4, 6, 2, 1}
        },

        //II2 V close
        {
            HarmonicFunction::S,
            2,
            ChordType::Two,
            Inversion::V,
            ChordPosition::Close,
            {6, 4, 2, 1}
        },

        //II2 V wide
        {
            HarmonicFunction::S,
            2,
            ChordType::Two,
            Inversion::V,
            ChordPosition::Wide,
            {6, 2, 4, 1}
        },
    });
}

void ChordTemplateLibrary::addVII7(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //VII7 III close
        {
            HarmonicFunction::D,
            7,
            ChordType::Seventh,
            Inversion::III,
            ChordPosition::Close,
            {2, 6, 4, 7}
        },

        //T53 III close
        {
            HarmonicFunction::T,
            1,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Close,
            {3, 5, 3, 1}
        },

        //VII7 III wide
        {
            HarmonicFunction::D,
            7,
            ChordType::Seventh,
            Inversion::III,
            ChordPosition::Wide,
            {2, 4, 6, 7}
        },

        //T53 III wide
        {
            HarmonicFunction::T,
            1,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {3, 3, 5, 1}
        },

        //VII7 V close
        {
            HarmonicFunction::D,
            7,
            ChordType::Seventh,
            Inversion::V,
            ChordPosition::Close,
            {4, 2, 6, 7}
        },

        //T53 III close
        {
            HarmonicFunction::T,
            1,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Close,
            {3, 3, 5, 1}
        },

        //VII7 V wide
        {
            HarmonicFunction::D,
            7,
            ChordType::Seventh,
            Inversion::V,
            ChordPosition::Wide,
            {4, 6, 2, 7}
        },

        //T53 III wide
        {
            HarmonicFunction::T,
            1,
            ChordType::Triad,
            Inversion::III,
            ChordPosition::Wide,
            {3, 5, 3, 1}
        },

        //VII7 VII close
        {
            HarmonicFunction::D,
            7,
            ChordType::Seventh,
            Inversion::VII,
            ChordPosition::Close,
            {6, 4, 2, 7}
        },

        //T53 V close
        {
            HarmonicFunction::T,
            1,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Close,
            {5, 3, 3, 1}
        },

        //VII7 VII wide
        {
            HarmonicFunction::D,
            7,
            ChordType::Seventh,
            Inversion::VII,
            ChordPosition::Wide,
            {6, 2, 4, 7}
        },

        //T53 V wide
        {
            HarmonicFunction::T,
            1,
            ChordType::Triad,
            Inversion::V,
            ChordPosition::Wide,
            {5, 3, 3, 1}
        },
    });
}

void ChordTemplateLibrary::addVII65(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //VII65 I close
        {
            HarmonicFunction::D,
            7,
            ChordType::SixFive,
            Inversion::I,
            ChordPosition::Close,
            {7, 6, 4, 2}
        },

        //VII65 I wide
        {
            HarmonicFunction::D,
            7,
            ChordType::SixFive,
            Inversion::I,
            ChordPosition::Wide,
            {7, 4, 6, 2}
        },

        //VII65 V close
        {
            HarmonicFunction::D,
            7,
            ChordType::SixFive,
            Inversion::V,
            ChordPosition::Close,
            {4, 7, 6, 2}
        },

        //VII65 V wide
        {
            HarmonicFunction::D,
            7,
            ChordType::SixFive,
            Inversion::V,
            ChordPosition::Wide,
            {4, 6, 7, 2}
        },

        //VII65 VII close
        {
            HarmonicFunction::D,
            7,
            ChordType::SixFive,
            Inversion::VII,
            ChordPosition::Close,
            {6, 4, 7, 2}
        },

        //VII65 VII wide
        {
            HarmonicFunction::D,
            7,
            ChordType::SixFive,
            Inversion::VII,
            ChordPosition::Wide,
            {6, 7, 4, 2}
        },
    });
}

void ChordTemplateLibrary::addVII43(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //VII43 I close
        {
            HarmonicFunction::D,
            7,
            ChordType::FourThree,
            Inversion::I,
            ChordPosition::Close,
            {7, 6, 2, 4}
        },

        //VII43 I wide
        {
            HarmonicFunction::D,
            7,
            ChordType::FourThree,
            Inversion::I,
            ChordPosition::Wide,
            {7, 2, 6, 4}
        },

        //VII43 III close
        {
            HarmonicFunction::D,
            7,
            ChordType::FourThree,
            Inversion::III,
            ChordPosition::Close,
            {2, 7, 6, 4}
        },

        //VII43 III wide
        {
            HarmonicFunction::D,
            7,
            ChordType::FourThree,
            Inversion::III,
            ChordPosition::Wide,
            {2, 6, 7, 4}
        },

        //VII43 VII close
        {
            HarmonicFunction::D,
            7,
            ChordType::FourThree,
            Inversion::VII,
            ChordPosition::Close,
            {6, 2, 7, 4}
        },

        //VII43 VII wide
        {
            HarmonicFunction::D,
            7,
            ChordType::FourThree,
            Inversion::VII,
            ChordPosition::Wide,
            {6, 7, 2, 4}
        },
    });
}

void ChordTemplateLibrary::addVII2(std::vector<ChordTemplate>& templates) {
    templates.insert(templates.end(), {
        //VII2 I close
        {
            HarmonicFunction::D,
            7,
            ChordType::Two,
            Inversion::I,
            ChordPosition::Close,
            {7, 4, 2, 6}
        },

        //T64 I close
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::I,
            ChordPosition::Close,
            {1, 3, 3, 5}
        },

        //VII2 I wide
        {
            HarmonicFunction::D,
            7,
            ChordType::Two,
            Inversion::I,
            ChordPosition::Wide,
            {7, 2, 4, 6}
        },

        //T64 I wide
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::I,
            ChordPosition::Wide,
            {1, 3, 3, 5}
        },

        //VII2 III close
        {
            HarmonicFunction::D,
            7,
            ChordType::Two,
            Inversion::III,
            ChordPosition::Close,
            {2, 7, 4, 6}
        },

        //T64 III close
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Close,
            {3, 1, 3, 5}
        },

        //VII2 III wide
        {
            HarmonicFunction::D,
            7,
            ChordType::Two,
            Inversion::III,
            ChordPosition::Wide,
            {2, 4, 7, 6}
        },

        //T64 III wide
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Wide,
            {3, 3, 1, 5}
        },

        //VII2 V close
        {
            HarmonicFunction::D,
            7,
            ChordType::Two,
            Inversion::V,
            ChordPosition::Close,
            {4, 2, 7, 6}
        },

        //T64 III close
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Close,
            {3, 3, 1, 5}
        },

        //VII2 V wide
        {
            HarmonicFunction::D,
            7,
            ChordType::Two,
            Inversion::V,
            ChordPosition::Wide,
            {4, 7, 2, 6}
        },

        //T64 III wide
        {
            HarmonicFunction::T,
            1,
            ChordType::SixFour,
            Inversion::III,
            ChordPosition::Wide,
            {3, 1, 3, 5}
        },
    });
}
