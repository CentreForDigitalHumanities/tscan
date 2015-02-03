#ifndef INTENSIFY_H
#define	INTENSIFY_H

namespace Intensify {

    enum Type {
        BVBW, BVNW, BW, COMBI, NW, TUSS, WW, NO_INTENSIFY
    };
    std::string toString(Type);
    Type classify(const std::string&);
}

#endif	/* INTENSIFY_H */

