#ifndef CONN_H
#define CONN_H

#include <string>
#include <iostream>

namespace Conn {

    enum Type {
        NOCONN, TEMPOREEL, OPSOMMEND_WG, OPSOMMEND_ZIN, CONTRASTIEF, COMPARATIEF, CAUSAAL
    };
    std::string toString(Type);
    std::ostream& operator<<( std::ostream&, const Type&);
}

#endif /* CONN_H */
