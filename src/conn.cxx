#include "tscan/conn.h"

using namespace std;

namespace Conn {
  string toString(Type c) {
    if ( c == NOCONN )
      return "no";
    else if ( c == TEMPOREEL )
      return "temporeel";
    else if ( c == OPSOMMEND_WG )
      return "opsommend_wg";
    else if ( c == OPSOMMEND_ZIN )
      return "opsommend_zin";
    else if ( c == CONTRASTIEF )
      return "contrastief";
    else if ( c == COMPARATIEF )
      return "comparatief";
    else if ( c == CAUSAAL )
      return "causaal";
    else
      throw "no translation for ConnType";
  }

  ostream& operator<<(ostream& os, const Type& s) {
    os << toString(s);
    return os;
  }
}
