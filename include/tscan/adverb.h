#ifndef ADVERB_H
#define ADVERB_H

#include <string>
#include <iostream>

namespace Adverb {
  enum Type {
    GENERAL,
    SPECIFIC,
    NO_ADVERB
  };
  std::string toString(Type);
  Type classify(const std::string&);
  std::ostream& operator<<(std::ostream&, const Type&);
}

#endif /* ADVERB_H */
