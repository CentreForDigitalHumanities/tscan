/*
  T-scan

  Copyright (c) 1998 - 2018

  This file is part of tscan

  tscan is free software; you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  tscan is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affere General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

*/

#include <string>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

#include "ticcutils/FdStream.h"
#include "ticcutils/ServerBase.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/XMLtools.h"
#include "ticcutils/Unicode.h"
#include "libfolia/folia.h"
#include "frog/FrogAPI.h"
#include "tscan/Alpino.h"
#include "tscan/cgn.h"
#include "tscan/sem.h"
#include "tscan/intensify.h"
#include "tscan/conn.h"
#include "tscan/general.h"
#include "tscan/situation.h"
#include "tscan/afk.h"
#include "tscan/adverb.h"
#include "tscan/ner.h"
#include "tscan/utils.h"
#include "tscan/stats.h"

using namespace std;

const string frog_pos_set = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
const string frog_lemma_set = "http://ilk.uvt.nl/folia/sets/frog-mblem-nl";
const string frog_morph_set = "http://ilk.uvt.nl/folia/sets/frog-mbma-nl";

string configFile = "tscan.cfg";
string probFilename = "problems.log";
ofstream problemFile;
TiCC::Configuration config;
string workdir_name;

struct cf_data {
  long int count;
  double freq;
};

struct noun {
  noun() :
      type( SEM::NO_SEMTYPE ), is_compound( false ), compound_parts( 0 ){};
  SEM::Type type;
  bool is_compound;
  string head;
  string satellite_clean;
  int compound_parts;
};

struct prevalence {
  double percentage;
  double zscore;
};

struct tagged_classification {
  CGN::Type tag;
  string classification;
};

struct settingData {
  void init( const TiCC::Configuration & );
  bool doAlpino;
  bool doAlpinoServer;
  bool doWopr;
  bool doXfiles;
  bool showProblems;
  bool sentencePerLine;
  string style;
  int rarityLevel;
  unsigned int overlapSize;
  double freq_clip;
  double mtld_threshold;
  map<string, SEM::Type> adj_sem;
  map<string, noun> noun_sem;
  map<string, SEM::Type> verb_sem;
  map<string, Intensify::Type> intensify;
  map<string, General::Type> general_nouns;
  map<string, General::Type> general_verbs;
  map<string, Adverb::adverb> adverbs;
  map<string, double> pol_lex;
  map<string, cf_data> staph_word_freq_lex;
  long int staph_total;
  map<string, cf_data> word_freq_lex;
  long int word_total;
  map<string, cf_data> lemma_freq_lex;
  long int lemma_total;
  map<string, top_val> top_freq_lex;
  map<CGN::Type, set<string>> temporals1;
  set<string> multi_temporals;
  map<CGN::Type, set<string>> causals1;
  set<string> multi_causals;
  map<CGN::Type, set<string>> opsommers_wg;
  set<string> multi_opsommers_wg;
  map<CGN::Type, set<string>> opsommers_zin;
  set<string> multi_opsommers_zin;
  map<CGN::Type, set<string>> contrast1;
  set<string> multi_contrast;
  map<CGN::Type, set<string>> compars1;
  set<string> multi_compars;
  map<CGN::Type, set<string>> causal_sits;
  set<string> multi_causal_sits;
  map<CGN::Type, set<string>> space_sits;
  set<string> multi_space_sits;
  map<CGN::Type, set<string>> time_sits;
  set<string> multi_time_sits;
  map<CGN::Type, set<string>> emotion_sits;
  set<string> multi_emotion_sits;
  set<string> vzexpr2;
  set<string> vzexpr3;
  set<string> vzexpr4;
  map<string, Afk::Type> afkos;
  map<string, prevalence> prevalences;
  map<CGN::Type, set<string>> stop_lemmata;
  map<string, tagged_classification> my_classification;
};

settingData settings;

bool fillN( map<string, noun> &m, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    // Trim the lines
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;

    // Split at a tab; the line should contain either 3 (non-compounds) or 6 (compounds) values
    vector<string> parts;
    int i = TiCC::split_at( line, parts, "\t" );
    if ( i != 3 && i != 6 ) {
      cerr << "skip line: " << line << " (expected 3 or 6 values, got " << i << ")" << endl;
      continue;
    }

    // Classify the noun, set the compound values and add the noun to the map
    noun n;
    n.type = SEM::classifyNoun( parts[1] );
    n.is_compound = parts[2] == "1";
    if ( n.is_compound ) {
      n.head = parts[3];
      n.satellite_clean = parts[4];
      n.compound_parts = atoi( parts[5].c_str() );
    }
    m[parts[0]] = n;
  }
  return true;
}

bool fillN( map<string, noun> &m, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fillN( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fillWW( map<string, SEM::Type> &m, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    vector<string> parts;
    int n = TiCC::split_at( line, parts, "\t" ); // split at tab
    if ( n != 3 ) {
      cerr << "skip line: " << line << " (expected 3 values, got "
           << n << ")" << endl;
      continue;
    }
    SEM::Type res = SEM::classifyWW( parts[1], parts[2] );
    if ( res != SEM::UNFOUND_VERB ) {
      // no use to store undefined values
      m[parts[0]] = res;
    }
  }
  return true;
}

bool fillADJ( map<string, SEM::Type> &m, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    vector<string> parts;
    int n = TiCC::split_at( line, parts, "\t" ); // split at tab
    if ( n < 2 || n > 3 ) {
      cerr << "skip line: " << line << " (expected 2 or 3 values, got "
           << n << ")" << endl;
      continue;
    }
    SEM::Type res = SEM::UNFOUND_ADJ;
    if ( n == 2 ) {
      res = SEM::classifyADJ( parts[1] );
    }
    else {
      res = SEM::classifyADJ( parts[1], parts[2] );
    }
    string low = TiCC::lowercase( parts[0] );
    if ( m.find( low ) != m.end() ) {
      cerr << "Information: multiple entry '" << low << "' in ADJ lex" << endl;
    }
    if ( res != SEM::UNFOUND_ADJ ) {
      // no use to store undefined values
      m[low] = res;
    }
  }
  return true;
}

bool fill( CGN::Type tag, map<string, SEM::Type> &m, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    if ( tag == CGN::WW )
      return fillWW( m, is );
    else if ( tag == CGN::ADJ )
      return fillADJ( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_intensify( map<string, Intensify::Type> &m, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    vector<string> parts;
    int n = TiCC::split_at( line, parts, "\t" ); // split at tab
    if ( n < 2 || n > 2 ) {
      cerr << "skip line: " << line << " (expected 2 values, got "
           << n << ")" << endl;
      continue;
    }
    string low = TiCC::trim( TiCC::lowercase( parts[0] ) );
    Intensify::Type res = Intensify::classify( TiCC::lowercase( parts[1] ) );
    if ( m.find( low ) != m.end() ) {
      cerr << "Information: multiple entry '" << low << "' in Intensify lex" << endl;
    }
    if ( res != Intensify::NO_INTENSIFY ) {
      // no use to store undefined values
      m[low] = res;
    }
  }
  return true;
}

bool fill_intensify( map<string, Intensify::Type> &m, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill_intensify( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_general( map<string, General::Type> &m, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    vector<string> parts;
    int n = TiCC::split_at( line, parts, "\t" ); // split at tab
    if ( n < 2 || n > 2 ) {
      cerr << "skip line: " << line << " (expected 2 values, got "
           << n << ")" << endl;
      continue;
    }
    string low = TiCC::trim( TiCC::lowercase( parts[0] ) );
    General::Type res = General::classify( TiCC::lowercase( parts[1] ) );
    if ( m.find( low ) != m.end() ) {
      cerr << "Information: multiple entry '" << low << "' in general lex" << endl;
    }
    if ( res != General::NO_GENERAL ) {
      // no use to store undefined values
      m[low] = res;
    }
  }
  return true;
}

bool fill_general( map<string, General::Type> &m, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill_general( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_adverbs( map<string, Adverb::adverb> &m, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    vector<string> parts;
    int n = TiCC::split_at( line, parts, "\t" ); // split at tab
    if ( n != 3 ) {
      cerr << "skip line: " << line << " (expected 3 values, got "
           << n << ")" << endl;
      continue;
    }
    string low = TiCC::trim( TiCC::lowercase( parts[0] ) );
    Adverb::adverb a;
    a.type = Adverb::classifyType( TiCC::lowercase( parts[1] ) );
    a.subtype = Adverb::classifySubType( TiCC::lowercase( parts[2] ) );
    if ( m.find( low ) != m.end() ) {
      cerr << "Information: multiple entry '" << low << "' in adverbs lex" << endl;
    }
    if ( a.type != Adverb::NO_ADVERB ) {
      // no use to store undefined values
      m[low] = a;
    }
  }
  return true;
}

bool fill_adverbs( map<string, Adverb::adverb> &m, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill_adverbs( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_freqlex( map<string, cf_data> &m, long int &total, istream &is ) {
  total = 0;
  string line;
  while ( safe_getline( is, line ) ) {
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    vector<string> parts;
    size_t n = TiCC::split_at( line, parts, "\t" ); // split at tabs
    if ( n != 4 ) {
      cerr << "skip line: " << line << " (expected 4 values, got "
           << n << ")" << endl;
      continue;
    }
    cf_data data;
    data.count = TiCC::stringTo<long int>( parts[1] );
    data.freq = TiCC::stringTo<double>( parts[3] );
    if ( data.count == 1 ) {
      // we are done. Skip all singleton stuff
      return true;
    }
    if ( settings.freq_clip > 0 ) {
      // skip low frequent word, when desired
      if ( data.freq > settings.freq_clip ) {
        return true;
      }
    }
    total += data.count;
    m[parts[0]] = data;
  }
  return true;
}

bool fill_freqlex( map<string, cf_data> &m, long int &total,
                   const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    fill_freqlex( m, total, is );
    cout << "read " << filename << " (" << total << " entries)" << endl;
    return true;
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_topvals( map<string, top_val> &m, istream &is ) {
  string line;
  int line_count = 0;
  top_val val = top2000;
  while ( safe_getline( is, line ) ) {
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    ++line_count;
    if ( line_count > 10000 )
      val = top20000;
    else if ( line_count > 5000 )
      val = top10000;
    else if ( line_count > 3000 )
      val = top5000;
    else if ( line_count > 2000 )
      val = top3000;
    else if ( line_count > 1000 )
      val = top2000;
    else
      val = top1000;
    vector<string> parts;
    size_t n = TiCC::split_at( line, parts, "\t" ); // split at tabs
    if ( n != 4 ) {
      cerr << "skip line: " << line << " (expected 2 values, got "
           << n << ")" << endl;
      continue;
    }
    m[parts[0]] = val;
  }
  return true;
}

bool fill_topvals( map<string, top_val> &m, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill_topvals( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_connectors( map<CGN::Type, set<string>> &c1,
                      set<string> &cM,
                      istream &is ) {
  cM.clear();
  string line;
  while ( safe_getline( is, line ) ) {
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR an entry of 1 to 4 words seperated by a single space
    // like: 'dus' OR 'de facto'
    // OR the 1 word followed by a TAB ('\t') and a CGN tag
    // like: 'maar   VG'
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = TiCC::split_at( line, vec, "\t" );
    if ( n == 0 || n > 2 ) {
      cerr << "skip line: " << line << " (expected 1 or 2 values, got "
           << n << ")" << endl;
      continue;
    }
    CGN::Type tag = CGN::UNASS;
    if ( n == 2 ) {
      tag = CGN::toCGN( vec[1] );
    }
    vector<string> dum;
    n = TiCC::split_at( vec[0], dum, " " );
    if ( n < 1 || n > 4 ) {
      cerr << "skip line: " << line
           << " (expected 1, to 4 values in the first part: " << vec[0]
           << ", got " << n << ")" << endl;
      continue;
    }
    if ( n == 1 ) {
      c1[tag].insert( vec[0] );
    }
    else if ( n > 1 && tag != CGN::UNASS ) {
      cerr << "skip line: " << line
           << " (no GCN tag info allowed for multiword entries) " << endl;
      continue;
    }
    else {
      cM.insert( vec[0] );
    }
  }
  return true;
}

bool fill_connectors( map<CGN::Type, set<string>> &c1,
                      set<string> &cM,
                      const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill_connectors( c1, cM, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_vzexpr( set<string> &vz2, set<string> &vz3, set<string> &vz4,
                  istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR an entry of 2, 3 or 4 words seperated by whitespace
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = TiCC::split_at_first_of( line, vec, " \t" );
    if ( n == 0 || n > 4 ) {
      cerr << "skip line: " << line << " (expected 2, 3 or 4 values, got "
           << n << ")" << endl;
      continue;
    }
    switch ( n ) {
      case 2: {
        string line = vec[0] + " " + vec[1];
        vz2.insert( line );
      } break;
      case 3: {
        string line = vec[0] + " " + vec[1] + " " + vec[2];
        vz3.insert( line );
      } break;
      case 4: {
        string line = vec[0] + " " + vec[1] + " " + vec[2] + " " + vec[3];
        vz4.insert( line );
      } break;
      default:
        throw logic_error( "switch out of range" );
    }
  }
  return true;
}

bool fill_vzexpr( set<string> &vz2, set<string> &vz3, set<string> &vz4,
                  const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill_vzexpr( vz2, vz3, vz4, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill( map<string, Afk::Type> &afkos, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR an entry of 2 words seperated by whitespace
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = TiCC::split_at_first_of( line, vec, " \t" );
    if ( n < 2 ) {
      cerr << "skip line: " << line << " (expected at least 2 values, got "
           << n << ")" << endl;
      continue;
    }
    if ( n == 2 ) {
      Afk::Type at = Afk::classify( vec[1] );
      if ( at != Afk::NO_A )
        afkos[vec[0]] = at;
    }
    else if ( n == 3 ) {
      Afk::Type at = Afk::classify( vec[2] );
      if ( at != Afk::NO_A ) {
        string s = vec[0] + " " + vec[1];
        afkos[s] = at;
      }
    }
    else if ( n == 4 ) {
      Afk::Type at = Afk::classify( vec[3] );
      if ( at != Afk::NO_A ) {
        string s = vec[0] + " " + vec[1] + " " + vec[2];
        afkos[s] = at;
      }
    }
    else {
      cerr << "skip line: " << line << " (expected at most 4 values, got "
           << n << ")" << endl;
      continue;
    }
  }
  return true;
}

bool fill( map<string, Afk::Type> &afks, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill( afks, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_prevalences( map<string, prevalence> &prevalences, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR: a lemma with prevalence values in columns 2 and 3
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = TiCC::split_at_first_of( line, vec, " \t" );
    if ( n != 6 ) {
      cerr << "skip line: " << line << " (expected 6 values, got " << n << ")" << endl;
      continue;
    }
    else {
      prevalence p;
      p.percentage = TiCC::stringTo<double>( vec[2] );
      p.zscore = TiCC::stringTo<double>( vec[3] );
      prevalences[vec[0]] = p;
    }
  }
  return true;
}

bool fill_prevalences( map<string, prevalence> &prevalences, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill_prevalences( prevalences, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_stop_lemmata( map<CGN::Type, set<string>> &stop_lemmata, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR: a lemma followed by an optional CGN tag
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = TiCC::split_at_first_of( line, vec, " \t" );
    if ( n == 0 || n > 2 ) {
      cerr << "skip line: " << line << " (expected 1 or 2 values, got "
           << n << ")" << endl;
      continue;
    }
    CGN::Type tag = CGN::UNASS;
    if ( n == 2 ) {
      tag = CGN::toCGN( vec[1] );
    }
    stop_lemmata[tag].insert( vec[0] );
  }
  return true;
}

bool fill_stop_lemmata( map<CGN::Type, set<string>> &stop_lemmata, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill_stop_lemmata( stop_lemmata, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill( map<string, tagged_classification> &my_classification, istream &is ) {
  string line;
  while ( safe_getline( is, line ) ) {
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR: a lemma with an optional pos-tag, followed by a categorization
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = TiCC::split_at_first_of( line, vec, " \t" );
    if ( n < 2 ) {
      cerr << "skip line: " << line << " (expected at least 2 values, got " << n << ")" << endl;
      continue;
    }
    if ( n == 2 ) {
      tagged_classification tc;
      tc.tag = CGN::UNASS;
      tc.classification = vec[1];
      my_classification[vec[0]] = tc;
    }
    else if ( n == 3 ) {
      tagged_classification tc;
      tc.tag = CGN::toCGN( vec[1] );
      tc.classification = vec[2];
      my_classification[vec[0]] = tc;
    }
    else {
      cerr << "skip line: " << line << " (expected at most 3 values, got " << n << ")" << endl;
      continue;
    }
  }
  return true;
}

bool fill( map<string, tagged_classification> &my_classification, const string &filename ) {
  ifstream is( filename.c_str() );
  if ( is ) {
    return fill( my_classification, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

void settingData::init( const TiCC::Configuration &cf ) {
  doXfiles = true;
  doAlpino = false;
  doAlpinoServer = false;
  string val = cf.lookUp( "useAlpinoServer" );
  if ( !val.empty() ) {
    if ( !TiCC::stringTo( val, doAlpinoServer ) ) {
      cerr << "invalid value for 'useAlpinoServer' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( !doAlpinoServer ) {
    val = cf.lookUp( "useAlpino" );
    if ( !TiCC::stringTo( val, doAlpino ) ) {
      cerr << "invalid value for 'useAlpino' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  doWopr = false;
  val = cf.lookUp( "useWopr" );
  if ( !val.empty() ) {
    if ( !TiCC::stringTo( val, doWopr ) ) {
      cerr << "invalid value for 'useWopr' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  showProblems = true;
  val = cf.lookUp( "logProblems" );
  if ( !val.empty() ) {
    if ( !TiCC::stringTo( val, showProblems ) ) {
      cerr << "invalid value for 'showProblems' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  sentencePerLine = false;
  val = cf.lookUp( "sentencePerLine" );
  if ( !val.empty() ) {
    if ( !TiCC::stringTo( val, sentencePerLine ) ) {
      cerr << "invalid value for 'sentencePerLine' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  val = cf.lookUp( "styleSheet" );
  if ( !val.empty() ) {
    style = val;
  }
  val = cf.lookUp( "rarityLevel" );
  if ( val.empty() ) {
    rarityLevel = 10;
  }
  else if ( !TiCC::stringTo( val, rarityLevel ) ) {
    cerr << "invalid value for 'rarityLevel' in config file" << endl;
  }
  val = cf.lookUp( "overlapSize" );
  if ( val.empty() ) {
    overlapSize = 50;
  }
  else if ( !TiCC::stringTo( val, overlapSize ) ) {
    cerr << "invalid value for 'overlapSize' in config file" << endl;
    exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "frequencyClip" );
  if ( val.empty() ) {
    freq_clip = 90;
  }
  else if ( !TiCC::stringTo( val, freq_clip )
            || ( freq_clip < 0 ) || ( freq_clip > 100 ) ) {
    cerr << "invalid value for 'frequencyClip' in config file" << endl;
    exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "mtldThreshold" );
  if ( val.empty() ) {
    mtld_threshold = 0.720;
  }
  else if ( !TiCC::stringTo( val, mtld_threshold )
            || ( mtld_threshold < 0 ) || ( mtld_threshold > 1.0 ) ) {
    cerr << "invalid value for 'frequencyClip' in config file" << endl;
    exit( EXIT_FAILURE );
  }

  val = cf.lookUp( "adj_semtypes" );
  if ( !val.empty() ) {
    if ( !fill( CGN::ADJ, adj_sem, val ) ) // 20150316: Full path necessary to allow custom input
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "noun_semtypes" );
  if ( !val.empty() ) {
    if ( !fillN( noun_sem, val ) ) // 20141121: Full path necessary to allow custom input
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "verb_semtypes" );
  if ( !val.empty() ) {
    if ( !fill( CGN::WW, verb_sem, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "intensify" );
  if ( !val.empty() ) {
    if ( !fill_intensify( intensify, val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "general_nouns" );
  if ( !val.empty() ) {
    if ( !fill_general( general_nouns, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "general_verbs" );
  if ( !val.empty() ) {
    if ( !fill_general( general_verbs, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "adverbs" );
  if ( !val.empty() ) {
    if ( !fill_adverbs( adverbs, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  staph_total = 0;
  val = cf.lookUp( "staph_word_freq_lex" );
  if ( !val.empty() ) {
    if ( !fill_freqlex( staph_word_freq_lex, staph_total,
                        cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  word_total = 0;
  val = cf.lookUp( "word_freq_lex" );
  if ( !val.empty() ) {
    if ( !fill_freqlex( word_freq_lex, word_total,
                        cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  lemma_total = 0;
  val = cf.lookUp( "lemma_freq_lex" );
  if ( !val.empty() ) {
    if ( !fill_freqlex( lemma_freq_lex, lemma_total,
                        cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "top_freq_lex" );
  if ( !val.empty() ) {
    if ( !fill_topvals( top_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "temporals" );
  if ( !val.empty() ) {
    if ( !fill_connectors( temporals1, multi_temporals, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "opsom_connectors_wg" );
  if ( !val.empty() ) {
    if ( !fill_connectors( opsommers_wg, multi_opsommers_wg, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "opsom_connectors_zin" );
  if ( !val.empty() ) {
    if ( !fill_connectors( opsommers_zin, multi_opsommers_zin, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "contrast" );
  if ( !val.empty() ) {
    if ( !fill_connectors( contrast1, multi_contrast, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "compars" );
  if ( !val.empty() ) {
    if ( !fill_connectors( compars1, multi_compars, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "causals" );
  if ( !val.empty() ) {
    if ( !fill_connectors( causals1, multi_causals, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "causal_situation" );
  if ( !val.empty() ) {
    if ( !fill_connectors( causal_sits, multi_causal_sits, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "space_situation" );
  if ( !val.empty() ) {
    if ( !fill_connectors( space_sits, multi_space_sits, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "time_situation" );
  if ( !val.empty() ) {
    if ( !fill_connectors( time_sits, multi_time_sits, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }

  val = cf.lookUp( "emotion_situation" );
  if ( !val.empty() ) {
    if ( !fill_connectors( emotion_sits, multi_emotion_sits, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }

  val = cf.lookUp( "voorzetselexpr" );
  if ( !val.empty() ) {
    if ( !fill_vzexpr( vzexpr2, vzexpr3, vzexpr4, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "afkortingen" );
  if ( !val.empty() ) {
    if ( !fill( afkos, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "prevalence" );
  if ( !val.empty() ) {
    if ( !fill_prevalences( prevalences, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }

  val = cf.lookUp( "stop_lemmata" );
  if ( !val.empty() ) {
    if ( !fill_stop_lemmata( stop_lemmata, val ) )
      exit( EXIT_FAILURE );
  }

  val = cf.lookUp( "my_classification" );
  if ( !val.empty() ) {
    if ( !fill( my_classification, val ) ) // full path necessary to allow custom input
      exit( EXIT_FAILURE );
  }
}

inline void usage() {
  cerr << "usage:  tscan [options] <inputfiles> " << endl;
  cerr << "options: " << endl;
  cerr << "\t-o <file> store XML in 'file' " << endl;
  cerr << "\t--config=<file> read configuration from 'file' " << endl;
  cerr << "\t-V or --version show version " << endl;
  cerr << "\t-n assume input file to hold one sentence per line" << endl;
  cerr << "\t--skip=[aclw]    Skip Alpino (a), CSV output (c) or Wopr (w).\n";
  cerr << "\t-t <file> process the 'file'. (deprecated)" << endl;
  cerr << endl;
}

Conn::Type wordStats::checkConnective() const {
  if ( tag != CGN::VG && tag != CGN::VZ && tag != CGN::BW )
    return Conn::NOCONN;

  if ( settings.temporals1[tag].find( lemma )
       != settings.temporals1[tag].end() )
    return Conn::TEMPOREEL;
  else if ( settings.temporals1[CGN::UNASS].find( lemma )
            != settings.temporals1[CGN::UNASS].end() )
    return Conn::TEMPOREEL;

  else if ( settings.opsommers_wg[tag].find( lemma )
            != settings.opsommers_wg[tag].end() )
    return Conn::OPSOMMEND_WG;
  else if ( settings.opsommers_wg[CGN::UNASS].find( lemma )
            != settings.opsommers_wg[CGN::UNASS].end() )
    return Conn::OPSOMMEND_WG;

  else if ( settings.opsommers_zin[tag].find( lemma )
            != settings.opsommers_zin[tag].end() )
    return Conn::OPSOMMEND_ZIN;
  else if ( settings.opsommers_zin[CGN::UNASS].find( lemma )
            != settings.opsommers_zin[CGN::UNASS].end() )
    return Conn::OPSOMMEND_ZIN;

  else if ( settings.contrast1[tag].find( lemma )
            != settings.contrast1[tag].end() )
    return Conn::CONTRASTIEF;
  else if ( settings.contrast1[CGN::UNASS].find( lemma )
            != settings.contrast1[CGN::UNASS].end() )
    return Conn::CONTRASTIEF;

  else if ( settings.compars1[tag].find( lemma )
            != settings.compars1[tag].end() )
    return Conn::COMPARATIEF;
  else if ( settings.compars1[CGN::UNASS].find( lemma )
            != settings.compars1[CGN::UNASS].end() )
    return Conn::COMPARATIEF;

  else if ( settings.causals1[tag].find( lemma )
            != settings.causals1[tag].end() )
    return Conn::CAUSAAL;
  else if ( settings.causals1[CGN::UNASS].find( lemma )
            != settings.causals1[CGN::UNASS].end() )
    return Conn::CAUSAAL;

  return Conn::NOCONN;
}

Situation::Type wordStats::checkSituation() const {
  if ( settings.time_sits[tag].find( lemma )
       != settings.time_sits[tag].end() ) {
    return Situation::TIME_SIT;
  }
  else if ( settings.time_sits[CGN::UNASS].find( lemma )
            != settings.time_sits[CGN::UNASS].end() ) {
    return Situation::TIME_SIT;
  }
  else if ( settings.causal_sits[tag].find( lemma )
            != settings.causal_sits[tag].end() ) {
    return Situation::CAUSAL_SIT;
  }
  else if ( settings.causal_sits[CGN::UNASS].find( lemma )
            != settings.causal_sits[CGN::UNASS].end() ) {
    return Situation::CAUSAL_SIT;
  }
  else if ( settings.space_sits[tag].find( lemma )
            != settings.space_sits[tag].end() ) {
    return Situation::SPACE_SIT;
  }
  else if ( settings.space_sits[CGN::UNASS].find( lemma )
            != settings.space_sits[CGN::UNASS].end() ) {
    return Situation::SPACE_SIT;
  }
  else if ( settings.emotion_sits[tag].find( lemma )
            != settings.emotion_sits[tag].end() ) {
    return Situation::EMO_SIT;
  }
  else if ( settings.emotion_sits[CGN::UNASS].find( lemma )
            != settings.emotion_sits[CGN::UNASS].end() ) {
    return Situation::EMO_SIT;
  }
  return Situation::NO_SIT;
}

noun splitCompound(const string &lemma) {
  noun n;
  // open connection
  string host = config.lookUp( "host", "compound_splitter" );
  string port = config.lookUp( "port", "compound_splitter" );
  string method = config.lookUp( "method", "compound_splitter" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ) {
    cerr << "failed to open compound splitter connection: " << host << ":" << port << endl;
    cerr << "Reason: " << client.getMessage() << endl;
  }
  else {
    cerr << "calling compound splitter for " << lemma << endl;
    client.write( lemma + "," + method );
    string result;
    client.read(result);
    cerr << "done with compound splitter" << endl;

    // store result in noun struct
    vector<string> parts;
    int size = TiCC::split_at( result, parts, "," );
    if (size > 1) {
      n.is_compound = true;
      n.head = parts[size -1];
      n.compound_parts = size;
      //n.satellite_clean = ??;
    }
    else {
      n.is_compound = false;
    }
  }
  return n;
}

void wordStats::checkNoun() {
  if ( tag == CGN::N ) {
    cerr << "lookup " << lemma << endl;
    map<string, noun>::const_iterator sit = settings.noun_sem.find( lemma );
    if ( sit != settings.noun_sem.end() ) {
      noun n = sit->second;
      sem_type = n.type;
      if ( n.is_compound ) {
        is_compound = n.is_compound;
        compound_parts = n.compound_parts;
        compound_head = n.head;
        compound_sat = n.satellite_clean;
      }
    }
    else {
      // call compound splitter
      bool found_split = false;
      if (config.lookUp("useCompoundSplitter") == "1") {
        noun n = splitCompound(lemma);
        if ( n.is_compound ) {
          // look for head in data
          sit = settings.noun_sem.find( n.head );
          if ( sit != settings.noun_sem.end() ) {
            // if there is a match, fill in results
            found_split = true;  
            noun match = sit->second;
            sem_type = match.type;
            is_compound = n.is_compound;
            compound_parts = n.compound_parts;
            compound_head = n.head;
            //compound_sat = ??;
          }
        }
      }
      if (found_split == false) {
        // If we still haven't found a SEM::Type, add this to the problemfile
        cerr << "unknown noun " << lemma << endl;
        sem_type = SEM::UNFOUND_NOUN;
        if ( settings.showProblems ) {
          problemFile << "N," << word << ", " << lemma << endl;
        }
      }
    }
  }
}

SEM::Type wordStats::checkSemProps() const {
  if ( prop == CGN::ISNAME ) {
    // Names are te be looked up in the Noun list too, but use the word instead of the lemma (case-sensitivity)
    SEM::Type sem = SEM::UNFOUND_NOUN;
    map<string, noun>::const_iterator sit = settings.noun_sem.find( word );
    if ( sit != settings.noun_sem.end() ) {
      sem = sit->second.type;
    }
    return sem;
  }
  else if ( tag == CGN::ADJ ) {
    //    cerr << "ADJ check semtype " << l_lemma << endl;
    SEM::Type sem = SEM::UNFOUND_ADJ;
    map<string, SEM::Type>::const_iterator sit = settings.adj_sem.find( l_lemma );
    if ( sit == settings.adj_sem.end() ) {
      // lemma not found. maybe the whole word?
      //      cerr << "ADJ check semtype " << word << endl;
      sit = settings.adj_sem.find( l_word );
    }
    if ( sit != settings.adj_sem.end() ) {
      sem = sit->second;
    }
    else if ( settings.showProblems ) {
      problemFile << "ADJ," << l_word << "," << l_lemma << endl;
    }
    //    cerr << "found semtype " << sem << endl;
    return sem;
  }
  else if ( tag == CGN::WW ) {
    //    cerr << "check semtype " << lemma << endl;
    SEM::Type sem = SEM::UNFOUND_VERB;
    map<string, SEM::Type>::const_iterator sit = settings.verb_sem.end();
    if ( !full_lemma.empty() ) {
      sit = settings.verb_sem.find( full_lemma );
    }
    if ( sit == settings.verb_sem.end() ) {
      if ( position == CGN::PRENOM
           && ( prop == CGN::ISVD || prop == CGN::ISOD ) ) {
        // might be a 'hidden' adj!
        //	cerr << "lookup a probable ADJ " << prop << " (" << word << ") " << endl;
        sit = settings.adj_sem.find( l_word );
        if ( sit == settings.adj_sem.end() )
          sit = settings.verb_sem.end();
      }
    }
    if ( sit == settings.verb_sem.end() ) {
      //      cerr << "lookup lemma as verb (" << lemma << ") " << endl;
      sit = settings.verb_sem.find( l_lemma );
    }
    if ( sit != settings.verb_sem.end() ) {
      sem = sit->second;
    }
    else if ( settings.showProblems ) {
      problemFile << "WW," << l_word << "," << l_lemma;
      if ( !full_lemma.empty() )
        problemFile << "," << full_lemma << endl;
      else
        problemFile << endl;
    }
    //    cerr << "found semtype " << sem << endl;
    return sem;
  }
  return SEM::NO_SEMTYPE;
}

// Looks up the Intensity type for a word, or NO_INTENSIFY if not found
Intensify::Type wordStats::checkIntensify( const xmlNode *alpWord ) const {
  Intensify::Type res = Intensify::NO_INTENSIFY;

  // First check the full lemma (if available), then the normal lemma
  map<string, Intensify::Type>::const_iterator sit = settings.intensify.end();
  if ( !full_lemma.empty() ) {
    sit = settings.intensify.find( full_lemma );
  }
  if ( sit == settings.intensify.end() ) {
    sit = settings.intensify.find( lemma );
  }

  if ( sit != settings.intensify.end() ) {
    res = sit->second;

    // Special case for BVBW: check if this is not a modifier
    if ( res == Intensify::BVBW ) {
      if ( !checkModifier( alpWord ) ) res = Intensify::NO_INTENSIFY;
    }
  }
  return res;
}

// Looks up the General type for a noun (based on lemma), or NO_GENERAL if not found
General::Type wordStats::checkGeneralNoun() const {
  if ( tag == CGN::N ) {
    map<string, General::Type>::const_iterator sit = settings.general_nouns.find( lemma );
    if ( sit != settings.general_nouns.end() ) {
      return sit->second;
    }
  }
  return General::NO_GENERAL;
}

// Looks up the General type for a verb (based on (full) lemma), or NO_GENERAL if not found
General::Type wordStats::checkGeneralVerb() const {
  if ( tag == CGN::WW ) {
    // First check the full lemma (if available), then the normal lemma
    map<string, General::Type>::const_iterator sit = settings.general_verbs.end();
    if ( !full_lemma.empty() ) {
      sit = settings.general_verbs.find( full_lemma );
    }
    if ( sit == settings.general_verbs.end() ) {
      sit = settings.general_verbs.find( lemma );
    }

    if ( sit != settings.general_verbs.end() ) {
      return sit->second;
    }
  }
  return General::NO_GENERAL;
}

Adverb::Type checkAdverbType( string word, CGN::Type tag ) {
  if ( tag == CGN::BW ) {
    map<string, Adverb::adverb>::const_iterator sit = settings.adverbs.find( word );
    if ( sit != settings.adverbs.end() ) {
      return sit->second.type;
    }
  }
  return Adverb::NO_ADVERB;
}

Adverb::SubType checkAdverbSubType( string word, CGN::Type tag ) {
  if ( tag == CGN::BW ) {
    map<string, Adverb::adverb>::const_iterator sit = settings.adverbs.find( word );
    if ( sit != settings.adverbs.end() ) {
      return sit->second.subtype;
    }
  }
  return Adverb::NO_ADVERB_SUBTYPE;
}

Afk::Type wordStats::checkAfk() const {
  if ( tag == CGN::N || tag == CGN::SPEC ) {
    map<string, Afk::Type>::const_iterator sit = settings.afkos.find( word );
    if ( sit != settings.afkos.end() ) {
      return sit->second;
    }
  }
  return Afk::NO_A;
}

// Returns the self-defined classification for a lemma (if its tag is correct)
string wordStats::checkMyClassification() const {
  string result;
  map<string, tagged_classification>::const_iterator sit = settings.my_classification.find( lemma );
  if ( sit != settings.my_classification.end() ) {
    tagged_classification tc = sit->second;
    if ( tc.tag == CGN::UNASS || tc.tag == tag ) {
      result = tc.classification;
    }
  }
  return result;
}

// Returns whether the lemma appears on the stoplist
bool wordStats::checkStoplist() const {
  bool result = false;

  if ( settings.stop_lemmata[tag].find( lemma )
       != settings.stop_lemmata[tag].end() ) {
    result = true;
  }
  else if ( settings.stop_lemmata[CGN::UNASS].find( lemma )
            != settings.stop_lemmata[CGN::UNASS].end() ) {
    result = true;
  }
  return result;
}

// Returns the position of a word in the top-20000 lexicon
top_val wordStats::topFreqLookup( const string &w ) const {
  map<string, top_val>::const_iterator it = settings.top_freq_lex.find( w );
  top_val result = notFound;
  if ( it != settings.top_freq_lex.end() ) {
    result = it->second;
  }
  return result;
}

// Returns the frequency of a word in the word lexicon
int wordStats::wordFreqLookup( const string &w ) const {
  int result = 0;
  map<string, cf_data>::const_iterator it = settings.word_freq_lex.find( w );
  if ( it != settings.word_freq_lex.end() ) {
    result = it->second.count;
  }
  return result;
}

// Returns the log of the frequency per billion words (with Laplace transformation)
// See http://crr.ugent.be/papers/van_Heuven_et_al_SUBTLEX-UK.pdf
double freqLog( const long int &freq, const long int &total ) {
  return log10( ( ( freq + 1 ) / double( total ) ) * 1e9 );
}

// Find the frequencies of words and lemmata
void wordStats::freqLookup() {
  word_freq = wordFreqLookup( l_word );
  word_freq_log = freqLog( word_freq, settings.word_total );

  map<string, cf_data>::const_iterator it = settings.lemma_freq_lex.end();
  if ( !full_lemma.empty() ) {
    // scheidbaar ww
    it = settings.lemma_freq_lex.find( full_lemma );
  }
  if ( it == settings.lemma_freq_lex.end() ) {
    it = settings.lemma_freq_lex.find( l_lemma );
  }
  if ( it != settings.lemma_freq_lex.end() ) {
    lemma_freq = it->second.count;
    lemma_freq_log = freqLog( lemma_freq, settings.lemma_total );
  }
  else {
    lemma_freq = 0;
    lemma_freq_log = freqLog( lemma_freq, settings.lemma_total );
  }
}

void wordStats::prevalenceLookup() {
  map<string, prevalence>::const_iterator it = settings.prevalences.find( l_lemma );
  if ( it != settings.prevalences.end() ) {
    prevalenceP = it->second.percentage;
    prevalenceZ = it->second.zscore;
  }
}

void wordStats::staphFreqLookup() {
  map<string, cf_data>::const_iterator it = settings.staph_word_freq_lex.find( l_word );
  if ( it != settings.staph_word_freq_lex.end() ) {
    double freq = it->second.freq;
    if ( freq <= 50 )
      f50 = true;
    if ( freq <= 65 )
      f65 = true;
    if ( freq <= 77 )
      f77 = true;
    if ( freq <= 80 )
      f80 = true;
  }
}

wordStats::wordStats( int index,
                      folia::Word *w,
                      const xmlNode *alpWord,
                      const set<size_t> &puncts,
                      bool fail ) :
    basicStats( index, w, "word" ),
    parseFail( fail ), wwform( ::NO_VERB ),
    isPersRef( false ), isPronRef( false ),
    archaic( false ), isContent( false ), isContentStrict( false ),
    isNominal( false ), isOnder( false ), isImperative( false ),
    isBetr( false ), isPropNeg( false ), isMorphNeg( false ),
    nerProp( NER::NONER ), connType( Conn::NOCONN ), isMultiConn( false ), sitType( Situation::NO_SIT ),
    prevalenceP( NAN ), prevalenceZ( NAN ),
    f50( false ), f65( false ), f77( false ), f80( false ),
    top_freq( notFound ), word_freq( 0 ), lemma_freq( 0 ),
    wordOverlapCnt( 0 ), lemmaOverlapCnt( 0 ),
    word_freq_log( NAN ), lemma_freq_log( NAN ),
    logprob10_fwd( NAN ), logprob10_bwd( NAN ),
    prop( CGN::JUSTAWORD ), position( CGN::NOPOS ),
    sem_type( SEM::NO_SEMTYPE ), intensify_type( Intensify::NO_INTENSIFY ),
    general_noun_type( General::NO_GENERAL ), general_verb_type( General::NO_GENERAL ),
    adverb_type( Adverb::NO_ADVERB ), adverb_sub_type( Adverb::NO_ADVERB_SUBTYPE ),
    afkType( Afk::NO_A ), is_compound( false ), compound_parts( 0 ),
    word_freq_log_head( NAN ), word_freq_log_sat( NAN ), word_freq_log_head_sat( NAN ), word_freq_log_corr( NAN ), on_stoplist( false ) {
  icu::UnicodeString us = w->text();
  charCnt = us.length();
  word = TiCC::UnicodeToUTF8( us );
  l_word = TiCC::UnicodeToUTF8( us.toLower() );
  if ( fail )
    return;
  vector<folia::PosAnnotation *> posV = w->select<folia::PosAnnotation>( frog_pos_set );
  if ( posV.size() != 1 )
    throw folia::ValueError( "word doesn't have Frog POS tag info" );
  folia::PosAnnotation *pa = posV[0];
  pos = pa->cls();
  tag = CGN::toCGN( pa->feat( "head" ) );
  lemma = w->lemma( frog_lemma_set );
  us = TiCC::UnicodeFromUTF8( lemma );
  l_lemma = TiCC::UnicodeToUTF8( us.toLower() );

  setCGNProps( pa );
  if ( alpWord ) {
    distances = getDependencyDist( alpWord, puncts );
    if ( tag == CGN::WW ) {
      string full;
      wwform = classifyVerb( alpWord, lemma, full );
      if ( !full.empty() ) {
        TiCC::to_lower( full );
        //	cerr << "scheidbaar WW: " << full << endl;
        full_lemma = full;
      }
      if ( ( prop == CGN::ISPVTGW || prop == CGN::ISPVVERL ) && wwform != PASSIVE_VERB ) {
        isImperative = checkImp( alpWord );
      }
    }
  }
  if ( prop != CGN::ISLET ) {
    vector<string> mv = get_full_morph_analysis( w, true );
    // get_full_morph_amalysis returns 1 or more morpheme sequences
    // like [appel][taart] of [veilig][heid]
    // there may be more readings/morpheme lists:
    // [ge][naken][t] versus [genaak][t]
    size_t max = 0;
    size_t pos = 0;
    size_t match_pos = 0;
    for ( auto const s : mv ) {
      vector<string> parts;
      TiCC::split_at_first_of( s, parts, "[]" );
      if ( parts.size() > max ) {
        // a hack: we assume the longest morpheme list to
        // be the best choice.
        morphemes = parts;
        max = parts.size();
        match_pos = pos;
      }
      ++pos;
    }
    if ( morphemes.size() == 0 ) {
      cerr << "unable to retrieve morphemes from folia." << endl;
    }
    //    cerr << "Morphemes " << word << "= " << morphemes << endl;
    vector<string> cmps = get_compound_analysis( w );
    //    cerr << "Comps " << word << "= " << cmps << endl;
    if ( cmps.size() > match_pos ) {
      // this might not be the case e.g. when frog isn't started
      // with the --deep-morph option!
      compstr = cmps[match_pos];
    }
    isPropNeg = checkPropNeg();
    isMorphNeg = checkMorphNeg();
    connType = checkConnective();
    sitType = checkSituation();
    morphCnt = morphemes.size();
    if ( prop != CGN::ISNAME ) {
      charCntExNames = charCnt;
      morphCntExNames = morphCnt;
    }
    sem_type = checkSemProps();
    checkNoun();
    intensify_type = checkIntensify( alpWord );
    general_noun_type = checkGeneralNoun();
    general_verb_type = checkGeneralVerb();
    adverb_type = checkAdverbType( l_word, tag );
    adverb_sub_type = checkAdverbSubType( l_word, tag );
    afkType = checkAfk();
    if ( alpWord )
      isNominal = checkNominal( alpWord );
    top_freq = topFreqLookup( l_word );
    prevalenceLookup();
    staphFreqLookup();
    isContent = checkContent( false );
    isContentStrict = checkContent( true );
    if ( isContent ) {
      freqLookup();
    }
    if ( is_compound ) {
      charCntHead = compound_head.length();
      charCntSat = compound_sat.length();
      word_freq_log_head = freqLog( wordFreqLookup( compound_head ), settings.word_total );
      word_freq_log_sat = freqLog( wordFreqLookup( compound_sat ), settings.word_total );
      word_freq_log_head_sat = ( word_freq_log_head + word_freq_log_sat ) / double( 2 );
      top_freq_head = topFreqLookup( compound_head );
      top_freq_sat = topFreqLookup( compound_sat );
      word_freq_log_corr = word_freq_log_head;
    }
    else {
      word_freq_log_corr = word_freq_log;
    }
    on_stoplist = checkStoplist();
    my_classification = checkMyClassification();
  }
}

//#define DEBUG_MTLD

double calculate_mtld( const vector<string> &v ) {
  if ( v.size() == 0 ) {
    return 0.0;
  }
  int token_count = 0;
  set<string> unique_tokens;
  double token_factor = 0.0;
  for ( size_t i = 0; i < v.size(); ++i ) {
    ++token_count;
    unique_tokens.insert( v[i] );
    double token_ttr = unique_tokens.size() / double( token_count );
#ifdef DEBUG_MTLD
    cerr << v[i] << "\t [" << unique_tokens.size() << "/"
         << token_count << "] >> ttr " << token_ttr << endl;
#endif
    if ( token_ttr <= settings.mtld_threshold ) {
#ifdef KOIZUMI
      if ( token_count >= 10 ) {
        token_factor += 1.0;
      }
#else
      token_factor += 1.0;
#endif
      token_count = 0;
      unique_tokens.clear();
#ifdef DEBUG_MTLD
      cerr << "\treset: token_factor = " << token_factor << endl
           << endl;
#endif
    }
    else if ( i == v.size() - 1 ) {
#ifdef DEBUG_MTLD
      cerr << "\trestje: huidige token_ttr= " << token_ttr;
#endif
      // partial result
      double threshold = ( 1 - token_ttr ) / ( 1 - settings.mtld_threshold );
#ifdef DEBUG_MTLD
      cerr << " dus verhoog de factor met " << ( 1 - token_ttr )
           << "/" << ( 1 - settings.mtld_threshold ) << endl;
#endif
      token_factor += threshold;
    }
  }
  if ( token_factor == 0.0 )
    token_factor = 1.0;
#ifdef DEBUG_MTLD
  cerr << "Factor = " << token_factor << " #words = " << v.size() << endl;
#endif
  return v.size() / token_factor;
}

double average_mtld( vector<string> &tokens ) {
#ifdef DEBUG_MTLD
  cerr << "bereken MTLD van " << tokens << endl;
#endif
  double mtld1 = calculate_mtld( tokens );
#ifdef DEBUG_MTLD
  cerr << "VOORUIT = " << mtld1 << endl;
#endif
  reverse( tokens.begin(), tokens.end() );
  double mtld2 = calculate_mtld( tokens );
#ifdef DEBUG_MTLD
  cerr << "ACHTERUIT = " << mtld2 << endl;
#endif
  double result = ( mtld1 + mtld2 ) / 2.0;
#ifdef DEBUG_MTLD
  cerr << "average mtld = " << result << endl;
#endif
  return result;
}

void structStats::calculate_MTLDs() {
  const vector<const wordStats *> wordNodes = collectWords();
  vector<string> words;
  vector<string> lemmas;
  vector<string> conts;
  vector<string> conts_strict;
  vector<string> names;
  vector<string> temp_conn;
  vector<string> reeks_wg_conn;
  vector<string> reeks_zin_conn;
  vector<string> contr_conn;
  vector<string> comp_conn;
  vector<string> cause_conn;
  vector<string> tijd_sits;
  vector<string> ruimte_sits;
  vector<string> cause_sits;
  vector<string> emotion_sits;
  for ( size_t i = 0; i < wordNodes.size(); ++i ) {
    if ( wordNodes[i]->wordProperty() == CGN::ISLET ) {
      continue;
    }
    string word = wordNodes[i]->ltext();
    words.push_back( word );
    string lemma = wordNodes[i]->llemma();
    lemmas.push_back( lemma );
    if ( wordNodes[i]->isContent ) {
      conts.push_back( wordNodes[i]->ltext() );
    }
    if ( wordNodes[i]->isContentStrict ) {
      conts_strict.push_back( wordNodes[i]->ltext() );
    }
    if ( wordNodes[i]->prop == CGN::ISNAME ) {
      names.push_back( wordNodes[i]->ltext() );
    }
    switch ( wordNodes[i]->getConnType() ) {
      case Conn::TEMPOREEL:
        temp_conn.push_back( wordNodes[i]->ltext() );
        break;
      case Conn::OPSOMMEND_WG:
        reeks_wg_conn.push_back( wordNodes[i]->ltext() );
        break;
      case Conn::OPSOMMEND_ZIN:
        reeks_zin_conn.push_back( wordNodes[i]->ltext() );
        break;
      case Conn::CONTRASTIEF:
        contr_conn.push_back( wordNodes[i]->ltext() );
        break;
      case Conn::COMPARATIEF:
        comp_conn.push_back( wordNodes[i]->ltext() );
        break;
      case Conn::CAUSAAL:
        cause_conn.push_back( wordNodes[i]->ltext() );
        break;
      default:
        break;
    }
    switch ( wordNodes[i]->getSitType() ) {
      case Situation::TIME_SIT:
        tijd_sits.push_back( wordNodes[i]->Lemma() );
        break;
      case Situation::CAUSAL_SIT:
        cause_sits.push_back( wordNodes[i]->Lemma() );
        break;
      case Situation::SPACE_SIT:
        ruimte_sits.push_back( wordNodes[i]->Lemma() );
        break;
      case Situation::EMO_SIT:
        emotion_sits.push_back( wordNodes[i]->Lemma() );
        break;
      default:
        break;
    }
  }

  word_mtld = average_mtld( words );
  lemma_mtld = average_mtld( lemmas );
  content_mtld = average_mtld( conts );
  content_mtld_strict = average_mtld( conts_strict );
  name_mtld = average_mtld( names );
  temp_conn_mtld = average_mtld( temp_conn );
  reeks_wg_conn_mtld = average_mtld( reeks_wg_conn );
  reeks_zin_conn_mtld = average_mtld( reeks_zin_conn );
  contr_conn_mtld = average_mtld( contr_conn );
  comp_conn_mtld = average_mtld( comp_conn );
  cause_conn_mtld = average_mtld( cause_conn );
  tijd_sit_mtld = average_mtld( tijd_sits );
  ruimte_sit_mtld = average_mtld( ruimte_sits );
  cause_sit_mtld = average_mtld( cause_sits );
  emotion_sit_mtld = average_mtld( emotion_sits );

  // Combined connective MLTD (but don't include reeks_wg_conn)
  vector<string> all_conn;
  all_conn.insert( all_conn.end(), temp_conn.begin(), temp_conn.end() );
  all_conn.insert( all_conn.end(), reeks_zin_conn.begin(), reeks_zin_conn.end() );
  all_conn.insert( all_conn.end(), contr_conn.begin(), contr_conn.end() );
  all_conn.insert( all_conn.end(), comp_conn.begin(), comp_conn.end() );
  all_conn.insert( all_conn.end(), cause_conn.begin(), cause_conn.end() );
  all_conn_mtld = average_mtld( all_conn );
}

//#define DEBUG_WOPR
void orderWopr( const string &type, const string &txt, vector<double> &wordProbsV,
                double &sentProb, double &entropy, double &perplexity ) {
  string host = config.lookUp( "host_" + type, "wopr" );
  string port = config.lookUp( "port_" + type, "wopr" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ) {
    cerr << "failed to open Wopr connection: " << host << ":" << port << endl;
    cerr << "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  cerr << "calling Wopr" << endl;
  client.write( txt + "\n\n" );
  string result;
  string s;
  while ( client.read( s ) ) {
    result += s + "\n";
  }
#ifdef DEBUG_WOPR
  cerr << "received data [" << result << "]" << endl;
#endif
  if ( !result.empty() && result.size() > 10 ) {
#ifdef DEBUG_WOPR
    cerr << "start FoLiA parsing" << endl;
#endif
    folia::Document doc;
    try {
      doc.readFromString( result );
#ifdef DEBUG_WOPR
      cerr << "finished parsing" << endl;
#endif
      vector<folia::Word *> wv = doc.words();
      if ( wv.size() != wordProbsV.size() ) {
        cerr << "unforseen mismatch between de number of words returned by WOPR"
             << endl
             << " and the number of words in the input sentence. "
             << endl;
        return;
      }
      for ( size_t i = 0; i < wv.size(); ++i ) {
        vector<folia::Metric *> mv = wv[i]->select<folia::Metric>();
        if ( mv.size() > 0 ) {
          for ( size_t j = 0; j < mv.size(); ++j ) {
            if ( mv[j]->cls() == "lprob10" ) {
              wordProbsV[i] = TiCC::stringTo<double>( mv[j]->feat( "value" ) );
            }
          }
        }
      }
      vector<folia::Sentence *> sv = doc.sentences();
      if ( sv.size() != 1 ) {
        throw logic_error( "The document returned by WOPR contains > 1 Sentence" );
      }
      vector<folia::Metric *> mv = sv[0]->select<folia::Metric>();
      if ( mv.size() > 0 ) {
        for ( size_t j = 0; j < mv.size(); ++j ) {
          if ( mv[j]->cls() == "avg_prob10" ) {
            string val = mv[j]->feat( "value" );
            if ( val != "nan" ) {
              sentProb = TiCC::stringTo<double>( val );
            }
          }
          else if ( mv[j]->cls() == "entropy" ) {
            string val = mv[j]->feat( "value" );
            if ( val != "nan" ) {
              entropy = TiCC::stringTo<double>( val );
            }
          }
          else if ( mv[j]->cls() == "perplexity" ) {
            string val = mv[j]->feat( "value" );
            if ( val != "nan" ) {
              perplexity = TiCC::stringTo<double>( val );
            }
          }
        }
      }
    }
    catch ( std::exception &e ) {
      cerr << "FoLiaParsing failed:" << endl
           << e.what() << endl;
    }
  }
  else {
    cerr << "No usable FoLia data retrieved from Wopr. Got '"
         << result << "'" << endl;
  }
  cerr << "done with Wopr" << endl;
}

xmlDoc *AlpinoServerParse( folia::Sentence * );

void fill_word_lemma_buffers( const sentStats *ss,
                              vector<string> &wv,
                              vector<string> &lv ) {
  vector<basicStats *> bv = ss->sv;
  for ( size_t i = 0; i < bv.size(); ++i ) {
    wordStats *w = dynamic_cast<wordStats *>( bv[i] );
    if ( w->isOverlapCandidate() ) {
      wv.push_back( w->l_word );
      lv.push_back( w->l_lemma );
    }
  }
}

void np_length( folia::Sentence *s, int &npcount, int &indefcount, int &size ) {
  vector<folia::Chunk *> cv = s->select<folia::Chunk>();
  size = 0;
  for ( size_t i = 0; i < cv.size(); ++i ) {
    if ( cv[i]->cls() == "NP" ) {
      ++npcount;
      size += cv[i]->size();
      folia::FoliaElement *det = cv[i]->index( 0 );
      if ( det ) {
        vector<folia::PosAnnotation *> posV = det->select<folia::PosAnnotation>( frog_pos_set );
        if ( posV.size() != 1 )
          throw folia::ValueError( "word doesn't have Frog POS tag info" );
        if ( posV[0]->feat( "head" ) == "LID" ) {
          if ( det->text() == "een" )
            ++indefcount;
        }
      }
    }
  }
}

sentStats::sentStats( int index, folia::Sentence *s, const sentStats *pred ) :
    structStats( index, s, "sent" ) {
  text = TiCC::UnicodeToUTF8( s->toktext() );
  cerr << "analyse tokenized sentence=" << text << endl;
  vector<folia::Word *> w = s->words();
  vector<double> woprProbsV_fwd( w.size(), NAN );
  vector<double> woprProbsV_bwd( w.size(), NAN );
  double sentProb_fwd = NAN;
  double sentProb_bwd = NAN;
  double sentEntropy_fwd = NAN;
  double sentEntropy_bwd = NAN;
  double sentPerplexity_fwd = NAN;
  double sentPerplexity_bwd = NAN;
  xmlDoc *alpDoc = 0;
  set<size_t> puncts;
  parseFailCnt = -1; // not parsed (yet)
#pragma omp parallel sections
  {
#pragma omp section
    {
      if ( settings.doAlpino || settings.doAlpinoServer ) {
        if ( settings.doAlpinoServer ) {
          cerr << "calling Alpino Server" << endl;
          alpDoc = AlpinoServerParse( s );
          if ( !alpDoc ) {
            cerr << "alpino parser failed!" << endl;
          }
          cerr << "done with Alpino Server" << endl;
        }
        else if ( settings.doAlpino ) {
          cerr << "calling Alpino parser" << endl;
          alpDoc = AlpinoParse( s, workdir_name );
          if ( !alpDoc ) {
            cerr << "alpino parser failed!" << endl;
          }
          cerr << "done with Alpino parser" << endl;
        }
        if ( alpDoc ) {
          parseFailCnt = 0; // OK
          for ( size_t i = 0; i < w.size(); ++i ) {
            vector<folia::PosAnnotation *> posV = w[i]->select<folia::PosAnnotation>( frog_pos_set );
            if ( posV.size() != 1 )
              throw folia::ValueError( "word doesn't have Frog POS tag info" );
            folia::PosAnnotation *pa = posV[0];
            string posHead = pa->feat( "head" );
            if ( posHead == "LET" ) {
              puncts.insert( i );
            }
          }
          dLevel = get_d_level( s, alpDoc );
          if ( dLevel > 4 )
            dLevel_gt4 = 1;
          mod_stats( alpDoc, adjNpModCnt, npModCnt );
          resolveAdverbials( alpDoc );
          resolveRelativeClauses( alpDoc );
          resolveFiniteVerbs( alpDoc );
          resolveConjunctions( alpDoc );
          resolveSmallConjunctions( alpDoc );
        }
        else {
          parseFailCnt = 1; // failed
        }
      }
    } // omp section

#pragma omp section
    {
      if ( settings.doWopr ) {
        orderWopr( "fwd", text, woprProbsV_fwd, sentProb_fwd, sentEntropy_fwd, sentPerplexity_fwd );
      }
    } // omp section
#pragma omp section
    {
      if ( settings.doWopr ) {
        orderWopr( "bwd", text, woprProbsV_bwd, sentProb_bwd, sentEntropy_bwd, sentPerplexity_bwd );
      }
    } // omp section
  }   // omp sections

  sentCnt = 1; // so only count the sentence when not failed

  bool question = false;
  vector<string> wordbuffer;
  vector<string> lemmabuffer;
  if ( pred ) {
    fill_word_lemma_buffers( pred, wordbuffer, lemmabuffer );
#ifdef DEBUG_OL
    cerr << "call sentenceOverlap, wordbuffer " << wordbuffer << endl;
    cerr << "call sentenceOverlap, lemmabuffer " << lemmabuffer << endl;
#endif
  }
  for ( size_t i = 0; i < w.size(); ++i ) {
    xmlNode *alpWord = 0;
    if ( alpDoc ) {
      alpWord = getAlpNodeWord( alpDoc, w[i] );
    }
    wordStats *ws = new wordStats( i, w[i], alpWord, puncts, parseFailCnt == 1 );
    if ( parseFailCnt ) {
      sv.push_back( ws );
      continue;
    }
    if ( woprProbsV_fwd[i] != -99 )
      ws->logprob10_fwd = woprProbsV_fwd[i];
    if ( woprProbsV_bwd[i] != -99 )
      ws->logprob10_bwd = woprProbsV_bwd[i];
    if ( pred ) {
      ws->getSentenceOverlap( wordbuffer, lemmabuffer );
    }

    if ( ws->lemma[ws->lemma.length() - 1] == '?' ) {
      question = true;
    }
    if ( ws->prop == CGN::ISLET ) {
      letCnt++;
      sv.push_back( ws );
      continue;
    }
    else if ( ws->on_stoplist ) {
      sentStats::setCommonCounts( ws );
      sv.push_back( ws );
      continue;
    }
    else {
      wordCnt++;
      if ( ws->prop == CGN::ISNAME ) nameCnt++;
      if ( ws->isContent ) contentCnt++;
      if ( ws->isContentStrict ) contentStrictCnt++;
      if ( ws->tag == CGN::N ) nounCnt++;
      if ( ws->tag == CGN::WW ) verbCnt++;
      if ( ws->tag == CGN::ADJ ) adjCnt++;

      NER::Type ner = NER::lookupNer( w[i], s );
      ws->nerProp = ner;

      // If we did not find a SEM::Type for a noun, use the NER::Type to possibly find one.
      if ( ws->sem_type == SEM::UNFOUND_NOUN ) {
        ws->sem_type = NER::toSem( ner );
      }

      switch ( ner ) {
        case NER::LOC_B:
        case NER::EVE_B:
        case NER::MISC_B:
        case NER::ORG_B:
        case NER::PER_B:
        case NER::PRO_B:
          ners[ner]++;
          nerCnt++;
          break;
        default:;
      }

      ws->isPersRef = ws->setPersRef(); // need NER Info for this

      sentStats::setCommonCounts( ws );

      heads[ws->tag]++;
      charCnt += ws->charCnt;
      charCntExNames += ws->charCntExNames;
      morphCnt += ws->morphCnt;
      morphCntExNames += ws->morphCntExNames;
      aggregate( distances, ws->distances );

      if ( ws->isContent ) {
        word_freq += ws->word_freq_log;
        lemma_freq += ws->lemma_freq_log;
        avg_prob10_fwd_content += ws->logprob10_fwd;
        avg_prob10_bwd_content += ws->logprob10_bwd;
        if ( ws->prop != CGN::ISNAME ) {
          word_freq_n += ws->word_freq_log;
          lemma_freq_n += ws->lemma_freq_log;
          avg_prob10_fwd_content_ex_names += ws->logprob10_fwd;
          avg_prob10_bwd_content_ex_names += ws->logprob10_bwd;
        }
      }
      if ( ws->isContentStrict ) {
        word_freq_strict += ws->word_freq_log;
        lemma_freq_strict += ws->lemma_freq_log;
        if ( ws->prop != CGN::ISNAME ) {
          word_freq_n_strict += ws->word_freq_log;
          lemma_freq_n_strict += ws->lemma_freq_log;
        }
      }
      if ( ws->prop != CGN::ISNAME ) {
        avg_prob10_fwd_ex_names += ws->logprob10_fwd;
        avg_prob10_bwd_ex_names += ws->logprob10_bwd;
      }

      if ( ws->isNominal ) nominalCnt++;

      if ( ws->f50 ) f50Cnt++;
      if ( ws->f65 ) f65Cnt++;
      if ( ws->f77 ) f77Cnt++;
      if ( ws->f80 ) f80Cnt++;

      switch ( ws->top_freq ) {
          // NO BREAKS (being in top1000 means being in top2000 as well)
        case top1000:
          ++top1000Cnt;
          if ( ws->isContent ) ++top1000ContentCnt;
          if ( ws->isContentStrict ) ++top1000ContentStrictCnt;
          // fallthrough
        case top2000:
          ++top2000Cnt;
          if ( ws->isContent ) ++top2000ContentCnt;
          if ( ws->isContentStrict ) ++top2000ContentStrictCnt;
          // fallthrough
        case top3000:
          ++top3000Cnt;
          if ( ws->isContent ) ++top3000ContentCnt;
          if ( ws->isContentStrict ) ++top3000ContentStrictCnt;
          // fallthrough
        case top5000:
          ++top5000Cnt;
          if ( ws->isContent ) ++top5000ContentCnt;
          if ( ws->isContentStrict ) ++top5000ContentStrictCnt;
          // fallthrough
        case top10000:
          ++top10000Cnt;
          if ( ws->isContent ) ++top10000ContentCnt;
          if ( ws->isContentStrict ) ++top10000ContentStrictCnt;
          // fallthrough
        case top20000:
          ++top20000Cnt;
          if ( ws->isContent ) ++top20000ContentCnt;
          if ( ws->isContentStrict ) ++top20000ContentStrictCnt;
          // fallthrough
        default:
          break;
      }

      switch ( ws->sem_type ) {
        case SEM::UNDEFINED_NOUN:
          ++undefinedNounCnt;
          break;
        case SEM::UNDEFINED_ADJ:
          ++undefinedAdjCnt;
          break;
        case SEM::UNFOUND_NOUN:
          ++uncoveredNounCnt;
          break;
        case SEM::UNFOUND_ADJ:
          ++uncoveredAdjCnt;
          break;
        case SEM::UNFOUND_VERB:
          ++uncoveredVerbCnt;
          break;
        case SEM::CONCRETE_HUMAN_NOUN:
          humanCnt++;
          strictNounCnt++;
          broadNounCnt++;
          break;
        case SEM::CONCRETE_NONHUMAN_NOUN:
          nonHumanCnt++;
          strictNounCnt++;
          broadNounCnt++;
          break;
        case SEM::CONCRETE_ARTEFACT_NOUN:
          artefactCnt++;
          strictNounCnt++;
          broadNounCnt++;
          break;
        case SEM::CONCRETE_SUBSTANCE_NOUN:
          substanceConcCnt++;
          strictNounCnt++;
          broadNounCnt++;
          break;
        case SEM::CONCRETE_FOOD_CARE_NOUN:
          foodcareCnt++;
          strictNounCnt++;
          broadNounCnt++;
          break;
        case SEM::CONCRETE_OTHER_NOUN:
          concrotherCnt++;
          strictNounCnt++;
          broadNounCnt++;
          break;
        case SEM::BROAD_CONCRETE_PLACE_NOUN:
          ++placeCnt;
          broadNounCnt++;
          break;
        case SEM::BROAD_CONCRETE_TIME_NOUN:
          ++timeCnt;
          broadNounCnt++;
          break;
        case SEM::BROAD_CONCRETE_MEASURE_NOUN:
          ++measureCnt;
          broadNounCnt++;
          break;
        case SEM::CONCRETE_DYNAMIC_NOUN:
          ++dynamicConcCnt;
          strictNounCnt++;
          broadNounCnt++;
          break;
        case SEM::ABSTRACT_SUBSTANCE_NOUN:
          ++substanceAbstrCnt;
          break;
        case SEM::ABSTRACT_DYNAMIC_NOUN:
          ++dynamicAbstrCnt;
          break;
        case SEM::ABSTRACT_NONDYNAMIC_NOUN:
          ++nonDynamicCnt;
          break;
        case SEM::INSTITUT_NOUN:
          institutCnt++;
          break;
        case SEM::HUMAN_ADJ:
          humanAdjCnt++;
          broadAdjCnt++;
          strictAdjCnt++;
          break;
        case SEM::EMO_ADJ:
          emoAdjCnt++;
          broadAdjCnt++;
          strictAdjCnt++;
          break;
        case SEM::NONHUMAN_SHAPE_ADJ:
          nonhumanAdjCnt++;
          shapeAdjCnt++;
          broadAdjCnt++;
          strictAdjCnt++;
          break;
        case SEM::NONHUMAN_COLOR_ADJ:
          nonhumanAdjCnt++;
          colorAdjCnt++;
          broadAdjCnt++;
          strictAdjCnt++;
          break;
        case SEM::NONHUMAN_MATTER_ADJ:
          nonhumanAdjCnt++;
          matterAdjCnt++;
          broadAdjCnt++;
          strictAdjCnt++;
          break;
        case SEM::NONHUMAN_SOUND_ADJ:
          nonhumanAdjCnt++;
          soundAdjCnt++;
          broadAdjCnt++;
          strictAdjCnt++;
          break;
        case SEM::NONHUMAN_OTHER_ADJ:
          nonhumanAdjCnt++;
          nonhumanOtherAdjCnt++;
          broadAdjCnt++;
          strictAdjCnt++;
          break;
        case SEM::TECH_ADJ:
          techAdjCnt++;
          break;
        case SEM::TIME_ADJ:
          timeAdjCnt++;
          broadAdjCnt++;
          break;
        case SEM::PLACE_ADJ:
          placeAdjCnt++;
          broadAdjCnt++;
          break;
        case SEM::SPEC_POS_ADJ:
          specPosAdjCnt++;
          subjectiveAdjCnt++;
          break;
        case SEM::SPEC_NEG_ADJ:
          specNegAdjCnt++;
          subjectiveAdjCnt++;
          break;
        case SEM::POS_ADJ:
          posAdjCnt++;
          subjectiveAdjCnt++;
          break;
        case SEM::NEG_ADJ:
          negAdjCnt++;
          subjectiveAdjCnt++;
          break;
        case SEM::EVALUATIVE_ADJ:
          evaluativeAdjCnt++;
          subjectiveAdjCnt++;
          break;
        case SEM::EPI_POS_ADJ:
          epiPosAdjCnt++;
          subjectiveAdjCnt++;
          break;
        case SEM::EPI_NEG_ADJ:
          epiNegAdjCnt++;
          subjectiveAdjCnt++;
          break;
        case SEM::ABSTRACT_ADJ:
          abstractAdjCnt++;
          break;
        case SEM::ABSTRACT_STATE:
          abstractWwCnt++;
          stateCnt++;
          break;
        case SEM::CONCRETE_STATE:
          concreteWwCnt++;
          stateCnt++;
          break;
        case SEM::UNDEFINED_STATE:
          undefinedWwCnt++;
          stateCnt++;
          break;
        case SEM::ABSTRACT_ACTION:
          abstractWwCnt++;
          actionCnt++;
          break;
        case SEM::CONCRETE_ACTION:
          concreteWwCnt++;
          actionCnt++;
          break;
        case SEM::UNDEFINED_ACTION:
          undefinedWwCnt++;
          actionCnt++;
          break;
        case SEM::ABSTRACT_PROCESS:
          abstractWwCnt++;
          processCnt++;
          break;
        case SEM::CONCRETE_PROCESS:
          concreteWwCnt++;
          processCnt++;
          break;
        case SEM::UNDEFINED_PROCESS:
          undefinedWwCnt++;
          processCnt++;
          break;
        case SEM::ABSTRACT_UNDEFINED:
          abstractWwCnt++;
          break;
        case SEM::CONCRETE_UNDEFINED:
          concreteWwCnt++;
          break;
        case SEM::UNDEFINED_VERB:
          undefinedWwCnt++;
          undefinedATPCnt++;
          break;
        default:;
      }

      // Counts for general nouns
      if ( ws->general_noun_type != General::NO_GENERAL ) generalNounCnt++;
      if ( General::isSeparate( ws->general_noun_type ) ) generalNounSepCnt++;
      if ( General::isRelated( ws->general_noun_type ) ) generalNounRelCnt++;
      if ( General::isActing( ws->general_noun_type ) ) generalNounActCnt++;
      if ( General::isKnowledge( ws->general_noun_type ) ) generalNounKnowCnt++;
      if ( General::isDiscussion( ws->general_noun_type ) ) generalNounDiscCnt++;
      if ( General::isDevelopment( ws->general_noun_type ) ) generalNounDeveCnt++;

      // Counts for general verbs
      if ( ws->general_verb_type != General::NO_GENERAL ) generalVerbCnt++;
      if ( General::isSeparate( ws->general_verb_type ) ) generalVerbSepCnt++;
      if ( General::isRelated( ws->general_verb_type ) ) generalVerbRelCnt++;
      if ( General::isActing( ws->general_verb_type ) ) generalVerbActCnt++;
      if ( General::isKnowledge( ws->general_verb_type ) ) generalVerbKnowCnt++;
      if ( General::isDiscussion( ws->general_verb_type ) ) generalVerbDiscCnt++;
      if ( General::isDevelopment( ws->general_verb_type ) ) generalVerbDeveCnt++;

      // Counts for compounds
      if ( ws->tag == CGN::N ) {
        charCntNoun += ws->charCnt;
        word_freq_log_noun += ws->word_freq_log;
        switch ( ws->top_freq ) {
          case top1000:
            top1000CntNoun++;
            // fallthrough
          case top2000:
          case top3000:
          case top5000:
            top5000CntNoun++;
            // fallthrough
          case top10000:
          case top20000:
            top20000CntNoun++;
            // fallthrough
          default:
            break;
        }

        if ( ws->is_compound ) {
          compoundCnt++;
          if ( ws->compound_parts == 3 ) {
            compound3Cnt++;
          }

          charCntComp += ws->charCnt;
          charCntHead += ws->charCntHead;
          charCntSat += ws->charCntSat;
          charCntNounCorr += ws->charCntHead;
          charCntCorr += ws->charCntHead;

          word_freq_log_comp += ws->word_freq_log;
          word_freq_log_head += ws->word_freq_log_head;
          word_freq_log_sat += ws->word_freq_log_sat;
          word_freq_log_head_sat += ws->word_freq_log_head_sat;
          word_freq_log_noun_corr += ws->word_freq_log_head;
          word_freq_log_corr += ws->word_freq_log_head;
          word_freq_log_n_corr += ws->word_freq_log_head;
          if ( ws->isContentStrict ) {
            word_freq_log_corr_strict += ws->word_freq_log_head;
            word_freq_log_n_corr_strict += ws->word_freq_log_head;
          }

          switch ( ws->top_freq ) {
            case top1000:
              top1000CntComp++;
              // fallthrough
            case top2000:
            case top3000:
            case top5000:
              top5000CntComp++;
              // fallthrough
            case top10000:
            // fallthrough
            case top20000:
              top20000CntComp++;
              // fallthrough
            default:
              break;
          }
          switch ( ws->top_freq_head ) {
            case top1000:
              top1000CntHead++;
              top1000CntNounCorr++;
              top1000CntCorr++;
              // fallthrough
            case top2000:
            case top3000:
            case top5000:
              top5000CntHead++;
              top5000CntNounCorr++;
              top5000CntCorr++;
              // fallthrough
            case top10000:
            case top20000:
              top20000CntHead++;
              top20000CntNounCorr++;
              top20000CntCorr++;
              // fallthrough
            default:
              break;
          }
          switch ( ws->top_freq_sat ) {
            case top1000:
              top1000CntSat++;
              // fallthrough
            case top2000:
            case top3000:
            case top5000:
              top5000CntSat++;
              // fallthrough
            case top10000:
            case top20000:
              top20000CntSat++;
              // fallthrough
            default:
              break;
          }
        }
        else {
          charCntNonComp += ws->charCnt;
          charCntNounCorr += ws->charCnt;
          charCntCorr += ws->charCnt;

          word_freq_log_non_comp += ws->word_freq_log;
          word_freq_log_noun_corr += ws->word_freq_log;
          word_freq_log_corr += ws->word_freq_log;
          word_freq_log_n_corr += ws->word_freq_log;
          if ( ws->isContentStrict ) {
            word_freq_log_corr_strict += ws->word_freq_log;
            word_freq_log_n_corr_strict += ws->word_freq_log;
          }

          switch ( ws->top_freq ) {
            case top1000:
              top1000CntNonComp++;
              top1000CntNounCorr++;
              top1000CntCorr++;
              // fallthrough
            case top2000:
            case top3000:
            case top5000:
              top5000CntNonComp++;
              top5000CntNounCorr++;
              top5000CntCorr++;
              // fallthrough
            case top10000:
            case top20000:
              top20000CntNonComp++;
              top20000CntNounCorr++;
              top20000CntCorr++;
              // fallthrough
            default:
              break;
          }
        }
      }
      else {
        charCntCorr += ws->charCnt;

        if ( ws->isContent ) {
          word_freq_log_corr += ws->word_freq_log;
          if ( ws->prop != CGN::ISNAME ) {
            word_freq_log_n_corr += ws->word_freq_log;
          }
        }
        if ( ws->isContentStrict ) {
          word_freq_log_corr_strict += ws->word_freq_log;
          if ( ws->prop != CGN::ISNAME ) {
            word_freq_log_n_corr_strict += ws->word_freq_log;
          }
        }

        switch ( ws->top_freq ) {
          case top1000:
            top1000CntCorr++;
            // fallthrough
          case top5000:
            top5000CntCorr++;
            // fallthrough
          case top20000:
            top20000CntCorr++;
            // fallthrough
          default: break;
        }
      }

      // Prevalences
      if ( !std::isnan( ws->prevalenceP ) ) {
        prevalenceP += ws->prevalenceP;
        prevalenceZ += ws->prevalenceZ;
        prevalenceCovered++;

        if ( ws->isContent ) {
          prevalenceContentP += ws->prevalenceP;
          prevalenceContentZ += ws->prevalenceZ;
          prevalenceContentCovered++;
        }
      }

      sv.push_back( ws );
    }
  }
  if ( alpDoc ) {
    xmlFreeDoc( alpDoc );
  }
  al_gem = getMeanAL();
  al_max = getHighestAL();
  resolveConnectives();
  resolveSituations();
  calculate_MTLDs();
  resolveMultiWordIntensify();
  // Disabled for now
  //  resolveMultiWordAfks();
  resolvePrepExpr();
  if ( question )
    questCnt = 1;
  if ( ( morphNegCnt + propNegCnt ) > 1 )
    multiNegCnt = 1;

  word_freq_log = proportion( word_freq, contentCnt ).p;
  lemma_freq_log = proportion( lemma_freq, contentCnt ).p;
  word_freq_log_n = proportion( word_freq_n, contentCnt - nameCnt ).p;
  lemma_freq_log_n = proportion( lemma_freq_n, contentCnt - nameCnt ).p;

  word_freq_log_strict = proportion( word_freq_strict, contentStrictCnt ).p;
  lemma_freq_log_strict = proportion( lemma_freq_strict, contentStrictCnt ).p;
  word_freq_log_n_strict = proportion( word_freq_n_strict, contentStrictCnt - nameCnt ).p;
  lemma_freq_log_n_strict = proportion( lemma_freq_n_strict, contentStrictCnt - nameCnt ).p;

  np_length( s, npCnt, indefNpCnt, npSize );
  rarityLevel = settings.rarityLevel;
  overlapSize = settings.overlapSize;

  // Assign and normalize the values from Wopr
  if ( sentProb_fwd != -99 ) {
    avg_prob10_fwd = sentProb_fwd;
  }
  if ( sentProb_bwd != -99 ) {
    avg_prob10_bwd = sentProb_bwd;
  }
  entropy_fwd = sentEntropy_fwd;
  entropy_bwd = sentEntropy_bwd;
  perplexity_fwd = sentPerplexity_fwd;
  perplexity_bwd = sentPerplexity_bwd;

  avg_prob10_fwd_content = proportion( avg_prob10_fwd_content, contentCnt ).p;
  avg_prob10_fwd_ex_names = proportion( avg_prob10_fwd_ex_names, wordCnt - nameCnt ).p;
  avg_prob10_fwd_content_ex_names = proportion( avg_prob10_fwd_content_ex_names, contentCnt - nameCnt ).p;
  avg_prob10_bwd_content = proportion( avg_prob10_bwd_content, contentCnt ).p;
  avg_prob10_bwd_ex_names = proportion( avg_prob10_bwd_ex_names, wordCnt - nameCnt ).p;
  avg_prob10_bwd_content_ex_names = proportion( avg_prob10_bwd_content_ex_names, contentCnt - nameCnt ).p;
  entropy_fwd_norm = proportion( entropy_fwd, w.size() ).p;
  entropy_bwd_norm = proportion( entropy_bwd, w.size() ).p;
  perplexity_fwd_norm = proportion( perplexity_fwd, pow( w.size(), 2 ) ).p;
  perplexity_bwd_norm = proportion( perplexity_bwd, pow( w.size(), 2 ) ).p;
}

Conn::Type sentStats::checkMultiConnectives( const string &mword ) {
  Conn::Type conn = Conn::NOCONN;
  if ( settings.multi_temporals.find( mword ) != settings.multi_temporals.end() ) {
    conn = Conn::TEMPOREEL;
  }
  else if ( settings.multi_opsommers_wg.find( mword ) != settings.multi_opsommers_wg.end() ) {
    conn = Conn::OPSOMMEND_WG;
  }
  else if ( settings.multi_opsommers_zin.find( mword ) != settings.multi_opsommers_zin.end() ) {
    conn = Conn::OPSOMMEND_ZIN;
  }
  else if ( settings.multi_contrast.find( mword ) != settings.multi_contrast.end() ) {
    conn = Conn::CONTRASTIEF;
  }
  else if ( settings.multi_compars.find( mword ) != settings.multi_compars.end() ) {
    conn = Conn::COMPARATIEF;
  }
  else if ( settings.multi_causals.find( mword ) != settings.multi_causals.end() ) {
    conn = Conn::CAUSAAL;
  }
  //  cerr << "multi-conn " << mword << " = " << conn << endl;
  return conn;
}

Situation::Type sentStats::checkMultiSituations( const string &mword ) {
  //  cerr << "check multi-sit '" << mword << "'" << endl;
  Situation::Type sit = Situation::NO_SIT;
  if ( settings.multi_time_sits.find( mword ) != settings.multi_time_sits.end() ) {
    sit = Situation::TIME_SIT;
  }
  else if ( settings.multi_space_sits.find( mword ) != settings.multi_space_sits.end() ) {
    sit = Situation::SPACE_SIT;
  }
  else if ( settings.multi_causal_sits.find( mword ) != settings.multi_causal_sits.end() ) {
    sit = Situation::CAUSAL_SIT;
  }
  else if ( settings.multi_emotion_sits.find( mword ) != settings.multi_emotion_sits.end() ) {
    sit = Situation::EMO_SIT;
  }
  //  cerr << "multi-sit " << mword << " = " << sit << endl;
  return sit;
}

void sentStats::resolveMultiWordIntensify() {
  size_t max_length_intensify = 5;
  for ( size_t i = 0; i < sv.size() - 1; ++i ) {
    string startword = sv[i]->text();
    string multiword = startword;

    for ( size_t j = 1; i + j < sv.size() && j < max_length_intensify; ++j ) {
      // Attach the next word to the expression
      multiword += " " + sv[i + j]->text();

      // Look for the expression in the list of intensifiers
      map<string, Intensify::Type>::const_iterator sit;
      sit = settings.intensify.find( multiword );
      // If found, update the counts, if not, continue
      if ( sit != settings.intensify.end() ) {
        intensCombiCnt += j + 1;
        intensCnt += j + 1;
        // Break and skip to the first word after this expression
        i += j;
        break;
      }
    }
  }
}

void sentStats::resolveMultiWordAfks() {
  if ( sv.size() > 1 ) {
    for ( size_t i = 0; i < sv.size() - 2; ++i ) {
      string word = sv[i]->text();
      string multiword2 = word + " " + sv[i + 1]->text();
      string multiword3 = multiword2 + " " + sv[i + 2]->text();
      Afk::Type at = Afk::NO_A;
      map<string, Afk::Type>::const_iterator sit
          = settings.afkos.find( multiword3 );
      if ( sit == settings.afkos.end() ) {
        sit = settings.afkos.find( multiword2 );
      }
      else {
        cerr << "FOUND a 3-word AFK: '" << multiword3 << "'" << endl;
      }
      if ( sit != settings.afkos.end() ) {
        cerr << "FOUND a 2-word AFK: '" << multiword2 << "'" << endl;
        at = sit->second;
      }
      if ( at != Afk::NO_A ) {
        ++afks[at];
      }
    }
    // don't forget the last 2 words
    string multiword2 = sv[sv.size() - 2]->text() + " " + sv[sv.size() - 1]->text();
    map<string, Afk::Type>::const_iterator sit
        = settings.afkos.find( multiword2 );
    if ( sit != settings.afkos.end() ) {
      cerr << "FOUND a 2-word AFK: '" << multiword2 << "'" << endl;
      Afk::Type at = sit->second;
      ++afks[at];
    }
  }
}

void sentStats::resolvePrepExpr() {
  if ( sv.size() > 2 ) {
    for ( size_t i = 0; i < sv.size() - 1; ++i ) {
      string word = sv[i]->ltext();
      string mw2 = word + " " + sv[i + 1]->ltext();
      if ( settings.vzexpr2.find( mw2 ) != settings.vzexpr2.end() ) {
        ++prepExprCnt;
        i += 1;
        continue;
      }
      if ( i < sv.size() - 2 ) {
        string mw3 = mw2 + " " + sv[i + 2]->ltext();
        if ( settings.vzexpr3.find( mw3 ) != settings.vzexpr3.end() ) {
          ++prepExprCnt;
          i += 2;
          continue;
        }
        if ( i < sv.size() - 3 ) {
          string mw4 = mw3 + " " + sv[i + 3]->ltext();
          if ( settings.vzexpr4.find( mw4 ) != settings.vzexpr4.end() ) {
            ++prepExprCnt;
            i += 3;
            continue;
          }
        }
      }
    }
  }
}

// Finds nodes of adverbials and reports counts
void sentStats::resolveAdverbials( xmlDoc *alpDoc ) {
  list<xmlNode *> nodes = getAdverbialNodes( alpDoc );
  vcModCnt = nodes.size();

  // Check for adverbials consisting of a single node that has the 'GENERAL' type.
  for ( auto &node : nodes ) {
    string word = TiCC::getAttribute( node, "word" );
    if ( word != "" ) {
      word = TiCC::lowercase( word );
      if ( checkAdverbType( word, CGN::BW ) == Adverb::GENERAL ) {
        vcModSingleCnt++;
      }
    }
  }
}

parStats::parStats( int index, folia::Paragraph *p ) :
    structStats( index, p, "par" ) {
  sentCnt = 0;
  vector<folia::Sentence *> sents = p->sentences();
  sentStats *prev = 0;
  for ( size_t i = 0; i < sents.size(); ++i ) {
    sentStats *ss = new sentStats( i, sents[i], prev );
    prev = ss;
    merge( ss );
  }
  calculate_MTLDs();

  word_freq_log = proportion( word_freq, contentCnt ).p;
  lemma_freq_log = proportion( lemma_freq, contentCnt ).p;
  word_freq_log_n = proportion( word_freq_n, contentCnt - nameCnt ).p;
  lemma_freq_log_n = proportion( lemma_freq_n, contentCnt - nameCnt ).p;

  word_freq_log_strict = proportion( word_freq_strict, contentStrictCnt ).p;
  lemma_freq_log_strict = proportion( lemma_freq_strict, contentStrictCnt ).p;
  word_freq_log_n_strict = proportion( word_freq_n_strict, contentStrictCnt - nameCnt ).p;
  lemma_freq_log_n_strict = proportion( lemma_freq_n_strict, contentStrictCnt - nameCnt ).p;
}

//#define DEBUG_DOL

void docStats::calculate_doc_overlap() {
  vector<const wordStats *> wv2 = collectWords();
  if ( wv2.size() < settings.overlapSize )
    return;
  vector<string> wordbuffer;
  vector<string> lemmabuffer;
  for ( vector<const wordStats *>::const_iterator it = wv2.begin();
        it != wv2.end();
        ++it ) {
    if ( ( *it )->wordProperty() == CGN::ISLET )
      continue;
    string l_word = ( *it )->ltext();
    string l_lemma = ( *it )->llemma();
    if ( wordbuffer.size() >= settings.overlapSize ) {
#ifdef DEBUG_DOL
      cerr << "Document overlap" << endl;
      cerr << "wordbuffer= " << wordbuffer << endl;
      cerr << "lemmabuffer= " << lemmabuffer << endl;
      cerr << "test overlap: << " << l_word << " " << l_lemma << endl;
#endif
      if ( ( *it )->isOverlapCandidate() ) {
#ifdef DEBUG_DOL
        int tmp = doc_word_overlapCnt;
#endif
        argument_overlap( l_word, wordbuffer, doc_word_overlapCnt );
#ifdef DEBUG_DOL
        if ( doc_word_overlapCnt > tmp ) {
          cerr << "word OVERLAP " << l_word << endl;
        }
#endif
#ifdef DEBUG_DOL
        tmp = doc_lemma_overlapCnt;
#endif
        argument_overlap( l_lemma, lemmabuffer, doc_lemma_overlapCnt );
#ifdef DEBUG_DOL
        if ( doc_lemma_overlapCnt > tmp ) {
          cerr << "lemma OVERLAP " << l_lemma << endl;
        }
#endif
      }
#ifdef DEBUG_DOL
      else {
        cerr << "geen kandidaat" << endl;
      }
#endif
      wordbuffer.erase( wordbuffer.begin() );
      lemmabuffer.erase( lemmabuffer.begin() );
    }
    wordbuffer.push_back( l_word );
    lemmabuffer.push_back( l_lemma );
  }
}

docStats::docStats( folia::Document *doc ) :
    structStats( 0, 0, "document" ),
    doc_word_overlapCnt( 0 ), doc_lemma_overlapCnt( 0 ) {
  sentCnt = 0;
  doc->declare( folia::AnnotationType::METRIC,
                "metricset",
                "annotator='tscan'" );
  doc->declare( folia::AnnotationType::POS,
                "tscan-set",
                "annotator='tscan'" );
  if ( !settings.style.empty() ) {
    doc->replaceStyle( "text/xsl", settings.style );
  }
  vector<folia::Paragraph *> pars = doc->paragraphs();
  if ( pars.size() > 0 )
    folia_node = pars[0]->parent();
  for ( size_t i = 0; i != pars.size(); ++i ) {
    parStats *ps = new parStats( i, pars[i] );
    merge( ps );
  }
  calculate_MTLDs();

  word_freq_log = proportion( word_freq, contentCnt ).p;
  lemma_freq_log = proportion( lemma_freq, contentCnt ).p;
  word_freq_log_n = proportion( word_freq_n, contentCnt - nameCnt ).p;
  lemma_freq_log_n = proportion( lemma_freq_n, contentCnt - nameCnt ).p;

  word_freq_log_strict = proportion( word_freq_strict, contentStrictCnt ).p;
  lemma_freq_log_strict = proportion( lemma_freq_strict, contentStrictCnt ).p;
  word_freq_log_n_strict = proportion( word_freq_n_strict, contentStrictCnt - nameCnt ).p;
  lemma_freq_log_n_strict = proportion( lemma_freq_n_strict, contentStrictCnt - nameCnt ).p;

  calculate_doc_overlap();

  rarity_index = rarity( settings.rarityLevel );
}

//#define DEBUG_FROG

folia::Document *getFrogResult( istream &is ) {
  string host = config.lookUp( "host", "frog" );
  string port = config.lookUp( "port", "frog" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ) {
    cerr << "failed to open Frog connection: " << host << ":" << port << endl;
    cerr << "Reason: " << client.getMessage() << endl;
    return 0;
  }
#ifdef DEBUG_FROG
  cerr << "start input loop" << endl;
#endif
  bool incomment = false;
  string line;
  while ( safe_getline( is, line ) ) {
#ifdef DEBUG_FROG
    cerr << "read: '" << line << "'" << endl;
#endif
    if ( line.length() > 2 ) {
      string start = line.substr( 0, 3 );
      if ( start == "###" )
        continue;
      else if ( start == "<<<" ) {
        if ( incomment ) {
          cerr << "Nested comment (<<<) not allowed!" << endl;
          return 0;
        }
        else {
          incomment = true;
        }
      }
      else if ( start == ">>>" ) {
        if ( !incomment ) {
          cerr << "end of comment (>>>) found without start." << endl;
          return 0;
        }
        else {
          incomment = false;
          continue;
        }
      }
    }
    if ( incomment )
      continue;
    if ( settings.sentencePerLine ) {
      client.write( line + "\n\n" );
    }
    else {
      client.write( line + "\n" );
    }
  }
  client.write( "\nEOT\n" );
  string result;
  string s;
  while ( client.read( s ) ) {
    if ( s == "READY" )
      break;
    result += s + "\n";
  }
#ifdef DEBUG_FROG
  cerr << "received data [" << result << "]" << endl;
#endif
  folia::Document *doc = 0;
  if ( !result.empty() && result.size() > 10 ) {
#ifdef DEBUG_FROG
    cerr << "start FoLiA parsing" << endl;
#endif
    doc = new folia::Document();
    try {
      doc->readFromString( result );
#ifdef DEBUG_FROG
      cerr << "finished" << endl;
#endif
    }
    catch ( std::exception &e ) {
      cerr << "FoLiaParsing failed:" << endl
           << e.what() << endl;
    }
  }
  return doc;
}

//#define DEBUG_ALPINO

xmlDoc *AlpinoServerParse( folia::Sentence *sent ) {
  string host = config.lookUp( "host", "alpino" );
  string port = config.lookUp( "port", "alpino" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ) {
    cerr << "failed to open Alpino connection: " << host << ":" << port << endl;
    cerr << "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
#ifdef DEBUG_ALPINO
  cerr << "start input loop" << endl;
#endif
  string txt = TiCC::UnicodeToUTF8( sent->toktext() );
  client.write( txt + "\n\n" );
  string result;
  string s;
  while ( client.read( s ) ) {
    result += s + "\n";
  }
#ifdef DEBUG_ALPINO
  cerr << "received data [" << result << "]" << endl;
#endif
  xmlDoc *doc = xmlReadMemory( result.c_str(), result.length(),
                               0, 0, XML_PARSE_NOBLANKS );
  string txtfile = workdir_name + "1.xml";
  xmlSaveFormatFileEnc( txtfile.c_str(), doc, "UTF8", 1 );
  return doc;
}

int main( int argc, char *argv[] ) {
  struct stat sbuf;
  pid_t pid = getpid();
  workdir_name = "/tmp/tscan-" + TiCC::toString( pid ) + "/";
  int res = stat( workdir_name.c_str(), &sbuf );
  if ( res == -1 || !S_ISDIR( sbuf.st_mode ) ) {
    res = mkdir( workdir_name.c_str(), S_IRWXU | S_IRWXG );
    if ( res ) {
      cerr << "problem creating working dir '" << workdir_name
           << "' : " << res << endl;
      exit( EXIT_FAILURE );
    }
  }
  cerr << "TScan " << VERSION << endl;
  cerr << "working dir " << workdir_name << endl;
  string shortOpt = "ht:o:Vn";
  string longOpt = "threads:,config:,skip:,version";
  TiCC::CL_Options opts( shortOpt, longOpt );
  try {
    opts.init( argc, argv );
  }
  catch ( TiCC::OptionError &e ) {
    cerr << e.what() << endl;
    usage();
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'h' ) || opts.extract( "help" ) ) {
    usage();
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'V' ) || opts.extract( "version" ) ) {
    exit( EXIT_SUCCESS );
  }

  string t_option;
  opts.extract( 't', t_option );
  vector<string> inputnames;
  if ( t_option.empty() ) {
    inputnames = opts.getMassOpts();
  }
  else {
    inputnames = TiCC::searchFiles( t_option );
  }

  if ( inputnames.size() == 0 ) {
    cerr << "no input file(s) found" << endl;
    exit( EXIT_FAILURE );
  }
  string o_option;
  if ( opts.extract( 'o', o_option ) ) {
    if ( inputnames.size() > 1 ) {
      cerr << "-o option not supported for multiple input files" << endl;
      exit( EXIT_FAILURE );
    }
  }

  string val;
  if ( opts.extract( "threads", val ) ) {
#ifdef HAVE_OPENMP
    int num = TiCC::stringTo<int>( val );
    if ( num < 1 || num > 4 ) {
      cerr << "wrong value for 'threads' option. (must be >=1 and <= 4 )"
           << endl;
      exit( EXIT_FAILURE );
    }
    else {
      omp_set_num_threads( num );
    }
#else
    cerr << "No OPEN_MP support available. 'threads' option ignored." << endl;
#endif
  }

  opts.extract( "config", configFile );
  if ( !configFile.empty() && config.fill( configFile ) ) {
    settings.init( config );
  }
  else {
    cerr << "invalid configuration" << endl;
    exit( EXIT_FAILURE );
  }
  if ( settings.showProblems ) {
    problemFile.open( "problems.log" );
    problemFile << "missing,word,lemma,voll_lemma" << endl;
  }
  if ( opts.extract( 'n' ) ) {
    settings.sentencePerLine = true;
  }
  if ( opts.extract( "skip", val ) ) {
    string skip = val;
    if ( skip.find_first_of( "wW" ) != string::npos ) {
      settings.doWopr = false;
    }
    if ( skip.find_first_of( "aA" ) != string::npos ) {
      settings.doAlpino = false;
      settings.doAlpinoServer = false;
    }
    if ( skip.find_first_of( "cC" ) != string::npos ) {
      settings.doXfiles = false;
    }
  };
  if ( !opts.empty() ) {
    cerr << "unsupported options in command: " << opts.toString() << endl;
    exit( EXIT_FAILURE );
  }

  if ( inputnames.size() > 1 ) {
    cerr << "processing " << inputnames.size() << " files." << endl;
  }
  for ( size_t i = 0; i < inputnames.size(); ++i ) {
    string inName = inputnames[i];
    string outName;
    if ( !o_option.empty() ) {
      // just 1 inputfile
      outName = o_option;
    }
    else {
      outName = inName + ".tscan.xml";
    }
    ifstream is( inName.c_str() );
    if ( !is ) {
      cerr << "failed to open file '" << inName << "'" << endl;
      if ( !o_option.empty() ) {
        // just 1 inputfile
        exit( EXIT_FAILURE );
      }
      continue;
    }
    else {
      cerr << "opened file " << inName << endl;
      folia::Document *doc = getFrogResult( is );
      if ( !doc ) {
        cerr << "big trouble: no FoLiA document created " << endl;
        if ( !o_option.empty() ) {
          // just 1 inputfile
          exit( EXIT_FAILURE );
        }
        continue;
      }
      else {
        docStats analyse( doc );
        analyse.addMetrics(); // add metrics info to doc
        doc->save( outName );
        if ( settings.doXfiles ) {
          analyse.toCSV( inName, DOC_CSV );
          analyse.toCSV( inName, PAR_CSV );
          analyse.toCSV( inName, SENT_CSV );
          analyse.toCSV( inName, WORD_CSV );
        }
        delete doc;
        cerr << "saved output in " << outName << endl;
      }
    }
  }
  exit( EXIT_SUCCESS );
}
