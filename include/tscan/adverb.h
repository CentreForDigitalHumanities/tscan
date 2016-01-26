#ifndef ADVERB_H
#define ADVERB_H

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
