#include "tscan/situation.h"

using namespace std;

namespace Situation {
  string toString(Type s) {
    switch (s) {
    case NO_SIT:
      return "Geen_situatie";
      break;
    case TIME_SIT:
      return "tijd";
      break;
    case SPACE_SIT:
      return "ruimte";
      break;
    case CAUSAL_SIT:
      return "causaliteit";
      break;
    case EMO_SIT:
      return "emotie";
      break;
    default:
      throw "no translation for SituationType";
    }
  }

  ostream& operator<<(ostream& os, const Type& s) {
    os << toString(s);
    return os;
  }
}
