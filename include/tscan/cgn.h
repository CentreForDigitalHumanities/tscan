#ifndef CGN_H
#define	CGN_H

namespace CGN {

    enum Type {
        UNASS, ADJ, BW, LET, LID, N, SPEC, TSW, TW, VG, VNW, VZ, WW
    };
    Type toCGN(std::string);
    std::string toString(Type);
}

#endif	/* CGN_H */

