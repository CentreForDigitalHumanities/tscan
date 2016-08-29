#include <cmath>
#include <iostream>
#include <set>
#include "ticcutils/StringOps.h"
#include "tscan/utils.h"

using namespace std;


/**
 * Adds a Metric to a FoliaElement in a FoliaDocument.
 * @param doc    the current FoliaDocument
 * @param parent the current FoliaElement
 * @param cls    the class of the new Metric
 * @param val    the value of the new Metric
 */
void addOneMetric( folia::Document *doc, folia::FoliaElement *parent, const string& cls, const string& val ) {
  folia::Metric *m = new folia::Metric( folia::getArgs( "class='" + cls + "', value='" + val + "'" ), doc );
  parent->append( m );
}

/**
 * Calculates the overlap of the Word or Lemma with the buffer
 * @param w_or_l          word or lemma
 * @param buffer          current buffer
 * @param arg_overlap_cnt resulting count
 */
void argument_overlap( const string w_or_l, const vector<string>& buffer, int& arg_overlap_cnt ) {
  if ( buffer.empty() )
    return;
  // cerr << "test overlap, lemma/word= " << w_or_l << endl;
  // cerr << "buffer=" << buffer << endl;
  static string vnw_1sA[] = {"ik", "mij", "me", "mijn" };
  static set<string> vnw_1s = set<string>( vnw_1sA,
             vnw_1sA + sizeof(vnw_1sA)/sizeof(string) );
  static string vnw_2sA[] = {"jij", "je", "jou", "jouw" };
  static set<string> vnw_2s = set<string>( vnw_2sA,
             vnw_2sA + sizeof(vnw_2sA)/sizeof(string) );
  static string vnw_3smA[] = {"hij", "hem", "zijn" };
  static set<string> vnw_3sm = set<string>( vnw_3smA,
              vnw_3smA + sizeof(vnw_3smA)/sizeof(string) );
  static string vnw_3sfA[] = {"zij", "ze", "haar"};
  static set<string> vnw_3sf = set<string>( vnw_3sfA,
              vnw_3sfA + sizeof(vnw_3sfA)/sizeof(string) );
  static string vnw_1pA[] = {"wij", "we", "ons", "onze"};
  static set<string> vnw_1p = set<string>( vnw_1pA,
             vnw_1pA + sizeof(vnw_1pA)/sizeof(string) );
  static string vnw_2pA[] = {"jullie"};
  static set<string> vnw_2p = set<string>( vnw_2pA,
             vnw_2pA + sizeof(vnw_2pA)/sizeof(string) );
  static string vnw_3pA[] = {"zij", "ze", "hen", "hun"};
  static set<string> vnw_3p = set<string>( vnw_3pA,
             vnw_3pA + sizeof(vnw_3pA)/sizeof(string) );

  for( size_t i=0; i < buffer.size(); ++i ){
    if ( w_or_l == buffer[i] ){
      ++arg_overlap_cnt;
      break;
    }
    else if ( vnw_1s.find( w_or_l ) != vnw_1s.end() &&
        vnw_1s.find( buffer[i] ) != vnw_1s.end() ){
      ++arg_overlap_cnt;
      break;
    }
    else if ( vnw_2s.find( w_or_l ) != vnw_2s.end() &&
        vnw_2s.find( buffer[i] ) != vnw_2s.end() ){
      ++arg_overlap_cnt;
      break;
    }
    else if ( vnw_3sm.find( w_or_l ) != vnw_3sm.end() &&
        vnw_3sm.find( buffer[i] ) != vnw_3sm.end() ){
      ++arg_overlap_cnt;
      break;
    }
    else if ( vnw_3sf.find( w_or_l ) != vnw_3sf.end() &&
        vnw_3sf.find( buffer[i] ) != vnw_3sf.end() ){
      ++arg_overlap_cnt;
      break;
    }
    else if ( vnw_1p.find( w_or_l ) != vnw_1p.end() &&
        vnw_1p.find( buffer[i] ) != vnw_1p.end() ){
      ++arg_overlap_cnt;
       break;
   }
    else if ( vnw_2p.find( w_or_l ) != vnw_2p.end() &&
        vnw_2p.find( buffer[i] ) != vnw_2p.end() ){
      ++arg_overlap_cnt;
      break;
    }
    else if ( vnw_3p.find( w_or_l ) != vnw_3p.end() &&
        vnw_3p.find( buffer[i] ) != vnw_3p.end() ){
      ++arg_overlap_cnt;
      break;
    }
  }
}

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
