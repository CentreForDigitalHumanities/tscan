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
}
