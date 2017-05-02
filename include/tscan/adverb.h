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

  enum SubType {
    AMBIGUOUS,
    ANAPHORIC,
    GRADE,
    QUANTITY,
    MODAL,
    MODAL_PARTICLE,
    NEGATION,
    RELATION,
    SPACE,
    SPACE_TIME,
    TIME,
    INTERJECTION,
    MANNER,
    OTHER,
    NO_ADVERB_SUBTYPE,
  };

  struct adverb {
    Type type;
    SubType subtype;
  };

  std::string toString(Type);
  Type classifyType(const std::string&);
  SubType classifySubType(const std::string&);
  bool isContent(SubType);
  std::ostream& operator<<(std::ostream&, const Type&);
}

#endif /* ADVERB_H */
