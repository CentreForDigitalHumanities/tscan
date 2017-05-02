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
        return "niet-gevonden";
    };
  }

  Type classifyType(const string& s) {
    if ( s == "algemeen" )
      return GENERAL;
    if ( s == "specifiek" )
      return SPECIFIC;
    return NO_ADVERB;
  }

  SubType classifySubType(const string& s) {
    if ( s == "ambigu" )
      return AMBIGUOUS;
    if ( s == "anaforisch" )
      return ANAPHORIC;
    if ( s == "graad" )
      return GRADE;
    if ( s == "kwantiteit" )
      return QUANTITY;
    if ( s == "modaal" )
      return MODAL;
    if ( s == "modaal partikel" )
      return MODAL_PARTICLE;
    if ( s == "negatie" )
      return NEGATION;
    if ( s == "relatiemarkering" )
      return RELATION;
    if ( s == "ruimte" )
      return SPACE;
    if ( s == "ruimte-tijd" )
      return SPACE_TIME;
    if ( s == "tijd" )
      return TIME;
    if ( s == "tussenwerpsel" )
      return INTERJECTION;
    if ( s == "wijze" )
      return MANNER;
    if ( s == "overig algemeen" )
      return OTHER;
    return NO_ADVERB_SUBTYPE;
  }

  bool isContent(SubType st) {
    return st == MANNER;
  }

  ostream& operator<<(ostream& os, const Type& s) {
    os << toString(s);
    return os;
  }
}
