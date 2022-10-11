#ifndef UTILS_H
#define UTILS_H

#include <map>
#include <cmath>
#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include "ticcutils/StringOps.h"
#include "libfolia/folia.h"

static std::string suffixesArray[] = { "e", "en", "s" };

void addOneMetric( folia::Document*, folia::FoliaElement*, const std::string&, const std::string& );
void argument_overlap( const std::string&, const std::vector<std::string>&, int& );
std::istream& safe_getline( std::istream&, std::string& );
void updateCounter( std::map<std::string, int>&, std::map<std::string, int>);
std::string toStringCounter( std::map<std::string, int>);
std::string toMString( double d );
std::string escape_quotes(const std::string &before);

/**
 * Search a maps for the passed word and also tries searching it
 * using common inflection endings if no match is found.
 * @tparam T the mapped type
 * @param map the map to search
 * @param val the word or lemma to search
 * @return map<string, T>::const_iterator
 */
template <typename T>
typename std::map<std::string, T>::const_iterator findInflected( const std::map<std::string, T> &m, const std::string &val ) {
  auto sit = m.find( val );
  static std::vector<std::string> suffixes = std::vector<std::string>(
      suffixesArray,
      suffixesArray + sizeof( suffixesArray ) / sizeof( std::string ) );
  size_t i = 0;
  size_t val_length = val.length();
  while ( sit == m.end() && i < suffixes.size() ) {
    std::string suffix = suffixes[i];
    size_t suffix_length = suffix.length();
    size_t suffix_start = val_length - suffix_length;
    if ( val_length > suffix_length && 0 == val.compare( suffix_start, suffix_length, suffix ) ) {
      // maybe it's in the map without this suffix?
      sit = m.find( val.substr( 0, suffix_start ) );
    }
    else {
      // maybe it's in the map with this suffix?
      sit = m.find( val + suffix );
    }
    ++i;
  }

  return sit;
}

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
