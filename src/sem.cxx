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
            case UNFOUND_ADJ:
            case UNFOUND_VERB:
                return "niet-gevonden";
                break;
            case UNDEFINED_NOUN:
            case UNDEFINED_ADJ:
            case UNDEFINED_VERB:
                return "ongedefinieerd";
                break;
            /** Noun semtypes start here */
            case CONCRETE_DYNAMIC_NOUN:
                return "concreet-gebeuren";
                break;
            case ABSTRACT_DYNAMIC_NOUN:
                return "abstract-gebeuren";
                break;
            case ABSTRACT_NONDYNAMIC_NOUN:
                return "abstract-overig";
                break;
            case BROAD_CONCRETE_PLACE_NOUN:
                return "plaats";
                break;
            case BROAD_CONCRETE_TIME_NOUN:
                return "tijd";
                break;
            case BROAD_CONCRETE_MEASURE_NOUN:
                return "maat";
                break;
            case CONCRETE_FOOD_CARE_NOUN:
                return "voeding-verzorging";
                break;
            case CONCRETE_OTHER_NOUN:
                return "concreet-overig";
                break;
            case INSTITUT_NOUN:
                return "organisatie";
                break;
            case CONCRETE_SUBSTANCE_NOUN:
                return "concrete-substantie";
                break;
            case ABSTRACT_SUBSTANCE_NOUN:
                return "abstracte-substantie";
                break;
            case CONCRETE_ARTEFACT_NOUN:
                return "gebruiksvoorwerp";
                break;
            case CONCRETE_HUMAN_NOUN:
                return "persoon";
                break;
            case CONCRETE_NONHUMAN_NOUN:
                return "plant-dier";
                break;
            /** Adjective semtypes start here */
            case ABSTRACT_ADJ:
                return "abstract";
                break;
            case HUMAN_ADJ:
                return "mens-waarneembaar";
                break;
            case EMO_ADJ:
                return "emoties-sociaal";
                break;
            case NONHUMAN_SHAPE_ADJ:
                return "ding-vorm-omvang";
                break;
            case NONHUMAN_COLOR_ADJ:
                return "ding-kleur";
                break;
            case NONHUMAN_MATTER_ADJ:
                return "ding-stof";
                break;
            case NONHUMAN_SOUND_ADJ:
                return "ding-geluid";
                break;
            case NONHUMAN_OTHER_ADJ:
                return "ding-overig";
                break;
            case TECH_ADJ:
                return "niet-waarneembaar";
                break;
            case TIME_ADJ:
                return "tijd";
                break;
            case PLACE_ADJ:
                return "plaats";
                break;
            case SPEC_POS_ADJ:
                return "spec-positief";
                break;
            case SPEC_NEG_ADJ:
                return "spec-negatief";
                break;
            case POS_ADJ:
                return "alg-positief";
                break;
            case NEG_ADJ:
                return "alg-negatief";
                break;
            case EVALUATIVE_ADJ:
                return "alg-evaluatief";
                break;
            case EPI_POS_ADJ:
                return "epist-pos";
                break;
            case EPI_NEG_ADJ:
                return "epist-neg";
                break;
            /** Verb semtypes start here */
            case ABSTRACT_UNDEFINED:
                return "abstract-ongedefinieerd";
                break;
            case CONCRETE_UNDEFINED:
                return "concreet-ongedefinieerd";
                break;
            case ABSTRACT_STATE:
                return "abstract-toestand";
                break;
            case CONCRETE_STATE:
                return "concreet-toestand";
                break;
            case UNDEFINED_STATE:
                return "ongedefinieerd-toestand";
                break;
            case ABSTRACT_ACTION:
                return "abstract-actie";
                break;
            case CONCRETE_ACTION:
                return "concreet-actie";
                break;
            case UNDEFINED_ACTION:
                return "ongedefinieerd-actie";
                break;
            case ABSTRACT_PROCESS:
                return "abstract-proces";
                break;
            case CONCRETE_PROCESS:
                return "concreet-proces";
                break;
            case UNDEFINED_PROCESS:
                return "ongedefinieerd-proces";
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
        else if (s == "substance_conc")
            return CONCRETE_SUBSTANCE_NOUN;
        else if (s == "artefact")
            return CONCRETE_ARTEFACT_NOUN;
        else if (s == "nonhuman")
            return CONCRETE_NONHUMAN_NOUN;
        else if (s == "human")
            return CONCRETE_HUMAN_NOUN;
        else if (s == "voed_verz") // 20150508: Added new SEM::Type
            return CONCRETE_FOOD_CARE_NOUN;
        else if (s == "dynamic_conc") // 20141031: Added new SEM::Type
            return CONCRETE_DYNAMIC_NOUN;
        else if (s == "substance_abstr") // 20150508: Added new SEM::Type
            return ABSTRACT_SUBSTANCE_NOUN;
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
        } else if (s == "alg_evaluatief") {
            result = EVALUATIVE_ADJ;
        } else if (s == "ep_positief") {
            result = EPI_POS_ADJ;
        } else if (s == "ep_negatief") {
            result = EPI_NEG_ADJ;
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
            case CONCRETE_FOOD_CARE_NOUN:
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
            case CONCRETE_FOOD_CARE_NOUN:
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