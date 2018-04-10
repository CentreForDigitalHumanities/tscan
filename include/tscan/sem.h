#ifndef SEM_H
#define SEM_H

#include <string>
#include <iostream>

namespace SEM {

    enum Type {
        NO_SEMTYPE,
        UNFOUND_NOUN, UNFOUND_ADJ, UNFOUND_VERB,
        UNDEFINED_NOUN, UNDEFINED_ADJ,
        CONCRETE_DYNAMIC_NOUN, ABSTRACT_DYNAMIC_NOUN,
        ABSTRACT_SUBSTANCE_NOUN, ABSTRACT_NONDYNAMIC_NOUN,
        BROAD_CONCRETE_PLACE_NOUN,
        BROAD_CONCRETE_TIME_NOUN,
        BROAD_CONCRETE_MEASURE_NOUN,
        CONCRETE_FOOD_CARE_NOUN,
        CONCRETE_OTHER_NOUN, INSTITUT_NOUN,
        CONCRETE_SUBSTANCE_NOUN, CONCRETE_ARTEFACT_NOUN,
        CONCRETE_HUMAN_NOUN, CONCRETE_NONHUMAN_NOUN,
        HUMAN_ADJ, EMO_ADJ,
        NONHUMAN_SHAPE_ADJ, NONHUMAN_COLOR_ADJ, NONHUMAN_MATTER_ADJ,
        NONHUMAN_SOUND_ADJ, NONHUMAN_OTHER_ADJ,
        TECH_ADJ, TIME_ADJ, PLACE_ADJ,
        SPEC_POS_ADJ, SPEC_NEG_ADJ,
        POS_ADJ, NEG_ADJ, EVALUATIVE_ADJ,
        EPI_POS_ADJ, EPI_NEG_ADJ,
        ABSTRACT_ADJ,
        ABSTRACT_UNDEFINED, CONCRETE_UNDEFINED, UNDEFINED_VERB,
        ABSTRACT_STATE, CONCRETE_STATE, UNDEFINED_STATE,
        ABSTRACT_ACTION, CONCRETE_ACTION, UNDEFINED_ACTION,
        ABSTRACT_PROCESS, CONCRETE_PROCESS, UNDEFINED_PROCESS
    };

    std::string toString(Type);
    Type classifyNoun( const std::string& );
    Type classifyWW( const std::string&, const std::string& = "");
    Type classifyADJ( const std::string&, const std::string& = "");
    bool isStrictNoun(Type);
    bool isBroadNoun(Type);
    bool isStrictAdj(Type);
    bool isBroadAdj(Type);
    std::ostream& operator<<(std::ostream&, Type);
}

#endif // SEM_H
