#ifndef AFK_H
#define	AFK_H

#include <string>
#include <iostream>

namespace Afk {
  enum Type { 
    NO_A, OVERHEID_A, ZORG_A, ONDERWIJS_A,
    JURIDISCH_A, OVERIGE_A,
    INTERNATIONAAL_A, MEDIA_A, GENERIEK_A 
  };
  std::string toString(Type);
  Type classify(const std::string&);
  std::ostream& operator<<(std::ostream& os, const Type& s);
}

#endif /* AFK_H */
