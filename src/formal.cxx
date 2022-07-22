#include "tscan/formal.h"

using namespace std;

namespace Formal {
    string toString(Type st) {
        switch (st) {
            case BVNW:
                return "adjectief";
                break;
            case BW:
                return "bijwoord";
                break;
            case VGW:
                return "voegwoord";
                break;
            case VNW:
                return "voornaamwoord";
                break;
            case VZ:
                return "voorzetsel";
                break;
            case VZG:
                return "voorzetselgroep";
                break;
            case WW:
                return "werkwoord";
                break;
            case ZNW:
                return "zelfstandig naamwoord";
                break;
            case NOT_FORMAL:
                return "NA";
                break;
            default:
                return "invalid formal value";
        }
    }
    
    Formal::Type classify(const string& s) {
        if (s == "adjectief")
            return BVNW;
        else if (s == "bijwoord")
            return BW;
        else if (s == "voegwoord")
            return VGW;
        else if (s == "voornaamwoord")
            return VNW;
        else if (s == "voorzetsel")
            return VZ;
        else if (s == "voorzetselgroep")
            return VZG;
        else if (s == "werkwoord")
            return WW;
        else if (s == "zelfstandig naamwoord")
            return ZNW;
        else
            return INVALID;
    }
}
