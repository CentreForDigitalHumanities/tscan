#ifndef FORMAL_H
#define	FORMAL_H

#include <string>
#include <iostream>

namespace Formal {

    enum Type {
        BVNW, BW, VGW, VNW, VZ, VZG, WW, ZNW, NOT_FORMAL, INVALID
    };
    std::string toString(Type);
    Type classify(const std::string&);
}

#endif /* FORMAL_H */
