#ifndef CGN_H
#define	CGN_H

namespace CGN {

    enum Type {
        UNASS, ADJ, BW, LET, LID, N, SPEC, TSW, TW, VG, VNW, VZ, WW
    };
    Type toCGN(std::string);
    std::string toString(Type);
    
    enum Prop { ISNAME, ISLET,
	       ISVD, ISOD, ISINF, ISPVTGW, ISPVVERL, ISSUBJ,
	       ISPPRON1, ISPPRON2, ISPPRON3, ISAANW,
	       JUSTAWORD, NOTAWORD };
    std::string toString(Prop);
    
    enum Position { 
        NOMIN, PRENOM, VRIJ, NOPOS 
    };
    std::string toString(Position);
}

#endif	/* CGN_H */

