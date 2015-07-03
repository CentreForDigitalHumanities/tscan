#include <iostream>
#include "tscan/cgn.h"

using namespace std;

namespace CGN {

    Type toCGN(string s) {
        if (s == "N")
            return N;
        else if (s == "ADJ")
            return ADJ;
        else if (s == "VNW")
            return VNW;
        else if (s == "LET")
            return LET;
        else if (s == "LID")
            return LID;
        else if (s == "SPEC")
            return SPEC;
        else if (s == "BW")
            return BW;
        else if (s == "WW")
            return WW;
        else if (s == "TSW")
            return TSW;
        else if (s == "TW")
            return TW;
        else if (s == "VZ")
            return VZ;
        else if (s == "VG")
            return VG;
        else
            return UNASS;
    }

    string toString(Type t) {
        switch (t) {
            case N:
                return "N";
            case ADJ:
                return "ADJ";
            case VNW:
                return "VNW";
            case LET:
                return "LET";
            case LID:
                return "LID";
            case SPEC:
                return "SPEC";
            case BW:
                return "BW";
            case WW:
                return "WW";
            case TW:
                return "TW";
            case VZ:
                return "VZ";
            case VG:
                return "VG";
            default:
                return "UNASSIGNED";
        }
    }

    string toString(Prop w) {
        switch (w) {
            case ISNAME:
                return "naam";
            case ISLET:
                return "punctuatie";
            case ISVD:
                return "voltooid_deelw";
            case ISOD:
                return "onvoltooid_deelw";
            case ISINF:
                return "infinitief";
            case ISPVTGW:
                return "tegenwoordige_tijd";
            case ISPVVERL:
                return "verleden_tijd";
            case ISSUBJ:
                return "subjonctive";
            case ISPPRON1:
                return "voornaamwoord_1";
            case ISPPRON2:
                return "voornaamwoord_2";
            case ISPPRON3:
                return "voornaamwoord_3";
            case ISAANW:
                return "aanwijzend";
            case NOTAWORD:
                return "GEEN_WOORD";
            default:
                return "default";
        }
    }

    string toString(Position w) {
        switch (w) {
            case NOMIN:
                return "nom";
            case PRENOM:
                return "prenom";
            case VRIJ:
                return "vrij";
            default:
                return "onbekende positie";
        }
    }

    ostream& operator<<( ostream& os, const Type& t ){
        os << toString( t );
        return os;
    }

    ostream& operator<<( ostream& os, const Prop& p ){
        os << toString( p );
        return os;
    }

    ostream& operator<<( ostream& os, const Position& p ){
        os << toString( p );
        return os;
    }
}
