#ifndef SITUATION_H
#define	SITUATION_H

#include <string>
#include <iostream>

namespace Situation {
    enum Type {
      NO_SIT, TIME_SIT, CAUSAL_SIT, SPACE_SIT, EMO_SIT
    };
    std::string toString(Type);
    std::ostream& operator<<(std::ostream& os, const Type& s);
}

#endif /* SITUATION_H */
