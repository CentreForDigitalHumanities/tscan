#ifndef UTILS_H
#define	UTILS_H

#include <map>
#include <cmath>
#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include "ticcutils/StringOps.h"
#include "libfolia/folia.h"

void addOneMetric( folia::Document*, folia::FoliaElement*, const std::string&, const std::string& );
void argument_overlap( const std::string, const std::vector<std::string>&, int& );
std::istream& safe_getline( std::istream&, std::string& );
std::string toMString( double d );
std::string escape_quotes(const std::string &before);

template<class T> int at( const std::map<T,int>& m, const T key ) {
  typename std::map<T,int>::const_iterator it = m.find( key );
  if ( it != m.end() )
    return it->second;
  else
    return 0;
}

template<class M> void aggregate( M& out, const M& in ){
  typename M::const_iterator ii = in.begin();
  while ( ii != in.end() ){
    typename M::iterator oi = out.find( ii->first );
    if ( oi == out.end() ){
      out[ii->first] = ii->second;
    }
    else {
      oi->second += ii->second;
    }
    ++ii;
  }
}

struct proportion {
  proportion( double d1, double d2 ) {
    if ( d2 == 0 || std::isnan(d1) || std::isnan(d2) )
      p = NAN;
    else
      p = d1/d2;
  };
  double p;
};

struct density {
  density( double d1, double d2 ) {
    if ( d2 == 0 || std::isnan(d1) || std::isnan(d2) )
      d = NAN;
    else
      d = (d1/d2) * 1000;
  };
  double d;
};

std::ostream& operator<<(std::ostream&, const proportion&);
std::ostream& operator<<(std::ostream&, const density&);

#endif /* UTILS_H */
