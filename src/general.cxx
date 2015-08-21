#include <string>
#include <iostream>
#include "tscan/general.h"

using namespace std;

namespace General {
    string toString(Type t) {
        switch (t) {
            case ADDITION_ALTERNATIVE:
                return "additie-alternatief";
                break;
            case IMPORTANCE_INTEREST:
                return "belang-interesse";
                break;
            case DESCRIPTION:
                return "beschrijving";
                break;
            case EXISTENCE:
                return "bestaan";
                break;
            case PHRASING:
                return "bewoording";
                break;
            case CONCEPT_SYSTEM:
                return "concept(systeem)";
                break;
            case CONTRAST_VARIATION:
                return "contrast-variatie";
                break;
            case DISCUSSION:
                return "discussie";
                break;
            case REACHING_GOALS:
                return "doelen-bereiken";
                break;
            case FACTUAL_CORRECTNESS:
                return "feitelijke-juistheid";
                break;
            case EVENT:
                return "gebeurtenis";
                break;
            case THOUGHT_POSITION:
                return "gedachte-standpunt";
                break;
            case GRADATION:
                return "gradatie";
                break;
            case ACTS_CHOICES:
                return "handelingen-keuzes";
                break;
            case INFORMATION:
                return "informatie";
                break;
            case INTERPRETATION:
                return "interpretatie";
                break;
            case KNOWLEDGE:
                return "kennisverwerving";
                break;
            case MEANS_GOAL:
                return "middel-doel";
                break;
            case OPPORTUNITY:
                return "mogelijkheid";
                break;
            case DEVELOPMENT_STABILITY:
                return "ontwikkeling-stabiliteit";
                break;
            case PROBLEM_SOLUTION:
                return "probleem-oplossing";
                break;
            case REASONING_CAUSALITY:
                return "redeneren-causaliteit";
                break;
            case STATE:
                return "toestand";
                break;
            case STRUCTURE:
                return "structuur";
                break;
            case DESIRABILITY:
                return "wenselijkheid";
                break;
            case NO_GENERAL:
                return "niet-algemeen";
                break;
            default:
                return "invalid general value";
        }
    }
    
    Type classify(const string& s) {
        if (s == "additie-alternatief")
            return ADDITION_ALTERNATIVE;
        else if (s == "belang-interesse")
            return IMPORTANCE_INTEREST;
        else if (s == "beschrijving")
            return DESCRIPTION;
        else if (s == "bestaan")
            return EXISTENCE;
        else if (s == "bewoording")
            return PHRASING;
        else if (s == "concept(systeem)")
            return CONCEPT_SYSTEM;
        else if (s == "contrast-variatie")
            return CONTRAST_VARIATION;
        else if (s == "discussie")
            return DISCUSSION;
        else if (s == "doelen-bereiken")
            return REACHING_GOALS;
        else if (s == "feitelijke-juistheid")
            return FACTUAL_CORRECTNESS;
        else if (s == "gebeurtenis")
            return EVENT;
        else if (s == "gedachte-standpunt")
            return THOUGHT_POSITION;
        else if (s == "gradatie")
            return GRADATION;
        else if (s == "handelingen-keuzes")
            return ACTS_CHOICES;
        else if (s == "informatie")
            return INFORMATION;
        else if (s == "interpretatie")
            return INTERPRETATION;
        else if (s == "kennisverwerving")
            return KNOWLEDGE;
        else if (s == "middel-doel")
            return MEANS_GOAL;
        else if (s == "mogelijkheid")
            return OPPORTUNITY;
        else if (s == "ontwikkeling-stabiliteit")
            return DEVELOPMENT_STABILITY;
        else if (s == "probleem-oplossing")
            return PROBLEM_SOLUTION;
        else if (s == "redeneren-causaliteit")
            return REASONING_CAUSALITY;
        else if (s == "toestand")
            return STATE;
        else if (s == "structuur")
            return STRUCTURE;
        else if (s == "wenselijkheid")
            return DESIRABILITY;
        else
            return NO_GENERAL;
    }

    // Returns true for general nouns about separate situations
    bool isSeparate(const Type t) {
        switch (t) {
            case IMPORTANCE_INTEREST:
            case CONCEPT_SYSTEM:
            case FACTUAL_CORRECTNESS:
            case THOUGHT_POSITION:
            case INFORMATION:
            case INTERPRETATION:
            case KNOWLEDGE:
            case OPPORTUNITY:
            case DESIRABILITY:
            case PHRASING:
            case DESCRIPTION:
            case EXISTENCE:
            case EVENT:
            case STATE:
            case GRADATION:
                return true;
                break;
            default:
                return false;
        }
    }

    // Returns true for general nouns about related situations
    bool isRelated(const Type t) {
        switch (t) {
            case ADDITION_ALTERNATIVE:
            case CONTRAST_VARIATION:
            case DISCUSSION:
            case REACHING_GOALS:
            case ACTS_CHOICES:
            case MEANS_GOAL:
            case DEVELOPMENT_STABILITY:
            case PROBLEM_SOLUTION:
            case REASONING_CAUSALITY:
            case STRUCTURE:
                return true;
                break;
            default:
                return false;
        }
    }

    // Returns true for general nouns about human acting
    bool isActing(const Type t) {
        switch (t) {
            case REACHING_GOALS:
            case ACTS_CHOICES:
            case MEANS_GOAL:
            case PROBLEM_SOLUTION:
                return true;
                break;
            default:
                return false;
        }
    }

    // Returns true for general nouns about knowledge
    bool isKnowledge(const Type t) {
        switch (t) {
            case CONCEPT_SYSTEM:
            case FACTUAL_CORRECTNESS:
            case THOUGHT_POSITION:
            case INFORMATION:
            case INTERPRETATION:
            case KNOWLEDGE:
            case DISCUSSION:
            case REASONING_CAUSALITY:
                return true;
                break;
            default:
                return false;
        }
    }

    // Returns true for general nouns about discussion and reasoning
    bool isDiscussion(const Type t) {
        switch (t) {
            case DISCUSSION:
            case REASONING_CAUSALITY:
                return true;
                break;
            default:
                return false;
        }
    }

    // Returns true for general nouns about development
    bool isDevelopment(const Type t) {
        switch (t) {
            case DEVELOPMENT_STABILITY:
                return true;
                break;
            default:
                return false;
        }
    }

    ostream& operator<<( ostream& os, const Type& t ){
      os << toString( t );
      return os;
    }
}
