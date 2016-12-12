#include "tscan/ner.h"

using namespace std;

namespace NER {
  const string frog_ner_set = "http://ilk.uvt.nl/folia/sets/frog-ner-nl";

  Type lookupNer(const folia::Word *w, const folia::Sentence *s) {
    Type result = NONER;
    vector<folia::Entity*> v = s->select<folia::Entity>(frog_ner_set);
    for ( size_t i=0; i < v.size(); ++i ) {
      folia::FoliaElement *e = v[i];
      for ( size_t j=0; j < e->size(); ++j ) {
        if ( e->index(j) == w ){
          string cls = v[i]->cls();
          if ( cls == "org" ) {
            result = j == 0 ? ORG_B : ORG_I;
          }
          else if ( cls == "eve" ) {
            result = j == 0 ? EVE_B : EVE_I;
          }
          else if ( cls == "loc" ) {
            result = j == 0 ? LOC_B : LOC_I;
          }
          else if ( cls == "misc" ) {
            result = j == 0 ? MISC_B : MISC_I;
          }
          else if ( cls == "per" ) {
            result = j == 0 ? PER_B : PER_I;
          }
          else if ( cls == "pro" ) {
            result = j == 0 ? PRO_B : PRO_I;
          }
          else {
            throw folia::ValueError( "unknown NER class: " + cls );
          }
        }
      }
    }
    return result;
  }

  string toString(Type n) {
    switch ( n ) {
      case LOC_B:
      case LOC_I:
        return "LOC";
        break;
      case EVE_B:
      case EVE_I:
        return "EVE";
        break;
      case ORG_B:
      case ORG_I:
        return "ORG";
        break;
      case MISC_B:
      case MISC_I:
        return "MISC";
        break;
      case PER_B:
      case PER_I:
        return "PER";
        break;
      case PRO_B:
      case PRO_I:
        return "PRO";
        break;
      case NONER:
        return "";
        break;
      default:
        return "invalid NER value";
    };
  }

  ostream& operator<<(ostream& os, Type n) {
    os << toString(n);
    return os;
  }

  SEM::Type toSem(Type n) {
    switch ( n ) {
      case LOC_B:
      case LOC_I:
        return SEM::BROAD_CONCRETE_PLACE_NOUN;
        break;
      case ORG_B:
      case ORG_I:
        return SEM::INSTITUT_NOUN;
        break;
      case PER_B:
      case PER_I:
        return SEM::CONCRETE_HUMAN_NOUN;
        break;
      default:
        return SEM::UNFOUND_NOUN;
        break;
    };
  }
}
