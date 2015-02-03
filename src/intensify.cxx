#include <string>
#include <iostream>
#include "tscan/intensify.h"

using namespace std;

namespace Intensify {
    string toString(Type st) {
        switch (st) {
            case BVBW:
                return "bvbw";
                break;
            case BVNW:
                return "bvnw";
                break;
            case BW:
                return "bw";
                break;
            case COMBI:
                return "combi";
                break;
            case NW:
                return "nw";
                break;
            case TUSS:
                return "tuss";
                break;
            case WW:
                return "ww";
                break;
            case NO_INTENSIFY:
                return "does not intensify";
                break;
            default:
                return "invalid intensify value";
        }
    }
    
    Intensify::Type classify(const string& s) {
        if (s == "bvbw")
            return BVBW;
        else if (s == "bvnw")
            return BVNW;
        else if (s == "bw")
            return BW;
        else if (s == "combi")
            return COMBI;
        else if (s == "nw")
            return NW;
        else if (s == "tuss")
            return TUSS;
        else if (s == "ww")
            return WW;
        else
            return NO_INTENSIFY;
    }
}