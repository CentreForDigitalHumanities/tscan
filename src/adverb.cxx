#include <string>
#include <iostream>
#include "tscan/adverb.h"

using namespace std;

namespace Adverb {
  string toString(Type t) {
    switch (t) {
      case GENERAL:
        return "algemeen";
      case SPECIFIC:
        return "specifiek";
      default:
        return "onbekend bijwoord";
    };
  }

  Type classify(const string& s) {
    if ( s == "algemeen" )
      return GENERAL;
    if ( s == "specifiek" )
      return SPECIFIC;
    return NO_ADVERB;
  }

  ostream& operator<<(ostream& os, const Type& s) {
    os << toString(s);
    return os;
  }
}
