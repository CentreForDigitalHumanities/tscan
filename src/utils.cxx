#include <cmath>
#include <iostream>
#include "ticcutils/StringOps.h"
#include "tscan/utils.h"

using namespace std;

/**
 * Reads a line and deals with all possible line endings (Unix, Windows, Mac)
 * Copied from http://stackoverflow.com/a/6089413
 * @param  is the inputstream
 * @param  t  the string
 * @return    the inputstream without line endings
 */
istream& safe_getline( istream& is, string& t ){
  t.clear();

  // The characters in the stream are read one-by-one using a std::streambuf.
  // That is faster than reading them one-by-one using the std::istream.
  // Code that uses streambuf this way must be guarded by a sentry object.
  // The sentry object performs various tasks,
  // such as thread synchronization and updating the stream state.

  istream::sentry se(is, true);
  streambuf* sb = is.rdbuf();

  for(;;) {
    int c = sb->sbumpc();
    switch (c) {
    case '\n':
      return is;
    case '\r':
      if(sb->sgetc() == '\n'){
	sb->sbumpc();
      }
      return is;
    case EOF:
      // Also handle the case when the last line has no line ending
      if(t.empty())
	is.setstate(std::ios::eofbit);
      return is;
    default:
      t += (char)c;
    }
  }
}

/**
 * Converts a double to string, if NAN, return "NA"
 * @param  d the double
 * @return   the double as a string
 */
string toMString( double d ){
  if ( std::isnan(d) )
    return "NA";
  else
    return TiCC::toString( d );
}

/**
 * Escapes quotes from a string.
 * Found on http://stackoverflow.com/a/1162786
 * @param  before the original string
 * @return        the escaped string
 */
string escape_quotes(const string &before)
{
  string after;

  for (string::size_type i = 0; i < before.length(); ++i) {
    switch (before[i]) {
      case '"':
        after += '"'; // duplicate quotes
      default:
        after += before[i];
    }
  }

  return after;
}

/**
 * Implements the << operator for proportions.
 */
ostream& operator<<( ostream& os, const proportion& p ) {
  if ( std::isnan(p.p) )
    os << "NA";
  else
    os << p.p;
  return os;
}

/**
 * Implements the << operator for densities.
 */
ostream& operator<<( ostream& os, const density& d ) {
  if ( std::isnan(d.d) )
    os << "NA";
  else
    os << d.d;
  return os;
}
