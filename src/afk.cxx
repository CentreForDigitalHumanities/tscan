#include <string>
#include <iostream>
#include "tscan/afk.h"

using namespace std;

namespace Afk {
  string toString(Type afk) {
    switch (afk) {
      case NO_A:
        return "none";
      case OVERHEID_A:
        return "Overheid_Politiek";
      case ZORG_A:
        return "Zorg";
      case INTERNATIONAAL_A:
        return "Internationaal";
      case MEDIA_A:
        return "Media";
      case ONDERWIJS_A:
        return "Onderwijs";
      case JURIDISCH_A:
        return "Juridisch";
      case OVERIGE_A:
        return "Overig";
      case GENERIEK_A:
        return "Generiek";
      default:
        return "UnknownAfkType";
    };
  }

  Type classify(const string& s) {
    if ( s == "none" )
      return NO_A;
    if ( s == "Overheid_Politiek" )
      return OVERHEID_A;
    if ( s == "Zorg" )
      return ZORG_A;
    if ( s == "Onderwijs" )
      return ONDERWIJS_A;
    if ( s == "Juridisch" )
      return JURIDISCH_A;
    if ( s == "Overig" )
      return OVERIGE_A;
    if ( s == "Internationaal" )
      return INTERNATIONAAL_A;
    if ( s == "Media" )
      return MEDIA_A;
    if (s == "Generiek" )
      return GENERIEK_A;
    return NO_A;
  }

  ostream& operator<<(ostream& os, const Type& s) {
    os << toString(s);
    return os;
  }
}
