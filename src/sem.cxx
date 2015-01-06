#include <string>
#include <iostream>
#include "tscan/sem.h"

using namespace std;

namespace SEM {

    string toString(Type st) {
        switch (st) {
            case NO_SEMTYPE:
                return "semtype_error";
                break;
            case UNFOUND_NOUN:
                return "noun_not_found";
                break;
            case UNFOUND_ADJ:
                return "adj_not_found";
                break;
            case UNFOUND_VERB:
                return "verb_not_found";
                break;
            case UNDEFINED_NOUN:
                return "undefined-noun";
                break;
            case UNDEFINED_ADJ:
                return "undefined-adj";
                break;
            case CONCRETE_DYNAMIC_NOUN:
                return "concrete-dynamic_noun";
                break;
            case ABSTRACT_DYNAMIC_NOUN:
                return "abstract-dynamic_noun";
                break;
            case ABSTRACT_NONDYNAMIC_NOUN:
                return "abstract-nondynamic-noun";
                break;
            case BROAD_CONCRETE_PLACE_NOUN:
                return "broad-place-noun";
                break;
            case BROAD_CONCRETE_TIME_NOUN:
                return "broad-time-noun";
                break;
            case BROAD_CONCRETE_MEASURE_NOUN:
                return "broad-measure-noun";
                break;
            case CONCRETE_OTHER_NOUN:
                return "concrete-other-noun";
                break;
            case INSTITUT_NOUN:
                return "institut-noun";
                break;
            case CONCRETE_SUBSTANCE_NOUN:
                return "concrete-substance-noun";
                break;
            case CONCRETE_ARTEFACT_NOUN:
                return "concrete-artefact-noun";
                break;
            case CONCRETE_HUMAN_NOUN:
                return "concrete-human-noun";
                break;
            case CONCRETE_NONHUMAN_NOUN:
                return "concrete-nonhuman-noun";
                break;
            case ABSTRACT_ADJ:
                return "abstract-adj";
                break;
            case HUMAN_ADJ:
                return "human-adj";
                break;
            case EMO_ADJ:
                return "emo-adj";
                break;
            case NONHUMAN_SHAPE_ADJ:
                return "shape-adj";
                break;
            case NONHUMAN_COLOR_ADJ:
                return "color-adj";
                break;
            case NONHUMAN_MATTER_ADJ:
                return "matter-adj";
                break;
            case NONHUMAN_SOUND_ADJ:
                return "sound-adj";
                break;
            case NONHUMAN_OTHER_ADJ:
                return "nonhuman-other-adj";
                break;
            case TECH_ADJ:
                return "tech-adj";
                break;
            case TIME_ADJ:
                return "time-adj";
                break;
            case PLACE_ADJ:
                return "place-adj";
                break;
            case SPEC_POS_ADJ:
                return "spec-pos-adj";
                break;
            case SPEC_NEG_ADJ:
                return "spec-neg-adj";
                break;
            case POS_ADJ:
                return "pos-adj";
                break;
            case NEG_ADJ:
                return "neg-adj";
                break;
            case EPI_POS_ADJ:
                return "epi-pos-adj";
                break;
            case EPI_NEG_ADJ:
                return "epi-neg-adj";
                break;
            case MORE_ADJ:
                return "stronger-adj";
                break;
            case LESS_ADJ:
                return "weaker-adj";
                break;
            case ABSTRACT_UNDEFINED:
                return "abstract-undefined";
                break;
            case CONCRETE_UNDEFINED:
                return "concrete-undefined";
                break;
            case UNDEFINED_VERB:
                return "undefined-verb";
                break;
            case ABSTRACT_STATE:
                return "abstract-state";
                break;
            case CONCRETE_STATE:
                return "concrete-state";
                break;
            case UNDEFINED_STATE:
                return "undefined-state";
                break;
            case ABSTRACT_ACTION:
                return "abstract-action";
                break;
            case CONCRETE_ACTION:
                return "concrete-action";
                break;
            case UNDEFINED_ACTION:
                return "undefined-action";
                break;
            case ABSTRACT_PROCESS:
                return "abstract-process";
                break;
            case CONCRETE_PROCESS:
                return "concrete-process";
                break;
            case UNDEFINED_PROCESS:
                return "undefined-process";
                break;
            default:
                return "invalid semtype value";
        }
    }

    SEM::Type classifyNoun(const string& s) {
        if (s == "undefined")
            return UNDEFINED_NOUN;
        else if (s == "concrother")
            return CONCRETE_OTHER_NOUN;
        else if (s == "institut")
            return INSTITUT_NOUN;
        else if (s == "substance")
            return CONCRETE_SUBSTANCE_NOUN;
        else if (s == "artefact")
            return CONCRETE_ARTEFACT_NOUN;
        else if (s == "nonhuman")
            return CONCRETE_NONHUMAN_NOUN;
        else if (s == "human")
            return CONCRETE_HUMAN_NOUN;
        else if (s == "dynamic_conc") // 20141031: Added new SEM::Type
            return CONCRETE_DYNAMIC_NOUN;
        else if (s == "dynamic_abstr")
            return ABSTRACT_DYNAMIC_NOUN;
        else if (s == "nondynamic")
            return ABSTRACT_NONDYNAMIC_NOUN;
        else if (s == "place")
            return BROAD_CONCRETE_PLACE_NOUN;
        else if (s == "time")
            return BROAD_CONCRETE_TIME_NOUN;
        else if (s == "measure")
            return BROAD_CONCRETE_MEASURE_NOUN;
        else
            return UNFOUND_NOUN;
    }

    SEM::Type classifyWW(const string& s, const string& c) {
        if (s == "undefined") {
            if (c == "abstract")
                return ABSTRACT_UNDEFINED;
            else if (c == "concreet")
                return CONCRETE_UNDEFINED;
            else if (c == "undefined")
                return UNDEFINED_VERB;
        } else if (s == "state") {
            if (c == "abstract")
                return ABSTRACT_STATE;
            else if (c == "concreet")
                return CONCRETE_STATE;
            else if (c == "undefined")
                return UNDEFINED_STATE;
        } else if (s == "action") {
            if (c == "abstract")
                return ABSTRACT_ACTION;
            else if (c == "concreet")
                return CONCRETE_ACTION;
            else if (c == "undefined")
                return UNDEFINED_ACTION;
        } else if (s == "process") {
            if (c == "abstract")
                return ABSTRACT_PROCESS;
            else if (c == "concreet")
                return CONCRETE_PROCESS;
            else if (c == "undefined")
                return UNDEFINED_PROCESS;
        }
        return UNFOUND_VERB;
    }

    SEM::Type classifyADJ(const string& s, const string& sub) {
        SEM::Type result = UNFOUND_ADJ;
        if (s == "undefined") {
            result = UNDEFINED_ADJ;
        } else if (s == "waarn_mens") {
            result = HUMAN_ADJ;
        } else if (s == "emosoc") {
            result = EMO_ADJ;
        } else if (s == "waarn_niet_mens") {
            if (sub == "vorm_omvang") {
                result = NONHUMAN_SHAPE_ADJ;
            } else if (sub == "kleur") {
                result = NONHUMAN_COLOR_ADJ;
            } else if (sub == "stof") {
                result = NONHUMAN_MATTER_ADJ;
            } else if (sub == "geluid") {
                result = NONHUMAN_SOUND_ADJ;
            } else if (sub == "waarn_niet_mens_ov") {
                result = NONHUMAN_OTHER_ADJ;
            }
        } else if (s == "technisch") {
            result = TECH_ADJ;
        } else if (s == "time") {
            result = TIME_ADJ;
        } else if (s == "place") {
            result = PLACE_ADJ;
        } else if (s == "spec_positief") {
            result = SPEC_POS_ADJ;
        } else if (s == "spec_negatief") {
            result = SPEC_NEG_ADJ;
        } else if (s == "alg_positief") {
            result = POS_ADJ;
        } else if (s == "alg_negatief") {
            result = NEG_ADJ;
        } else if (s == "ep_positief") {
            result = EPI_POS_ADJ;
        } else if (s == "ep_negatief") {
            result = EPI_NEG_ADJ;
        } else if (s == "versterker") {
            result = MORE_ADJ;
        } else if (s == "verzwakker") {
            result = LESS_ADJ;
        } else if (s == "abstract_ov") {
            result = ABSTRACT_ADJ;
        }
        if (result == UNFOUND_ADJ) {
            cerr << "classify ADJ " << s << "," << sub << endl;
        }
        return result;
    }

    bool isStrictNoun(const Type st) {
        switch (st) {
            case CONCRETE_HUMAN_NOUN:
            case CONCRETE_NONHUMAN_NOUN:
            case CONCRETE_ARTEFACT_NOUN:
            case CONCRETE_SUBSTANCE_NOUN:
            case CONCRETE_OTHER_NOUN:
            case CONCRETE_DYNAMIC_NOUN:
                return true;
                break;
            default:
                return false;
        }
    }

    bool isBroadNoun(const Type st) {
        switch (st) {
            case CONCRETE_HUMAN_NOUN:
            case CONCRETE_NONHUMAN_NOUN:
            case CONCRETE_ARTEFACT_NOUN:
            case CONCRETE_SUBSTANCE_NOUN:
            case CONCRETE_OTHER_NOUN:
            case CONCRETE_DYNAMIC_NOUN:
            case BROAD_CONCRETE_PLACE_NOUN:
            case BROAD_CONCRETE_TIME_NOUN:
            case BROAD_CONCRETE_MEASURE_NOUN:
                return true;
                break;
            default:
                return false;
        }
    }

    bool isStrictAdj(const Type st) {
        switch (st) {
            case HUMAN_ADJ:
            case EMO_ADJ:
            case NONHUMAN_SHAPE_ADJ:
            case NONHUMAN_COLOR_ADJ:
            case NONHUMAN_MATTER_ADJ:
            case NONHUMAN_SOUND_ADJ:
            case NONHUMAN_OTHER_ADJ:
                return true;
                break;
            default:
                return false;
        }
    }

    bool isBroadAdj(const Type st) {
        switch (st) {
            case HUMAN_ADJ:
            case EMO_ADJ:
            case NONHUMAN_SHAPE_ADJ:
            case NONHUMAN_COLOR_ADJ:
            case NONHUMAN_MATTER_ADJ:
            case NONHUMAN_SOUND_ADJ:
            case NONHUMAN_OTHER_ADJ:
            case TIME_ADJ:
            case PLACE_ADJ:
                return true;
                break;
            default:
                return false;
        }
    }

}