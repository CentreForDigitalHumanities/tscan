#ifndef CGN_H
#define	CGN_H

#include <string>
#include <iostream>

namespace CGN {

    enum Type {
        UNASS, ADJ, BW, LET, LID, N, SPEC, TSW, TW, VG, VNW, VZ, WW
    };
    Type toCGN(std::string);
    std::string toString(Type);
    std::ostream& operator<<( std::ostream& os, const Type& t );
    
    enum Prop { ISNAME, ISLET,
	       ISVD, ISOD, ISINF, ISPVTGW, ISPVVERL, ISSUBJ,
	       ISPPRON1, ISPPRON2, ISPPRON3, ISAANW,
	       JUSTAWORD, NOTAWORD };
    std::string toString(Prop);
    std::ostream& operator<<( std::ostream& os, const Prop& p );
    
    enum Position { 
        NOMIN, PRENOM, VRIJ, NOPOS 
    };
    std::string toString(Position);
    std::ostream& operator<<( std::ostream& os, const Position& p );

}

#endif	/* CGN_H */

