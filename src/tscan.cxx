/*
  $Id$
  $URL$

  Copyright (c) 1998 - 2013

  This file is part of tscan

  tscan is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  tscan is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:

  or send mail to:

*/

#include <string>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif
#include "timblserver/TimblServerAPI.h"
#include "libfolia/document.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/FileUtils.h"
#include "tscan/Alpino.h"
#include "tscan/decomp.h"

using namespace std;
using namespace TiCC;
using namespace folia;

TiCC::LogStream cur_log( "T-scan", StampMessage );

#define LOG (*Log(cur_log))
#define DBG (*Dbg(cur_log))

const double NA = -123456789;
const string frog_pos_set = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
const string frog_lemma_set = "http://ilk.uvt.nl/folia/sets/frog-mblem-nl";
const string frog_morph_set = "http://ilk.uvt.nl/folia/sets/frog-mbma-nl";
const string frog_ner_set = "http://ilk.uvt.nl/folia/sets/frog-ner-nl";

enum csvKind { DOC_CSV, PAR_CSV, SENT_CSV, WORD_CSV };

string configFile = "tscan.cfg";
Configuration config;
string workdir_name;

struct cf_data {
  long int count;
  double freq;
};

enum top_val { top1000, top2000, top3000, top5000, top10000, top20000, notFound  };

enum SemType { UNFOUND, ABSTRACT_NOUN, ABSTRACT_ADJ,
	       BROAD_NOUN, BROAD_ADJ, EMO_ADJ,
	       CONCRETE_NOUN, CONCRETE_ADJ,
	       CONCRETE_HUMAN_NOUN,
	       STATE, ACTION, PROCESS, WEIRD };

string toString( const SemType st ){
  switch ( st ){
  case UNFOUND:
    return "not_found";
    break;
  case CONCRETE_NOUN:
    return "concrete-noun";
    break;
  case CONCRETE_ADJ:
    return "concrete-adj";
    break;
  case CONCRETE_HUMAN_NOUN:
    return "concrete_human";
    break;
  case ABSTRACT_NOUN:
    return "abstract-noun";
    break;
  case ABSTRACT_ADJ:
    return "abstract-adj";
    break;
  case EMO_ADJ:
    return "emo-adj";
    break;
  case BROAD_NOUN:
    return "broad-noun";
    break;
  case BROAD_ADJ:
    return "broad-adj";
    break;
  case STATE:
    return "state";
    break;
  case ACTION:
    return "action";
    break;
  case PROCESS:
    return "process";
    break;
  case WEIRD:
    return "weird";
    break;
  default:
    return "invalid semtype value";
  }
}

ostream& operator<<( ostream& os, const SemType s ){
  os << toString( s );
  return os;
}

namespace CGN {
  enum Type { UNASS, ADJ, BW, LET, LID, N, SPEC, TSW, TW, VG, VNW, VZ, WW };

  Type toCGN( const string& s ){
    if ( s == "N" )
      return N;
    else if ( s == "ADJ" )
      return ADJ;
    else if ( s == "VNW" )
      return VNW;
    else if ( s == "LET" )
      return LET;
    else if ( s == "LID" )
      return LID;
    else if ( s == "SPEC" )
      return SPEC;
    else if ( s == "BW" )
      return BW;
    else if ( s == "WW" )
      return WW;
    else if ( s == "TW" )
      return TW;
    else if ( s == "VZ" )
      return VZ;
    else if ( s == "VG" )
      return VG;
    else
      return UNASS;
  }

  string toString( const Type& t ){
    switch ( t ){
    case N:
      return "N";
    case ADJ:
      return "ADJ";
    case VNW:
      return "VNW";
    case LET:
      return "LET";
    case LID:
      return "LID";
    case SPEC:
      return "SPEC";
    case BW:
      return "BW";
    case WW:
      return "WW";
    case TW:
      return "TW";
    case VZ:
      return "VZ";
    case VG:
      return "VG";
    default:
      return "UNASSIGNED";
    }
  }
}

ostream& operator<<( ostream& os, const CGN::Type t ){
  os << toString( t );
  return os;
}

enum NerProp { NONER, LOC_B, LOC_I, EVE_B, EVE_I, ORG_B, ORG_I,
	       MISC_B, MISC_I, PER_B, PER_I, PRO_B, PRO_I };

ostream& operator<<( ostream& os, const NerProp& n ){
  switch ( n ){
  case NONER:
    break;
  case LOC_B:
  case LOC_I:
    os << "LOC";
    break;
  case EVE_B:
  case EVE_I:
    os << "EVE";
    break;
  case ORG_B:
  case ORG_I:
    os << "ORG";
    break;
  case MISC_B:
  case MISC_I:
    os << "MISC";
    break;
  case PER_B:
  case PER_I:
    os << "PER";
    break;
  case PRO_B:
  case PRO_I:
    os << "PRO";
    break;
  };
  return os;
}

enum AfkType { NO_A, OVERHEID_A, ZORG_A, ONDERWIJS_A,
	       JURIDISCH_A, OVERIGE_A,
	       INTERNATIONAAL_A, MEDIA_A, GENERIEK_A };

AfkType stringTo( const string& s ){
  if ( s == "none" )
    return NO_A;
  if ( s == "Overheid_Politiek" )
    return OVERHEID_A;
  if ( s == "Zorg" )
    return ZORG_A;
  if ( s == "Onderwijs" )
    return ONDERWIJS_A;
  if ( s == "Juridisch" )
    return JURIDISCH_A;
  if ( s == "Overig" )
    return OVERIGE_A;
  if ( s == "Internationaal" )
    return INTERNATIONAAL_A;
  if ( s == "Media" )
    return MEDIA_A;
  if (s == "Generiek" )
    return GENERIEK_A;
  return NO_A;
}

string toString( const AfkType& afk ){
  switch ( afk ){
  case NO_A:
    return "none";
  case OVERHEID_A:
    return "Overheid_Politiek";
  case ZORG_A:
    return "Zorg";
  case INTERNATIONAAL_A:
    return "Internationaal";
  case MEDIA_A:
    return "Media";
  case ONDERWIJS_A:
    return "Onderwijs";
  case JURIDISCH_A:
    return "Juridisch";
  case OVERIGE_A:
    return "Overig";
  case GENERIEK_A:
    return "Generiek";
  default:
    return "PANIEK";
  };
}

ostream& operator<<( ostream& os, const AfkType& afk ){
  os << toString( afk );
  return os;
}


struct settingData {
  void init( const Configuration& );
  bool doAlpino;
  bool doAlpinoServer;
  bool doDecompound;
  bool doWopr;
  bool doLsa;
  bool doXfiles;
  string decompounderPath;
  string style;
  int rarityLevel;
  unsigned int overlapSize;
  double freq_clip;
  map <string, SemType> adj_sem;
  map <string, SemType> noun_sem;
  map <string, SemType> verb_sem;
  map <string, double> pol_lex;
  map<string, cf_data> staph_word_freq_lex;
  map<string, cf_data> word_freq_lex;
  map<string, cf_data> lemma_freq_lex;
  map<string, top_val> top_freq_lex;
  map<CGN::Type, set<string> > temporals1;
  set<string> multi_temporals;
  map<CGN::Type, set<string> > causals1;
  set<string> multi_causals;
  map<CGN::Type, set<string> > opsommers1;
  set<string> multi_opsommers;
  map<CGN::Type, set<string> > contrast1;
  set<string> multi_contrast;
  map<CGN::Type, set<string> > compars1;
  set<string> multi_compars;
  set<string> vzexpr2;
  set<string> vzexpr3;
  set<string> vzexpr4;
  map<string,AfkType> afkos;
};

settingData settings;

SemType classifySem( CGN::Type tag, const string& s ){
  if ( tag == CGN::N ){
    if ( s == "undefined" ||
	 s == "institut" )
      return UNFOUND;
    else if ( s == "concrother" ||
	      s == "substance" ||
	      s == "artefact" ||
	      s == "nonhuman" )
      return CONCRETE_NOUN;
    else if ( s == "human" )
      return CONCRETE_HUMAN_NOUN;
    else if ( s == "dynamic" ||
	      s == "nondynamic" )
      return ABSTRACT_NOUN;
    else // "place", "time" and "measure"
      return BROAD_NOUN;
  }
  else if ( tag == CGN::ADJ ){
    if ( s == "undefined" )
      return UNFOUND;
    else if ( s == "phyper" ||
	      s == "stuff" ||
	      s == "colour" )
      return CONCRETE_ADJ;
    else if ( s == "abstract" )
      return ABSTRACT_ADJ;
    else if ( s == "emomen" )
      return EMO_ADJ;
    else // "place" and "temp"
      return BROAD_ADJ;
  }
  else if ( tag == CGN::WW ){
    if ( s == "undefined" ){
      return UNFOUND;
    }
    else if ( s == "state" )
      return STATE;
    else if ( s == "action" )
      return ACTION;
    else if (s == "process" )
      return PROCESS;
    else
      return WEIRD;
  }
  else
    return UNFOUND;
}

bool fill( CGN::Type tag, map<string,SemType>& m, istream& is ){
  string line;
  while( getline( is, line ) ){
    vector<string> parts;
    int n = split_at( line, parts, "\t" ); // split at tab
    if ( n != 2 ){
      cerr << "skip line: " << line << " (expected 2 values, got "
	   << n << ")" << endl;
      continue;
    }
    vector<string> vals;
    n = split_at( parts[1], vals, "," ); // split at ,
    if ( n == 1 ){
      if ( vals[0] != "undefined" ){
	m[parts[0]] = classifySem( tag, vals[0] );
      }
    }
    else if ( n == 0 ){
      cerr << "skip line: " << line << " (expected some values, got none."
	   << endl;
      continue;
    }
    else {
      SemType topval = UNFOUND;
      if ( tag == CGN::WW ){
	map<SemType,int> stats;
	int max = 0;
	for ( size_t i=0; i< vals.size(); ++i ){
	  SemType val = classifySem( tag, vals[i] );
	  if ( ++stats[val] > max ){
	    max = stats[val];
	    topval = val;
	  }
	}
      }
      else { // N or ADJ
	map<SemType,int> stats;
	for ( size_t i=0; i< vals.size(); ++i ){
	  SemType val = classifySem( tag, vals[i] );
	  stats[val]++;
	}
	if ( tag == CGN::N ){
	  // possible values are (from high to low)
	  // CONCRETE_NOUN CONCRETE_HUMAN_NOUN ( strict )
	  // BROAD_NOUN ( broad )
	  // ABSTRACT_NOUN ( none )
	  // UNFOUND
	  SemType res = UNFOUND;
	  int concr_cnt = stats[CONCRETE_NOUN];
	  int hum_cnt = stats[CONCRETE_HUMAN_NOUN];
	  if ( concr_cnt == hum_cnt ){
	    if ( concr_cnt != 0 )
	      res = CONCRETE_NOUN;
	  }
	  else if ( concr_cnt > hum_cnt )
	    res = CONCRETE_NOUN;
	  else
	    res = CONCRETE_HUMAN_NOUN;
	  if ( res == UNFOUND ){
	    if ( stats[BROAD_NOUN] > 0 )
	      res = BROAD_NOUN;
	    else if ( stats[ABSTRACT_NOUN] > 0 ){
	      res = ABSTRACT_NOUN;
	    }
	  }
	  topval = res;
	}
	else {
	  // CGN::ADJ
	  // possible values are (from high to low)
	  // CONCRETE_ADJ (strict)
	  // BROAD_ADJ EMO_ADJ (broad)
	  // ABSTRACT_ADJ (none)
	  // UNFOUND
	  if ( stats[CONCRETE_ADJ] > 0 ){
	    topval = CONCRETE_ADJ;
	  }
	  else {
	    SemType res = UNFOUND;
	    int broad_cnt = stats[BROAD_ADJ];
	    int emo_cnt = stats[EMO_ADJ];
	    if ( broad_cnt == emo_cnt ){
	      if ( emo_cnt != 0 )
		res = EMO_ADJ;
	    }
	    else if ( broad_cnt > emo_cnt )
	      res = BROAD_ADJ;
	    else
	      res = EMO_ADJ;
	    if ( res == UNFOUND ){
	      if ( stats[ABSTRACT_ADJ] > 0 ){
		res = ABSTRACT_ADJ;
	      }
	    }
	    topval = res;
	  }
	}
      }
      if ( m.find(parts[0]) != m.end() ){
	cerr << "multiple entry '" << parts[0] << "' in " << tag << " lex" << endl;
      }
      if ( topval != UNFOUND ){
	// no use to store undefined values
	m[parts[0]] = topval;
      }
    }
  }
  return true;
}

bool fill( CGN::Type tag, map<string,SemType>& m, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill( tag, m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_freqlex( map<string,cf_data>& m, istream& is ){
  string line;
  while( getline( is, line ) ){
    vector<string> parts;
    size_t n = split_at( line, parts, "\t" ); // split at tabs
    if ( n != 4 ){
      cerr << "skip line: " << line << " (expected 4 values, got "
	   << n << ")" << endl;
      continue;
    }
    cf_data data;
    data.count = TiCC::stringTo<long int>( parts[1] );
    if ( data.count == 1 ){
      // we are done. Skip all singleton stuff
      return true;
    }
    data.freq = TiCC::stringTo<double>( parts[3] );
    if ( settings.freq_clip > 0 ){
      // skip low frequent word, when desired
      if ( data.freq > settings.freq_clip ){
	return true;
      }
    }
    m[parts[0]] = data;
  }
  return true;
}

bool fill_freqlex( map<string,cf_data>& m, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill_freqlex( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_topvals( map<string,top_val>& m, istream& is ){
  string line;
  int line_count = 0;
  top_val val = top2000;
  while( getline( is, line ) ){
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
    size_t n = split_at( line, parts, "\t" ); // split at tabs
    if ( n != 4 ){
      cerr << "skip line: " << line << " (expected 2 values, got "
	   << n << ")" << endl;
      continue;
    }
    m[parts[0]] = val;
  }
  return true;
}

bool fill_topvals( map<string,top_val>& m, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill_topvals( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_connectors( map<CGN::Type,set<string> >& c1,
		      set<string>& cM,
		      istream& is ){
  string line;
  while( getline( is, line ) ){
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR an entry of 1, 2 or 3 words seperated by a single space
    // like: 'dus' OR 'de facto'
    // OR the same followed by a TAB ('\t') and a CGN tag
    // like: 'maar   VG'
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = split_at( line, vec, "\t" );
    if ( n == 0 || n > 2 ){
      cerr << "skip line: " << line << " (expected 1 or 2 values, got "
	   << n << ")" << endl;
      continue;
    }
    CGN::Type tag = CGN::UNASS;
    if ( n == 2 ){
      tag = CGN::toCGN( vec[1] );
    }
    vector<string> dum;
    n = split_at( vec[0], dum, " " );
    if ( n < 1 || n > 3 ){
      cerr << "skip line: " << line
	   << " (expected 1, 2 or 3 values in the first part: " << vec[0]
	   << ", got " << n << ")" << endl;
      continue;
    }
    if ( n == 1 ){
      c1[tag].insert( vec[0] );
    }
    else if ( n > 1 && tag != CGN::UNASS ){
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

bool fill_connectors( map<CGN::Type, set<string> >& c1,
		      set<string>& cM,
		      const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill_connectors( c1, cM, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill_vzexpr( set<string>& vz2, set<string>& vz3, set<string>& vz4,
		  istream& is ){
  string line;
  while( getline( is, line ) ){
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR an entry of 2, 3 or 4 words seperated by whitespace
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = split_at_first_of( line, vec, " \t" );
    if ( n == 0 || n > 4 ){
      cerr << "skip line: " << line << " (expected 2, 3 or 4 values, got "
	   << n << ")" << endl;
      continue;
    }
    switch ( n ){
    case 2: {
      string line = vec[0] + " " + vec[1];
      vz2.insert( line );
    }
      break;
    case 3: {
      string line = vec[0] + " " + vec[1] + " " + vec[2];
      vz3.insert( line );
    }
      break;
    case 4: {
      string line = vec[0] + " " + vec[1] + " " + vec[2] + " " + vec[3];
      vz4.insert( line );
    }
      break;
    default:
      throw logic_error( "switch out of range" );
    }
  }
  return true;
}

bool fill_vzexpr( set<string>& vz2, set<string>& vz3, set<string>& vz4,
		  const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill_vzexpr( vz2, vz3, vz4 , is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill( map<string,AfkType>& afkos, istream& is ){
  string line;
  while( getline( is, line ) ){
    // a line is supposed to be :
    // a comment, starting with '#'
    // like: '# comment'
    // OR an entry of 2 words seperated by whitespace
    line = TiCC::trim( line );
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> vec;
    int n = split_at_first_of( line, vec, " \t" );
    if ( n < 2 ){
      cerr << "skip line: " << line << " (expected at least 2 values, got "
	   << n << ")" << endl;
      continue;
    }
    if ( n == 2 ){
      AfkType at = stringTo( vec[1] );
      if ( at != NO_A )
	afkos[vec[0]] = at;
    }
    else if ( n == 3 ){
      AfkType at = stringTo( vec[2] );
      if ( at != NO_A ){
	string s = vec[0] + " " + vec[1];
	afkos[s] = at;
      }
    }
    else if ( n == 4 ){
      AfkType at = stringTo( vec[3] );
      if ( at != NO_A ){
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

bool fill( map<string,AfkType>& afks, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill( afks , is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

void settingData::init( const Configuration& cf ){
  doXfiles = true;
  doAlpino = false;
  doAlpinoServer = false;
  string val = cf.lookUp( "useAlpinoServer" );
  if ( !val.empty() ){
    if ( !TiCC::stringTo( val, doAlpinoServer ) ){
      cerr << "invalid value for 'useAlpinoServer' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( !doAlpinoServer ){
    val = cf.lookUp( "useAlpino" );
    if( !TiCC::stringTo( val, doAlpino ) ){
      cerr << "invalid value for 'useAlpino' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  val = cf.lookUp( "useWopr" );
  doWopr = false;
  if ( !val.empty() ){
    if ( !TiCC::stringTo( val, doWopr ) ){
      cerr << "invalid value for 'useWopr' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  doLsa = true;
  val = cf.lookUp( "useLsa" );
  if ( !val.empty() ){
    if ( !TiCC::stringTo( val, doLsa ) ){
      cerr << "invalid value for 'useLsa' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  doDecompound = false;
  val = cf.lookUp( "decompounderPath" );
  if( !val.empty() ){
    decompounderPath = val + "/";
    doDecompound = true;
  }
  val = cf.lookUp( "styleSheet" );
  if( !val.empty() ){
    style = val;
  }
  val = cf.lookUp( "rarityLevel" );
  if ( val.empty() ){
    rarityLevel = 10;
  }
  else if ( !TiCC::stringTo( val, rarityLevel ) ){
    cerr << "invalid value for 'rarityLevel' in config file" << endl;
  }
  val = cf.lookUp( "overlapSize" );
  if ( val.empty() ){
    overlapSize = 50;
  }
  else if ( !TiCC::stringTo( val, overlapSize ) ){
    cerr << "invalid value for 'overlapSize' in config file" << endl;
    exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "frequencyClip" );
  if ( val.empty() ){
    freq_clip = 90;
  }
  else if ( !TiCC::stringTo( val, freq_clip )
	    || (freq_clip < 0) || (freq_clip > 100) ){
    cerr << "invalid value for 'frequencyClip' in config file" << endl;
    exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "adj_semtypes" );
  if ( !val.empty() ){
    if ( !fill( CGN::ADJ, adj_sem, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "noun_semtypes" );
  if ( !val.empty() ){
    if ( !fill( CGN::N, noun_sem, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "verb_semtypes" );
  if ( !val.empty() ){
    if ( !fill( CGN::WW, verb_sem, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "staph_word_freq_lex" );
  if ( !val.empty() ){
    if ( !fill_freqlex( staph_word_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "word_freq_lex" );
  if ( !val.empty() ){
    if ( !fill_freqlex( word_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "lemma_freq_lex" );
  if ( !val.empty() ){
    if ( !fill_freqlex( lemma_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "top_freq_lex" );
  if ( !val.empty() ){
    if ( !fill_topvals( top_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "temporals" );
  if ( !val.empty() ){
    if ( !fill_connectors( temporals1, multi_temporals, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "opsommers" );
  if ( !val.empty() ){
    if ( !fill_connectors( opsommers1, multi_opsommers, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "contrast" );
  if ( !val.empty() ){
    if ( !fill_connectors( contrast1, multi_contrast, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "compars" );
  if ( !val.empty() ){
    if ( !fill_connectors( compars1, multi_compars, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "causals" );
  if ( !val.empty() ){
    if ( !fill_connectors( causals1, multi_causals, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "voorzetselexpr" );
  if ( !val.empty() ){
    if ( !fill_vzexpr( vzexpr2, vzexpr3, vzexpr4, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "afkortingen" );
  if ( !val.empty() ){
    if ( !fill( afkos, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
}

inline void usage(){
  cerr << "usage:  tscan [options] -t <inputfile> " << endl;
  cerr << "options: " << endl;
  cerr << "\t-o <file> store XML in file " << endl;
  cerr << "\t--config=<file> read configuration from 'file' " << endl;
  cerr << "\t-V or --version show version " << endl;
  cerr << "\t-D <value> set debug level " << endl;
  cerr << "\t--skip=[acdlw]    Skip Alpino (a), CSV output (c), Decompounder (d), Lsa (l) or Wopr (w).\n";
  cerr << "\t-t <file> process the 'file'." << endl;
  cerr << "\t-e <exp> process the files using the (possibly wildcarded) expression 'exp'." << endl;
  cerr << "\t\t Wildcards must be escaped!" << endl;
  cerr << "\t-d <dir> use path 'dir' as a prefix for -e ." << endl;
  cerr << "\t\t example: -e \\*.txt" << endl;
  cerr << "\t\t          -e 'b*.txt'" << endl;
  cerr << "\t\t          -d tests -e '*.example'" << endl;
  cerr << endl;
}

template <class M>
void aggregate( M& out, const M& in ){
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

void aggregate( multimap<DD_type,int>& out,
		const multimap<DD_type,int>& in ){
  multimap<DD_type,int>::const_iterator ii = in.begin();
  while ( ii != in.end() ){
    out.insert( make_pair(ii->first, ii->second ) );
    ++ii;
  }
}

struct ratio {
  ratio( double d1, double d2 ){
    if ( d1 < 0 || d2 == 0 ||
	 d1 == NA || d2 == NA )
      r = NA;
    else
      r = d1/d2;
  };
  double r;
};

ostream& operator<<( ostream& os, const ratio& r ){
  if ( r.r == NA )
    os << "NA";
  else
    os << r.r;
  return os;
}

struct density {
  density( double d1, double d2 ){
    if ( d1 < 0 || d2 == 0 || d1 == NA || d2 == NA )
      d = NA;
    else
      d = (d1/d2) * 1000;
  };
  double d;
};

ostream& operator<<( ostream& os, const density& d ){
  if ( d.d == NA )
    os << "NA";
  else
    os << d.d;
  return os;
}

enum WordProp { ISNAME, ISLET,
		ISVD, ISOD, ISINF, ISPVTGW, ISPVVERL, ISSUBJ,
		ISPPRON1, ISPPRON2, ISPPRON3, ISAANW,
		JUSTAWORD, NOTAWORD };

string toString( const WordProp& w ){
  switch ( w ){
  case ISNAME:
    return "naam";
  case ISLET:
    return "punctuatie";
  case ISVD:
    return "voltooid_deelw";
  case ISOD:
    return "onvoltooid_deelw";
  case ISINF:
    return "infinitief";
  case ISPVTGW:
    return "tegenwoordige_tijd";
  case ISPVVERL:
    return "verleden_tijd";
  case ISSUBJ:
    return "subjonctive";
  case ISPPRON1:
    return "voornaamwoord_1";
  case ISPPRON2:
    return "voornaamwoord_2";
  case ISPPRON3:
    return "voornaamwoord_3";
  case  ISAANW:
    return "aanwijzend";
  case  NOTAWORD:
    return "GEEN_WOORD";
  default:
    return "default";
  }
}

ostream& operator<<( ostream& os, const WordProp& p ){
  os << toString( p );
  return os;
}

enum ConnType { NOCONN, TEMPOREEL, OPSOMMEND, CONTRASTIEF, COMPARATIEF, CAUSAAL };

string toString( const ConnType& c ){
  if ( c == NOCONN )
    return "Not_a_connector";
  else if ( c == TEMPOREEL )
    return "temporeel";
  else if ( c == OPSOMMEND )
    return "opsommend";
  else if ( c == CONTRASTIEF )
    return "contrastief";
  else if ( c == COMPARATIEF )
    return "comparatief";
  else if ( c == CAUSAAL )
    return "causaal";
  else
    throw "no translation for ConnType";
}

struct sentStats;
struct wordStats;

struct basicStats {
  basicStats( FoliaElement *el, const string& cat ):
    folia_node( el ),
    category( cat ),
    charCnt(0),charCntExNames(0),
    morphCnt(0), morphCntExNames(0),
    lsa_opv(0),
    lsa_ctx(0)
  { if ( el ){
      id = el->id();
    }
    else {
      id = "document";
    }
  };
  virtual ~basicStats(){};
  virtual void CSVheader( ostream&, const string& ) const = 0;
  virtual void wordDifficultiesHeader( ostream& ) const = 0;
  virtual void wordDifficultiesToCSV( ostream& ) const = 0;
  virtual void sentDifficultiesHeader( ostream& ) const = 0;
  virtual void sentDifficultiesToCSV( ostream& ) const = 0;
  virtual void infoHeader( ostream& ) const = 0;
  virtual void informationDensityToCSV( ostream& ) const = 0;
  virtual void coherenceHeader( ostream& ) const = 0;
  virtual void coherenceToCSV( ostream& ) const = 0;
  virtual void concreetHeader( ostream& ) const = 0;
  virtual void concreetToCSV( ostream& ) const = 0;
  virtual void persoonlijkheidHeader( ostream& ) const = 0;
  virtual void persoonlijkheidToCSV( ostream& ) const = 0;
  virtual void wordSortHeader( ostream& ) const = 0;
  virtual void wordSortToCSV( ostream& ) const = 0;
  virtual void miscToCSV( ostream& ) const = 0;
  virtual void miscHeader( ostream& ) const = 0;
  virtual string rarity( int ) const { return "NA"; };
  virtual void toCSV( ostream& ) const =0;
  virtual void addMetrics( ) const = 0;
  virtual string text() const { return ""; };
  virtual string ltext() const { return ""; };
  virtual string llemma() const { return ""; };
  virtual CGN::Type postag() const { return CGN::UNASS; };
  virtual WordProp wordProperty() const { return NOTAWORD; };
  virtual ConnType getConnType() const { return NOCONN; };
  virtual void setConnType( ConnType ){
    throw logic_error("settConnType() only valid for words" );
  };
  virtual void setMultiConn(){
    throw logic_error("settMultiConn() only valid for words" );
  };
  virtual vector<const wordStats*> collectWords() const = 0;

  void setLSAsuc( double d ){ lsa_opv = d; };
  void setLSAcontext( double d ){ lsa_ctx = d; };
  FoliaElement *folia_node;
  string category;
  string id;
  int charCnt;
  int charCntExNames;
  int morphCnt;
  int morphCntExNames;
  double lsa_opv;
  double lsa_ctx;
  vector<basicStats *> sv;
};

struct wordStats : public basicStats {
  wordStats( Word *, const xmlNode *, const set<size_t>& );
  void CSVheader( ostream&, const string& ) const;
  void wordDifficultiesHeader( ostream& ) const;
  void wordDifficultiesToCSV( ostream& ) const;
  void sentDifficultiesHeader( ostream& ) const {};
  void sentDifficultiesToCSV( ostream& ) const {};
  void infoHeader( ostream& ) const {};
  void informationDensityToCSV( ostream& ) const {};
  void coherenceHeader( ostream& ) const;
  void coherenceToCSV( ostream& ) const;
  void concreetHeader( ostream& ) const;
  void concreetToCSV( ostream& ) const;
  void persoonlijkheidHeader( ostream& ) const;
  void persoonlijkheidToCSV( ostream& ) const;
  void wordSortHeader( ostream& ) const;
  void wordSortToCSV( ostream& ) const;
  void miscHeader( ostream& os ) const;
  void miscToCSV( ostream& ) const;
  void toCSV( ostream& ) const;
  string text() const { return word; };
  string ltext() const { return l_word; };
  string llemma() const { return l_lemma; };
  CGN::Type postag() const { return tag; };
  ConnType getConnType() const { return connType; };
  void setConnType( ConnType t ){ connType = t; };
  void setMultiConn(){ isMultiConn = true; };
  void addMetrics( ) const;
  bool checkContent() const;
  ConnType checkConnective( const xmlNode * ) const;
  ConnType check_small_connector( const xmlNode * ) const;
  bool checkNominal( const xmlNode * ) const;
  WordProp checkProps( const PosAnnotation* );
  WordProp wordProperty() const { return prop; };
  SemType checkSemProps( ) const;
  AfkType checkAfk( ) const;
  bool checkPropNeg() const;
  bool checkMorphNeg() const;
  void staphFreqLookup();
  void topFreqLookup();
  void freqLookup();
  void getSentenceOverlap( const vector<string>&, const vector<string>& );
  bool isOverlapCandidate() const;
  vector<const wordStats*> collectWords() const;
  string word;
  string l_word;
  string pos;
  CGN::Type tag;
  string lemma;
  string l_lemma;
  WWform wwform;
  bool isPersRef;
  bool isPronRef;
  bool archaic;
  bool isContent;
  bool isNominal;
  bool isOnder;
  bool isImperative;
  bool isBetr;
  bool isPropNeg;
  bool isMorphNeg;
  NerProp nerProp;
  ConnType connType;
  bool isMultiConn;
  bool f50;
  bool f65;
  bool f77;
  bool f80;
  int compPartCnt;
  top_val top_freq;
  int word_freq;
  int lemma_freq;
  int wordOverlapCnt;
  int lemmaOverlapCnt;
  double word_freq_log;
  double lemma_freq_log;
  double logprob10;
  WordProp prop;
  SemType sem_type;
  vector<string> morphemes;
  multimap<DD_type,int> distances;
  AfkType afkType;
};

vector<const wordStats*> wordStats::collectWords() const {
  vector<const wordStats*> result;
  result.push_back( this );
  return result;
}

ConnType wordStats::check_small_connector( const xmlNode *alpWord ) const {
  if ( alpWord == 0 ){
    return OPSOMMEND;
  }
  else {
    if ( isSmallCnj( alpWord ) )
      return NOCONN;
    else
      return OPSOMMEND;
  }
}

ConnType wordStats::checkConnective( const xmlNode *alpWord ) const {
  if ( tag != CGN::VG && tag != CGN::VZ && tag != CGN::BW )
    return NOCONN;
  if ( settings.temporals1[tag].find( l_word )
       != settings.temporals1[tag].end() ){
    return TEMPOREEL;
  }
  else if ( settings.temporals1[CGN::UNASS].find( l_word )
	    != settings.temporals1[CGN::UNASS].end() ){
    return TEMPOREEL;
  }
  else if ( settings.opsommers1[tag].find( l_word )
	    != settings.opsommers1[tag].end() ){
    if ( l_word == "en" || l_word == "of" ){
      return check_small_connector( alpWord );
    }
    return OPSOMMEND;
  }
  else if ( settings.opsommers1[CGN::UNASS].find( l_word )
	    != settings.opsommers1[CGN::UNASS].end() ){
    if ( l_word == "en" || l_word == "of" ){
      return check_small_connector( alpWord );
    }
    return OPSOMMEND;
  }
  else if ( settings.contrast1[tag].find( l_word )
	    != settings.contrast1[tag].end() )
    return CONTRASTIEF;
  else if ( settings.contrast1[CGN::UNASS].find( l_word )
	    != settings.contrast1[CGN::UNASS].end() )
    return CONTRASTIEF;
  else if ( settings.compars1[tag].find( l_word )
	    != settings.compars1[tag].end() )
    return COMPARATIEF;
  else if ( settings.compars1[CGN::UNASS].find( l_word )
	    != settings.compars1[CGN::UNASS].end() )
    return COMPARATIEF;
  else if ( settings.causals1[tag].find( l_word )
	    != settings.causals1[tag].end() )
    return CAUSAAL;
  else if ( settings.causals1[CGN::UNASS].find( l_word )
	    != settings.causals1[CGN::UNASS].end() )
    return CAUSAAL;
  return NOCONN;
}

bool wordStats::checkContent() const {
  if ( tag == CGN::WW ){
    if ( wwform == HEAD_VERB ){
      return true;
    }
  }
  else {
    return ( prop == ISNAME
	     || tag == CGN::N || tag == CGN::BW || tag == CGN::ADJ );
  }
  return false;
}

bool match_tail( const string& word, const string& tail ){
  //  cerr << "match tail " << word << " tail=" << tail << endl;
  if ( tail.size() > word.size() ){
    //    cerr << "match tail failed" << endl;
    return false;
  }
  string::const_reverse_iterator wir = word.rbegin();
  string::const_reverse_iterator tir = tail.rbegin();
  while ( tir != tail.rend() ){
    if ( *tir != *wir ){
      //      cerr << "match tail failed" << endl;
      return false;
    }
    ++tir;
    ++wir;
  }
  //  cerr << "match tail succes" << endl;
  return true;
}

//#define DEBUG_NOMINAL

bool wordStats::checkNominal( const xmlNode *alpWord ) const {
  static string morphList[] = { "ing", "sel", "nis", "enis", "heid", "te",
				"schap", "dom", "sie", "ie", "iek", "iteit",
				"isme", "age", "atie", "esse",	"name" };
  static set<string> morphs( morphList,
			     morphList + sizeof(morphList)/sizeof(string) );
#ifdef DEBUG_NOMINAL
  cerr << "check Nominal " << word << " tag=" << tag << " morphemes=" << morphemes << endl;
#endif
  if ( tag == CGN::N && morphemes.size() > 1 ){
    // morphemes.size() > 1 check mijdt false hits voor "dom", "schap".
    string last_morph = morphemes[morphemes.size()-1];
#ifdef DEBUG_NOMINAL
    cerr << "check morphemes, last= " << last_morph << endl;
#endif
    if ( last_morph == "en" || last_morph == "s" || last_morph == "n" ) {
      last_morph =  morphemes[morphemes.size()-2];
    }
#ifdef DEBUG_NOMINAL
    cerr << "check morphemes, last= " << last_morph << endl;
#endif
    if ( morphs.find( last_morph ) != morphs.end() ){
#ifdef DEBUG_NOMINAL
      cerr << "check Nominal, MATCHED morpheme " << last_morph << endl;
#endif
      return true;
    }

    if ( last_morph.size() > 4 ){
      // avoid false positives for words like oase, base, rose, fase
      bool matched = match_tail( last_morph, "ose" ) ||
	match_tail( last_morph, "ase" ) ||
	match_tail( last_morph, "ese" ) ||
	match_tail( last_morph, "isme" ) ||
	match_tail( last_morph, "sie" ) ||
	match_tail( last_morph, "tie" );
      if ( matched ){
#ifdef DEBUG_NOMINAL
	cerr << "check Nominal, MATCHED tail of morpheme " << last_morph << endl;
#endif
	return true;
      }
    }
  }
  if ( morphemes.size() < 2 && word.size() > 4 ){
    bool matched = match_tail( word, "ose" ) ||
      match_tail( word, "ase" ) ||
      match_tail( word, "ese" ) ||
      match_tail( word, "isme" ) ||
      match_tail( word, "sie" ) ||
      match_tail( word, "tie" );
    if (matched ){
#ifdef DEBUG_NOMINAL
      cerr << "check Nominal, MATCHED tail " <<  word << endl;
#endif
      return true;
    }
  }

  string pos = getAttribute( alpWord, "pos" );
  if ( pos == "verb" ){
    // Alpino heeft de voor dit feature prettige eigenschap dat het nogal
    // eens nominalisaties wil taggen als werkwoord dat onder een
    // NP knoop hangt
    alpWord = alpWord->parent;
    string cat = getAttribute( alpWord, "cat" );
    if ( cat == "np" ){
#ifdef DEBUG_NOMINAL
      cerr << "Alpino says NOMINAL!" << endl;
#endif
      return true;
    }
  }
#ifdef DEBUG_NOMINAL
  cerr << "check Nominal. NO WAY" << endl;
#endif
  return false;
}

WordProp wordStats::checkProps( const PosAnnotation* pa ) {
  if ( tag == CGN::LET )
    prop = ISLET;
  else if ( tag == CGN::SPEC && pos.find("eigen") != string::npos )
    prop = ISNAME;
  else if ( tag == CGN::WW ){
    string wvorm = pa->feat("wvorm");
    if ( wvorm == "inf" )
      prop = ISINF;
    else if ( wvorm == "vd" )
      prop = ISVD;
    else if ( wvorm == "od" )
      prop = ISOD;
    else if ( wvorm == "pv" ){
      string tijd = pa->feat("pvtijd");
      if ( tijd == "tgw" )
	prop = ISPVTGW;
      else if ( tijd == "verl" )
	prop = ISPVVERL;
      else if ( tijd == "conj" )
	prop = ISSUBJ;
      else {
	cerr << "PANIEK: een onverwachte ww tijd: " << tijd << endl;
	exit(3);
      }
    }
    else {
      cerr << "PANIEK: een onverwachte ww vorm: " << wvorm << endl;
      exit(3);
    }
  }
  else if ( tag == CGN::VNW ){
    string vwtype = pa->feat("vwtype");
    isBetr = vwtype == "betr";
    if ( l_word != "men"
	 && l_word != "er"
	 && l_word != "het" ){
      string cas = pa->feat("naamval");
      archaic = ( cas == "gen" || cas == "dat" );
      if ( vwtype == "pers" || vwtype == "refl"
	   || vwtype == "pr" || vwtype == "bez" ) {
	string persoon = pa->feat("persoon");
	if ( !persoon.empty() ){
	  if ( persoon[0] == '1' )
	    prop = ISPPRON1;
	  else if ( persoon[0] == '2' )
	    prop = ISPPRON2;
	  else if ( persoon[0] == '3' ){
	    prop = ISPPRON3;
	    isPronRef = ( vwtype == "pers" || vwtype == "bez" );
	  }
	  else {
	    cerr << "PANIEK: een onverwachte PRONOUN persoon : " << persoon
		 << " for word " << word << endl;
	    exit(3);
	  }
	}
      }
      else if ( vwtype == "aanw" ){
	prop = ISAANW;
	isPronRef = true;
      }
    }
  }
  else if ( tag == CGN::LID ) {
    string cas = pa->feat("naamval");
    archaic = ( cas == "gen" || cas == "dat" );
  }
  else if ( tag == CGN::VG ) {
    string cp = pa->feat("conjtype");
    isOnder = cp == "onder";
  }
  return prop;
}

SemType wordStats::checkSemProps( ) const {
  if ( tag == CGN::N ){
    SemType sem2 = UNFOUND;
    map<string,SemType>::const_iterator sit = settings.noun_sem.find( lemma );
    if ( sit != settings.noun_sem.end() ){
      sem2 = sit->second;
    }
    return sem2;
  }
  else if ( prop == ISNAME ){
    // Names are te be looked up in the Noun list too
    SemType sem2 = UNFOUND;
    map<string,SemType>::const_iterator sit = settings.noun_sem.find( lemma );
    if ( sit != settings.noun_sem.end() ){
      sem2 = sit->second;
    }
    return sem2;
  }
  else if ( tag == CGN::ADJ ) {
    SemType sem2 = UNFOUND;
    map<string,SemType>::const_iterator sit = settings.adj_sem.find( lemma );
    if ( sit == settings.adj_sem.end() ){
      // lemma not found. maybe the whole word?
      sit = settings.adj_sem.find( word );
    }
    if ( sit != settings.adj_sem.end() ){
      sem2 = sit->second;
    }
    return sem2;
  }
  else if ( tag == CGN::WW ) {
    SemType sem2 = UNFOUND;
    map<string,SemType>::const_iterator sit = settings.verb_sem.find( lemma );
    if ( sit != settings.verb_sem.end() ){
      sem2 = sit->second;
    }
    return sem2;
  }
  return UNFOUND;
}

AfkType wordStats::checkAfk() const {
  if ( tag == CGN::N || tag == CGN::SPEC) {
    map<string,AfkType>::const_iterator sit = settings.afkos.find( word );
    if ( sit != settings.afkos.end() ){
      return sit->second;
    }
  }
  return NO_A;
}

void wordStats::topFreqLookup(){
  map<string,top_val>::const_iterator it = settings.top_freq_lex.find( l_word );
  top_freq = notFound;
  if ( it != settings.top_freq_lex.end() ){
    top_freq = it->second;
  }
}

void wordStats::freqLookup(){
  map<string,cf_data>::const_iterator it = settings.word_freq_lex.find( l_word );
  if ( it != settings.word_freq_lex.end() ){
    word_freq = it->second.count;
    word_freq_log = log10(word_freq);
  }
  else {
    word_freq = 1;
    word_freq_log = 0;
  }
  it = settings.lemma_freq_lex.find( l_lemma );
  if ( it != settings.lemma_freq_lex.end() ){
    lemma_freq = it->second.count;
    lemma_freq_log = log10(lemma_freq);
  }
  else {
    lemma_freq = 1;
    lemma_freq_log = 0;
  }
}

void wordStats::staphFreqLookup(){
  map<string,cf_data>::const_iterator it = settings.staph_word_freq_lex.find( l_word );
  if ( it != settings.staph_word_freq_lex.end() ){
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

const string negativesA[] = { "geeneens", "geenszins", "kwijt", "nergens",
			      "niet", "niets", "nooit", "allerminst",
			      "allesbehalve", "amper", "behalve", "contra",
			      "evenmin", "geen", "generlei", "nauwelijks",
			      "niemand", "niemendal", "nihil", "niks", "nimmer",
			      "nimmermeer", "noch", "ongeacht", "slechts",
			      "tenzij", "ternauwernood", "uitgezonderd",
			      "weinig", "zelden", "zeldzaam", "zonder" };
static set<string> negatives = set<string>( negativesA,
					    negativesA + sizeof(negativesA)/sizeof(string) );

const string neg_longA[] = { "afgezien van",
			     "zomin als",
			     "met uitzondering van"};
static set<string> negatives_long = set<string>( neg_longA,
						 neg_longA + sizeof(neg_longA)/sizeof(string) );

const string negmorphA[] = { "mis", "de", "non", "on" };
static set<string> negmorphs = set<string>( negmorphA,
					    negmorphA + sizeof(negmorphA)/sizeof(string) );

const string negminusA[] = { "mis-", "non-", "niet-", "anti-",
			     "ex-", "on-", "oud-" };
static vector<string> negminus = vector<string>( negminusA,
						 negminusA + sizeof(negminusA)/sizeof(string) );

bool wordStats::checkPropNeg() const {
  if ( negatives.find( l_word ) != negatives.end() ){
    return true;
  }
  else if ( tag == CGN::BW &&
	    ( l_word == "moeilijk" || l_word == "weg" ) ){
    // "moeilijk" en "weg" mochten kennelijk alleen als bijwoord worden
    // meegeteld (in het geval van weg natuurlijk duidelijk ivm "de weg")
    return true;
  }
  return false;
}

bool wordStats::checkMorphNeg() const {
  string m1 = morphemes[0];
  string m2;
  if ( morphemes.size() > 1 ){
    m2 = morphemes[1];
  }
  if ( negmorphs.find( m1 ) != negmorphs.end() && m2 != "en" && !m2.empty() ){
    // dit om gevallen als "nonnen" niet mee te rekenen
    return true;
  }
  else {
    for ( size_t i=0; i < negminus.size(); ++i ){
      if ( word.find( negminus[i] ) != string::npos )
	return true;
    }
  }
  return false;
}

void argument_overlap( const string w_or_l,
		       const vector<string>& buffer,
		       int& arg_overlap_cnt ){
  // calculate the overlap of the Word or Lemma with the buffer
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

wordStats::wordStats( Word *w, const xmlNode *alpWord, const set<size_t>& puncts ):
  basicStats( w, "word" ), wwform(::NO_VERB),
  isPersRef(false), isPronRef(false),
  archaic(false), isContent(false), isNominal(false),isOnder(false), isImperative(false),
  isBetr(false), isPropNeg(false), isMorphNeg(false),
  nerProp(NONER), connType(NOCONN), isMultiConn(false),
  f50(false), f65(false), f77(false), f80(false),  compPartCnt(0),
  top_freq(notFound), word_freq(0), lemma_freq(0),
  wordOverlapCnt(0), lemmaOverlapCnt(0),
  word_freq_log(NA), lemma_freq_log(NA),
  logprob10(NA), prop(JUSTAWORD), sem_type(UNFOUND),afkType(NO_A)
{
  UnicodeString us = w->text();
  charCnt = us.length();
  word = UnicodeToUTF8( us );
  l_word = UnicodeToUTF8( us.toLower() );

  vector<PosAnnotation*> posV = w->select<PosAnnotation>(frog_pos_set);
  if ( posV.size() != 1 )
    throw ValueError( "word doesn't have Frog POS tag info" );
  PosAnnotation *pa = posV[0];
  pos = pa->cls();
  tag = CGN::toCGN( pa->feat("head") );
  lemma = w->lemma( frog_lemma_set );
  us = UTF8ToUnicode( lemma );
  l_lemma = UnicodeToUTF8( us.toLower() );

  prop = checkProps( pa );
  if ( alpWord ){
    distances = getDependencyDist( alpWord, puncts);
    if ( tag == CGN::WW ){
      wwform = classifyVerb( alpWord, lemma );
      if ( (prop == ISPVTGW || prop == ISPVVERL) &&
	   wwform != PASSIVE_VERB ){
	isImperative = checkImp( alpWord );
      }
    }
  }
  isContent = checkContent();
  if ( prop != ISLET ){
    size_t max = 0;
    vector<Morpheme*> morphemeV;
    vector<MorphologyLayer*> ml = w->annotations<MorphologyLayer>();
    for ( size_t q=0; q < ml.size(); ++q ){
      vector<Morpheme*> m = ml[q]->select<Morpheme>( frog_morph_set );
      if ( m.size() > max ){
	// a hack: we assume the longest morpheme list to
	// be the best choice.
	morphemeV = m;
	max = m.size();
      }
    }
    for ( size_t q=0; q < morphemeV.size(); ++q ){
      morphemes.push_back( morphemeV[q]->str() );
    }
    isPropNeg = checkPropNeg();
    isMorphNeg = checkMorphNeg();
    connType = checkConnective( alpWord );
    //    cerr << "checkConn " << word << " = " << connType << endl;
    morphCnt = max;
    if ( prop != ISNAME ){
      charCntExNames = charCnt;
      morphCntExNames = max;
    }
    sem_type = checkSemProps();
    if ( sem_type == CONCRETE_HUMAN_NOUN ||
	 prop == ISNAME ||
	 prop == ISPPRON1 || prop == ISPPRON2 || prop == ISPPRON3 ){
      isPersRef = true;
    }
    afkType = checkAfk();
    if ( alpWord )
      isNominal = checkNominal( alpWord );
    topFreqLookup();
    staphFreqLookup();
    if ( isContent ){
      freqLookup();
    }
    if ( settings.doDecompound )
      compPartCnt = runDecompoundWord( word, workdir_name,
				       settings.decompounderPath );
  }
}

void addOneMetric( Document *doc, FoliaElement *parent,
		   const string& cls, const string& val ){
  MetricAnnotation *m = new MetricAnnotation( doc,
					      "class='" + cls + "', value='"
					      + val + "'" );
  parent->append( m );
}

void fill_word_lemma_buffers( const sentStats*,
			      vector<string>&, vector<string>& );

//#define DEBUG_OL

bool wordStats::isOverlapCandidate() const {
  if ( ( tag == CGN::VNW && prop != ISAANW ) ||
       ( tag == CGN::N ) ||
       ( prop == ISNAME ) ||
       ( tag == CGN::WW && wwform == HEAD_VERB ) ){
    return true;
  }
  else {
#ifdef DEBUG_OL
    if ( tag == CGN::WW ){
      cerr << "WW overlapcandidate REJECTED " << toString(wwform) << " " << word << endl;
    }
    else if ( tag == CGN::VNW ){
      cerr << "VNW overlapcandidate REJECTED " << toString(prop) << " " << word << endl;
    }
#endif
    return false;
  }
}

void wordStats::getSentenceOverlap( const vector<string>& wordbuffer,
				    const vector<string>& lemmabuffer ){
  if ( isOverlapCandidate() ){
    // get the words and lemmas' of the previous sentence
#ifdef DEBUG_OL
    cerr << "call word sentenceOverlap, word = " << l_word;
    int tmp2 = wordOverlapCnt;
#endif
    argument_overlap( l_word, wordbuffer, wordOverlapCnt );
#ifdef DEBUG_OL
    if ( tmp2 != wordOverlapCnt ){
      cerr << " OVERLAPPED " << endl;
    }
    else
      cerr << endl;
    cerr << "call lemma sentenceOverlap, lemma= " << l_lemma;
    tmp2 = lemmaOverlapCnt;
#endif
    argument_overlap( l_lemma, lemmabuffer, lemmaOverlapCnt );
#ifdef DEBUG_OL
    if ( tmp2 != lemmaOverlapCnt ){
      cerr << " OVERLAPPED " << endl;
    }
    else
      cerr << endl;
#endif
  }
}

void wordStats::addMetrics( ) const {
  FoliaElement *el = folia_node;
  Document *doc = el->doc();
  if ( wwform != ::NO_VERB ){
    KWargs args;
    args["set"] = "tscan-set";
    args["class"] = "wwform(" + toString(wwform) + ")";
    el->addPosAnnotation( args );
  }
  if ( isPersRef )
    addOneMetric( doc, el, "pers_ref", "true" );
  if ( isPronRef )
    addOneMetric( doc, el, "pron_ref", "true" );
  if ( archaic )
    addOneMetric( doc, el, "archaic", "true" );
  if ( isContent )
    addOneMetric( doc, el, "content_word", "true" );
  if ( isNominal )
    addOneMetric( doc, el, "nominalization", "true" );
  if ( isOnder )
    addOneMetric( doc, el, "subordinate", "true" );
  if ( isImperative )
    addOneMetric( doc, el, "imperative", "true" );
  if ( isBetr )
    addOneMetric( doc, el, "betrekkelijk", "true" );
  if ( isPropNeg )
    addOneMetric( doc, el, "proper_negative", "true" );
  if ( isMorphNeg )
    addOneMetric( doc, el, "morph_negative", "true" );
  if ( connType != NOCONN )
    addOneMetric( doc, el, "connective", toString(connType) );
  if ( isMultiConn )
    addOneMetric( doc, el, "multi_connective", "true" );
  if ( lsa_opv )
    addOneMetric( doc, el, "lsa_word_suc", toString(lsa_opv) );
  if ( lsa_ctx )
    addOneMetric( doc, el, "lsa_word_ctx", toString(lsa_ctx) );
  if ( f50 )
    addOneMetric( doc, el, "f50", "true" );
  if ( f65 )
    addOneMetric( doc, el, "f65", "true" );
  if ( f77 )
    addOneMetric( doc, el, "f77", "true" );
  if ( f80 )
    addOneMetric( doc, el, "f80", "true" );
  if ( top_freq == top1000 )
    addOneMetric( doc, el, "top1000", "true" );
  else if ( top_freq == top2000 )
    addOneMetric( doc, el, "top2000", "true" );
  else if ( top_freq == top3000 )
    addOneMetric( doc, el, "top3000", "true" );
  else if ( top_freq == top5000 )
    addOneMetric( doc, el, "top5000", "true" );
  else if ( top_freq == top10000 )
    addOneMetric( doc, el, "top10000", "true" );
  else if ( top_freq == top20000 )
    addOneMetric( doc, el, "top20000", "true" );
  if ( compPartCnt > 0 )
    addOneMetric( doc, el, "compound_length", toString(compPartCnt) );
  addOneMetric( doc, el, "word_freq", toString(word_freq) );
  if ( word_freq_log != NA )
    addOneMetric( doc, el, "log_word_freq", toString(word_freq_log) );
  addOneMetric( doc, el, "lemma_freq", toString(lemma_freq) );
  if ( lemma_freq_log != NA )
    addOneMetric( doc, el, "log_lemma_freq", toString(lemma_freq_log) );
  addOneMetric( doc, el,
		"word_overlap_count", toString( wordOverlapCnt ) );
  addOneMetric( doc, el,
		"lemma_overlap_count", toString( lemmaOverlapCnt ) );
  if ( logprob10 != NA  )
    addOneMetric( doc, el, "lprob10", toString(logprob10) );
  if ( prop != JUSTAWORD )
    addOneMetric( doc, el, "property", toString(prop) );
  if ( sem_type != UNFOUND )
    addOneMetric( doc, el, "semtype", toString(sem_type) );
  if ( afkType != NO_A )
    addOneMetric( doc, el, "afktype", toString(afkType) );
}

void wordStats::CSVheader( ostream& os, const string& intro ) const {
  os << intro << ",";
  wordDifficultiesHeader( os );
  coherenceHeader( os );
  concreetHeader( os );
  persoonlijkheidHeader( os );
  wordSortHeader( os );
  miscHeader( os );
  os << endl;
}

void wordStats::wordDifficultiesHeader( ostream& os ) const {
  os << "lpw,wpl,lpwzn,wplzn,mpw,wpm,mpwzn,wpmzn,"
     << "sdpw,sdens,freq50,freq65,freq77,freq80,"
     << "word_freq_log,word_freq_log_zn,"
     << "lemfreq_log,lemfreq_log_zn,"
     << "freq1000,freq2000,freq3000,freq5000,freq10000,freq20000,so_suc,so_ctx,";
}

void wordStats::wordDifficultiesToCSV( ostream& os ) const {
  os << std::showpoint
     << double(charCnt) << ","
     << 1.0/double(charCnt) <<  ",";
  if ( prop == ISNAME ){
    os << "NA,NA,";
  }
  else {
    os << double(charCnt) << "," << 1.0/double(charCnt) <<  ",";
  }
  if ( morphCnt == 0 ){
    // LET() zaken o.a.
    os << 1.0 << "," << 1.0 << ",";
  }
  else {
    os << double(morphCnt) << ","
       << 1.0/double(morphCnt) << ",";
  }
  if ( prop == ISNAME ){
    os << "NA,NA,";
  }
  else {
    if ( morphCnt == 0 ){
      os << 1.0 << "," << 1.0 << ",";
    }
    else {
      os << double(morphCnt) << ","
	 << 1.0/double(morphCnt) << ",";
    }
  }
  os << double(compPartCnt) << ",";
  os << (compPartCnt?1:0) << ",";
  os << (f50?1:0) << ",";
  os << (f65?1:0) << ",";
  os << (f77?1:0) << ",";
  os << (f80?1:0) << ",";
  if ( word_freq_log == NA )
    os << "NA,";
  else
    os << word_freq_log << ",";
  if ( prop == ISNAME || word_freq_log == NA )
    os << "NA" << ",";
  else
    os << word_freq_log << ",";
  if ( lemma_freq_log == NA )
    os << "NA,";
  else
    os << lemma_freq_log << ",";
  if ( prop == ISNAME || lemma_freq_log == NA )
    os << "NA" << ",";
  else
    os << lemma_freq_log << ",";
  os << (top_freq==top1000?1:0) << ",";
  os << (top_freq<=top2000?1:0) << ",";
  os << (top_freq<=top3000?1:0) << ",";
  os << (top_freq<=top5000?1:0) << ",";
  os << (top_freq<=top10000?1:0) << ",";
  os << (top_freq<=top20000?1:0) << ",";
  os << lsa_opv << ",";
  os << lsa_ctx << ",";
}

void wordStats::coherenceHeader( ostream& os ) const {
  os << "temporeel,reeks,contrastief,comparatief,causaal,multiword,referential_pron,";
}

void wordStats::coherenceToCSV( ostream& os ) const {
  os << (connType==TEMPOREEL?1:0) << ","
     << (connType==OPSOMMEND?1:0) << ","
     << (connType==CONTRASTIEF) << ","
     << (connType==COMPARATIEF) << ","
     << (connType==CAUSAAL) << ","
     << isMultiConn << ","
     << isPronRef << ",";
}

void wordStats::concreetHeader( ostream& os ) const {
  os << "noun_conc_strict,";
  os << "noun_conc_broad,";
  os << "adj_conc_strict,";
  os << "adj_conc_broad,";
}

void wordStats::concreetToCSV( ostream& os ) const {
  if ( sem_type == CONCRETE_HUMAN_NOUN || sem_type == CONCRETE_NOUN ){
    os << "1,1,0,0,";
  }
  else if ( sem_type == BROAD_NOUN ){
    os << "0,1,0,0,";
  }
  else if ( sem_type  == CONCRETE_ADJ ){
    os << "0,0,1,1,";
  }
  else if ( sem_type == BROAD_ADJ || sem_type == EMO_ADJ ){
    os << "0,0,0,1,";
  }
  else {
    os << "0,0,0,0,";
  }
}

void wordStats::persoonlijkheidHeader( ostream& os ) const {
  os << "pers_ref,pers_pron_1,pers_pron_2,pers_pron_3,pers_pron,"
     << "name, NerPers, NerLoc, NerOrg, NerProd, NerEvent, NerMisc,"
     << "action_verb,state_verb,"
     << "process_verb,human_noun,"
     << "emo_adj,imperative,"
     << "question,";
}

void wordStats::persoonlijkheidToCSV( ostream& os ) const {
  os << isPersRef << ","
     << (prop == ISPPRON1 ) << ","
     << (prop == ISPPRON2 ) << ","
     << (prop == ISPPRON3 ) << ","
     << (prop == ISPPRON1 || prop == ISPPRON2 || prop == ISPPRON3) << ","
     << (prop == ISNAME) << ","
     << (nerProp == PER_B || nerProp == PER_I ) << ","
     << (nerProp == LOC_B || nerProp == LOC_I ) << ","
     << (nerProp == ORG_B || nerProp == ORG_I ) << ","
     << (nerProp == PRO_B || nerProp == PRO_I ) << ","
     << (nerProp == EVE_B || nerProp == EVE_I ) << ","
     << (nerProp == MISC_B || nerProp == MISC_I ) << ","
     << (sem_type == ACTION ) << ","
     << (sem_type == STATE ) << ","
     << (sem_type == PROCESS ) << ","
     << (sem_type == CONCRETE_HUMAN_NOUN ) << ","
     << (sem_type == EMO_ADJ ) << ","
     << isImperative << ","
     << "NA,";
}

void wordStats::wordSortHeader( ostream& os ) const {
  os << "adj,vg,vnw,lid,vz,bijw,tw,noun,verb,interjections,spec,let,"
     << "afk_tot,afk_gen,afk_int,afk_jur,afk_med,afk_ond,afk_pol,afk_ov,afk_zorg,";
}

void wordStats::wordSortToCSV( ostream& os ) const {
  os << (tag == CGN::ADJ ) << ","
     << (tag == CGN::VG ) << ","
     << (tag == CGN::VNW ) << ","
     << (tag == CGN::LID ) << ","
     << (tag == CGN::VZ ) << ","
     << (tag == CGN::BW ) << ","
     << (tag == CGN::TW ) << ","
     << (tag == CGN::N ) << ","
     << (tag == CGN::WW ) << ","
     << (tag == CGN::TSW ) << ","
     << (tag == CGN::SPEC ) << ","
     << (tag == CGN::LET ) << ","
     << toString( afkType != NO_A ) << ",";
  if ( afkType == GENERIEK_A )
    os << "1,";
  else
    os << "0,";
  if ( afkType == INTERNATIONAAL_A )
    os << "1,";
  else
    os << "0,";
  if ( afkType == JURIDISCH_A )
    os << "1,";
  else
    os << "0,";
  if ( afkType == MEDIA_A )
    os << "1,";
  else
    os << "0,";
  if ( afkType == ONDERWIJS_A )
    os << "1,";
  else
    os << "0,";
  if ( afkType == OVERHEID_A )
    os << "1,";
  else
    os << "0,";
  if ( afkType == OVERIGE_A )
    os << "1,";
  else
    os << "0,";
  if ( afkType == ZORG_A )
    os << "1,";
  else
    os << "0,";
}

void wordStats::miscHeader( ostream& os ) const {
  os << "present_verb,modal,time_verb,copula,archaic,"
     << "vol_deelw,onvol_deelw,infin,wopr_logprob";
}

void wordStats::miscToCSV( ostream& os ) const {
  os << (prop == ISPVTGW) << ","
     << (wwform == MODAL_VERB ) << ","
     << (wwform == TIME_VERB ) << ","
     << (wwform == COPULA ) << ","
     << archaic << ","
     << (prop == ISVD ) << ","
     << (prop == ISOD) << ","
     << (prop == ISINF ) << ",";
  if ( logprob10 == NA )
    os << "NA,";
  else
    os << logprob10 << ",";
}

void wordStats::toCSV( ostream& os ) const {
  os << '"' << word << '"';
  os << ",";
  wordDifficultiesToCSV( os );
  coherenceToCSV( os );
  concreetToCSV( os );
  persoonlijkheidToCSV( os );
  wordSortToCSV( os );
  miscToCSV( os );
  os << endl;
}

struct structStats: public basicStats {
  structStats( FoliaElement *el, const string& cat ):
    basicStats( el, cat ),
    wordCnt(0),
    sentCnt(0),
    parseFailCnt(0),
    vdCnt(0),odCnt(0),
    infCnt(0), presentCnt(0), pastCnt(0), subjonctCnt(0),
    nameCnt(0),
    pron1Cnt(0), pron2Cnt(0), pron3Cnt(0),
    passiveCnt(0),modalCnt(0),timeCnt(0),koppelCnt(0),
    persRefCnt(0), pronRefCnt(0),
    archaicsCnt(0),
    contentCnt(0),
    nominalCnt(0),
    adjCnt(0),
    vgCnt(0),
    vnwCnt(0),
    lidCnt(0),
    vzCnt(0),
    bwCnt(0),
    twCnt(0),
    nounCnt(0),
    verbCnt(0),
    tswCnt(0),
    specCnt(0),
    letCnt(0),
    onderCnt(0),
    betrCnt(0),
    cnjCnt(0),
    crdCnt(0),
    tempConnCnt(0),
    opsomConnCnt(0),
    contConnCnt(0),
    compConnCnt(0),
    causeConnCnt(0),
    propNegCnt(0),
    morphNegCnt(0),
    multiNegCnt(0),
    wordOverlapCnt(0),
    lemmaOverlapCnt(0),
    f50Cnt(0),
    f65Cnt(0),
    f77Cnt(0),
    f80Cnt(0),
    compCnt(0),
    compPartCnt(0),
    top1000Cnt(0),
    top2000Cnt(0),
    top3000Cnt(0),
    top5000Cnt(0),
    top10000Cnt(0),
    top20000Cnt(0),
    word_freq(0),
    word_freq_n(0),
    word_freq_log(NA),
    word_freq_log_n(NA),
    lemma_freq(0),
    lemma_freq_n(0),
    lemma_freq_log(NA),
    lemma_freq_log_n(NA),
    avg_prob10(NA),
    entropy(NA),
    perplexity(NA),
    lsa_word_suc(NA),
    lsa_word_net(NA),
    lsa_sent_suc(NA),
    lsa_sent_net(NA),
    lsa_sent_ctx(NA),
    lsa_par_suc(NA),
    lsa_par_net(NA),
    lsa_par_ctx(NA),
    lsa_doc_ctx(NA),
    broadConcreteNounCnt(0),
    strictConcreteNounCnt(0),
    broadAbstractNounCnt(0),
    strictAbstractNounCnt(0),
    broadConcreteAdjCnt(0),
    strictConcreteAdjCnt(0),
    broadAbstractAdjCnt(0),
    strictAbstractAdjCnt(0),
    stateCnt(0),
    actionCnt(0),
    processCnt(0),
    weirdCnt(0),
    emoCnt(0),
    humanCnt(0),
    npCnt(0),
    indefNpCnt(0),
    npSize(0),
    vcModCnt(0),
    adjNpModCnt(0),
    npModCnt(0),
    dLevel(-1),
    dLevel_gt4(0),
    impCnt(0),
    questCnt(0),
    prepExprCnt(0)
 {};
  ~structStats();
  void addMetrics( ) const;
  void wordDifficultiesHeader( ostream& ) const;
  void wordDifficultiesToCSV( ostream& ) const;
  void sentDifficultiesHeader( ostream& ) const;
  void sentDifficultiesToCSV( ostream& ) const;
  void infoHeader( ostream& ) const;
  void informationDensityToCSV( ostream& ) const;
  void coherenceHeader( ostream& ) const;
  void coherenceToCSV( ostream& ) const;
  void concreetHeader( ostream& ) const;
  void concreetToCSV( ostream& ) const;
  void persoonlijkheidHeader( ostream& ) const;
  void persoonlijkheidToCSV( ostream& ) const;
  void wordSortHeader( ostream& ) const;
  void wordSortToCSV( ostream& ) const;
  void miscHeader( ostream& ) const;
  void miscToCSV( ostream& ) const;
  void CSVheader( ostream&, const string& ) const;
  void toCSV( ostream& ) const;
  void merge( structStats * );
  virtual bool isSentence() const { return false; };
  virtual bool isDocument() const { return false; };
  virtual int word_overlapCnt() const { return -1; };
  virtual int lemma_overlapCnt() const { return-1; };
  vector<const wordStats*> collectWords() const;
  virtual void setLSAvalues( double, double, double = 0 ) = 0;
  virtual void resolveLSA( const map<string,double>& );
  void calculate_LSA_summary();
  string text;
  int wordCnt;
  int sentCnt;
  int parseFailCnt;
  int vdCnt;
  int odCnt;
  int infCnt;
  int presentCnt;
  int pastCnt;
  int subjonctCnt;
  int nameCnt;
  int pron1Cnt;
  int pron2Cnt;
  int pron3Cnt;
  int passiveCnt;
  int modalCnt;
  int timeCnt;
  int koppelCnt;
  int persRefCnt;
  int pronRefCnt;
  int archaicsCnt;
  int contentCnt;
  int nominalCnt;
  int adjCnt;
  int vgCnt;
  int vnwCnt;
  int lidCnt;
  int vzCnt;
  int bwCnt;
  int twCnt;
  int nounCnt;
  int verbCnt;
  int tswCnt;
  int specCnt;
  int letCnt;
  int onderCnt;
  int betrCnt;
  int cnjCnt;
  int crdCnt;
  int tempConnCnt;
  int opsomConnCnt;
  int contConnCnt;
  int compConnCnt;
  int causeConnCnt;
  int propNegCnt;
  int morphNegCnt;
  int multiNegCnt;
  int wordOverlapCnt;
  int lemmaOverlapCnt;
  int f50Cnt;
  int f65Cnt;
  int f77Cnt;
  int f80Cnt;
  int compCnt;
  int compPartCnt;
  int top1000Cnt;
  int top2000Cnt;
  int top3000Cnt;
  int top5000Cnt;
  int top10000Cnt;
  int top20000Cnt;
  int word_freq;
  int word_freq_n;
  double word_freq_log;
  double word_freq_log_n;
  int lemma_freq;
  int lemma_freq_n;
  double lemma_freq_log;
  double lemma_freq_log_n;
  double avg_prob10;
  double entropy;
  double perplexity;
  double lsa_word_suc;
  double lsa_word_net;
  double lsa_sent_suc;
  double lsa_sent_net;
  double lsa_sent_ctx;
  double lsa_par_suc;
  double lsa_par_net;
  double lsa_par_ctx;
  double lsa_doc_ctx;
  int broadConcreteNounCnt;
  int strictConcreteNounCnt;
  int broadAbstractNounCnt;
  int strictAbstractNounCnt;
  int broadConcreteAdjCnt;
  int strictConcreteAdjCnt;
  int broadAbstractAdjCnt;
  int strictAbstractAdjCnt;
  int stateCnt;
  int actionCnt;
  int processCnt;
  int weirdCnt;
  int emoCnt;
  int humanCnt;
  int npCnt;
  int indefNpCnt;
  int npSize;
  int vcModCnt;
  int adjNpModCnt;
  int npModCnt;
  int dLevel;
  int dLevel_gt4;
  int impCnt;
  int questCnt;
  int prepExprCnt;
  map<CGN::Type,int> heads;
  map<string,int> unique_names;
  map<string,int> unique_contents;
  map<string,int> unique_words;
  map<string,int> unique_lemmas;
  map<NerProp, int> ners;
  map<AfkType, int> afks;
  multimap<DD_type,int> distances;
};

structStats::~structStats(){
  vector<basicStats *>::iterator it = sv.begin();
  while ( it != sv.end() ){
    delete( *it );
    ++it;
  }
}

void structStats::merge( structStats *ss ){
  if ( ss->parseFailCnt == -1 ) // not parsed
    parseFailCnt = -1;
  else
    parseFailCnt += ss->parseFailCnt;
  wordCnt += ss->wordCnt;
  sentCnt += ss->sentCnt;
  charCnt += ss->charCnt;
  charCntExNames += ss->charCntExNames;
  morphCnt += ss->morphCnt;
  morphCntExNames += ss->morphCntExNames;
  nameCnt += ss->nameCnt;
  vdCnt += ss->vdCnt;
  odCnt += ss->odCnt;
  infCnt += ss->infCnt;
  passiveCnt += ss->passiveCnt;
  modalCnt += ss->modalCnt;
  timeCnt += ss->timeCnt;
  koppelCnt += ss->koppelCnt;
  archaicsCnt += ss->archaicsCnt;
  contentCnt += ss->contentCnt;
  nominalCnt += ss->nominalCnt;
  adjCnt += ss->adjCnt;
  vgCnt += ss->vgCnt;
  vnwCnt += ss->vnwCnt;
  lidCnt += ss->lidCnt;
  vzCnt += ss->vzCnt;
  bwCnt += ss->bwCnt;
  twCnt += ss->twCnt;
  nounCnt += ss->nounCnt;
  verbCnt += ss->verbCnt;
  tswCnt += ss->tswCnt;
  specCnt += ss->specCnt;
  letCnt += ss->letCnt;
  onderCnt += ss->onderCnt;
  betrCnt += ss->betrCnt;
  cnjCnt += ss->cnjCnt;
  crdCnt += ss->crdCnt;
  tempConnCnt += ss->tempConnCnt;
  opsomConnCnt += ss->opsomConnCnt;
  contConnCnt += ss->contConnCnt;
  compConnCnt += ss->compConnCnt;
  causeConnCnt += ss->causeConnCnt;
  prepExprCnt += ss->prepExprCnt;
  propNegCnt += ss->propNegCnt;
  morphNegCnt += ss->morphNegCnt;
  multiNegCnt += ss->multiNegCnt;
  wordOverlapCnt += ss->wordOverlapCnt;
  lemmaOverlapCnt += ss->lemmaOverlapCnt;
  f50Cnt += ss->f50Cnt;
  f65Cnt += ss->f65Cnt;
  f77Cnt += ss->f77Cnt;
  f80Cnt += ss->f80Cnt;
  compCnt += ss->compCnt;
  compPartCnt += ss->compPartCnt;
  top1000Cnt += ss->top1000Cnt;
  top2000Cnt += ss->top2000Cnt;
  top3000Cnt += ss->top3000Cnt;
  top5000Cnt += ss->top5000Cnt;
  top10000Cnt += ss->top10000Cnt;
  top20000Cnt += ss->top20000Cnt;
  word_freq += ss->word_freq;
  word_freq_n += ss->word_freq_n;
  lemma_freq += ss->lemma_freq;
  lemma_freq_n += ss->lemma_freq_n;
  if ( ss->avg_prob10 != NA ){
    if ( avg_prob10 == NA )
      avg_prob10 = ss->avg_prob10;
    else
      avg_prob10 += ss->avg_prob10;
  }
  if ( ss->entropy != NA ){
    if ( entropy == NA )
      entropy = ss->entropy;
    else
      entropy += ss->entropy;
  }
  if ( ss->perplexity != NA ){
    if ( perplexity == NA )
      perplexity = ss->perplexity;
    else
      perplexity += ss->perplexity;
  }

  presentCnt += ss->presentCnt;
  pastCnt += ss->pastCnt;
  subjonctCnt += ss->subjonctCnt;
  pron1Cnt += ss->pron1Cnt;
  pron2Cnt += ss->pron2Cnt;
  pron3Cnt += ss->pron3Cnt;
  persRefCnt += ss->persRefCnt;
  pronRefCnt += ss->pronRefCnt;
  strictAbstractNounCnt += ss->strictAbstractNounCnt;
  broadAbstractNounCnt += ss->broadAbstractNounCnt;
  strictConcreteNounCnt += ss->strictConcreteNounCnt;
  broadConcreteNounCnt += ss->broadConcreteNounCnt;
  strictAbstractAdjCnt += ss->strictAbstractAdjCnt;
  broadAbstractAdjCnt += ss->broadAbstractAdjCnt;
  strictConcreteAdjCnt += ss->strictConcreteAdjCnt;
  broadConcreteAdjCnt += ss->broadConcreteAdjCnt;
  stateCnt += ss->stateCnt;
  actionCnt += ss->actionCnt;
  processCnt += ss->processCnt;
  weirdCnt += ss->weirdCnt;
  emoCnt += ss->emoCnt;
  humanCnt += ss->humanCnt;
  npCnt += ss->npCnt;
  indefNpCnt += ss->indefNpCnt;
  npSize += ss->npSize;
  vcModCnt += ss->vcModCnt;
  adjNpModCnt += ss->adjNpModCnt;
  npModCnt += ss->npModCnt;
  if ( ss->dLevel >= 0 ){
    if ( dLevel < 0 )
      dLevel = ss->dLevel;
    else
      dLevel += ss->dLevel;
  }
  dLevel_gt4 += ss->dLevel_gt4;
  impCnt += ss->impCnt;
  questCnt += ss->questCnt;
  sv.push_back( ss );
  aggregate( heads, ss->heads );
  aggregate( unique_names, ss->unique_names );
  aggregate( unique_contents, ss->unique_contents );
  aggregate( unique_words, ss->unique_words );
  aggregate( unique_lemmas, ss->unique_lemmas );
  aggregate( ners, ss->ners );
  aggregate( afks, ss->afks );
  aggregate( distances, ss->distances );
}

string MMtoString( const multimap<DD_type, int>& mm, DD_type t ){
  size_t len = mm.count(t);
  if ( len > 0 ){
    int result = 0;
    for( multimap<DD_type, int>::const_iterator pos = mm.lower_bound(t);
	 pos != mm.upper_bound(t);
	 ++pos ){
      result += pos->second;
    }
    return toString( result/double(len) );
  }
  else
    return "NA";
}

string MMtoString( const multimap<DD_type, int>& mm ){
  size_t len = mm.size();
  if ( len > 0 ){
    int result = 0;
    for( multimap<DD_type, int>::const_iterator pos = mm.begin();
	 pos != mm.end();
	 ++pos ){
      result += pos->second;
    }
    return toString( result/double(len) );
  }
  else
    return "NA";
}

double getHighest( const multimap<DD_type, int>&mm ){
  double result = NA;
  for( multimap<DD_type, int>::const_iterator pos = mm.begin();
       pos != mm.end();
       ++pos ){
    if ( pos->second > result )
      result = pos->second;
  }
  return result;
}

template <class T>
int at( const map<T,int>& m, const T key ){
  typename map<T,int>::const_iterator it = m.find( key );
  if ( it != m.end() )
    return it->second;
  else
    return 0;
}

string toMString( double d ){
  if ( d == NA )
    return "NA";
  else
    return toString( d );
}

void structStats::addMetrics( ) const {
  FoliaElement *el = folia_node;
  Document *doc = el->doc();
  addOneMetric( doc, el, "word_count", toString(wordCnt) );
  addOneMetric( doc, el, "vd_count", toString(vdCnt) );
  addOneMetric( doc, el, "od_count", toString(odCnt) );
  addOneMetric( doc, el, "inf_count", toString(infCnt) );
  addOneMetric( doc, el, "present_verb_count", toString(presentCnt) );
  addOneMetric( doc, el, "past_verb_count", toString(pastCnt) );
  addOneMetric( doc, el, "subjonct_count", toString(subjonctCnt) );
  addOneMetric( doc, el, "name_count", toString(nameCnt) );
  int val = at( ners, PER_B );
  addOneMetric( doc, el, "personal_name_count", toString(val) );
  val = at( ners, LOC_B );
  addOneMetric( doc, el, "location_name_count", toString(val) );
  val = at( ners, ORG_B );
  addOneMetric( doc, el, "organization_name_count", toString(val) );
  val = at( ners, PRO_B );
  addOneMetric( doc, el, "product_name_count", toString(val) );
  val = at( ners, EVE_B );
  addOneMetric( doc, el, "event_name_count", toString(val) );
  val = at( afks, OVERHEID_A );
  if ( val > 0 ){
    addOneMetric( doc, el, "overheid_afk_count", toString(val) );
  }
  val = at( afks, JURIDISCH_A );
  if ( val > 0 ){
    addOneMetric( doc, el, "juridisch_afk_count", toString(val) );
  }
  val = at( afks, ONDERWIJS_A );
  if ( val > 0 ){
    addOneMetric( doc, el, "onderwijs_afk_count", toString(val) );
  }
  val = at( afks, MEDIA_A );
  if ( val > 0 ){
    addOneMetric( doc, el, "media_afk_count", toString(val) );
  }
  val = at( afks, GENERIEK_A );
  if ( val > 0 ){
    addOneMetric( doc, el, "generiek_afk_count", toString(val) );
  }
  val = at( afks, OVERIGE_A );
  if ( val > 0 ){
    addOneMetric( doc, el, "overige_afk_count", toString(val) );
  }
  val = at( afks, INTERNATIONAAL_A );
  if ( val > 0 ){
    addOneMetric( doc, el, "internationaal_afk_count", toString(val) );
  }
  val = at( afks, ZORG_A );
  if ( val > 0 ){
    addOneMetric( doc, el, "zorg_afk_count", toString(val) );
  }

  addOneMetric( doc, el, "pers_pron_1_count", toString(pron1Cnt) );
  addOneMetric( doc, el, "pers_pron_2_count", toString(pron2Cnt) );
  addOneMetric( doc, el, "pres_pron_3_count", toString(pron3Cnt) );
  addOneMetric( doc, el, "passive_count", toString(passiveCnt) );
  addOneMetric( doc, el, "modal_count", toString(modalCnt) );
  addOneMetric( doc, el, "time_count", toString(timeCnt) );
  addOneMetric( doc, el, "koppel_count", toString(koppelCnt) );
  addOneMetric( doc, el, "pers_ref_count", toString(persRefCnt) );
  addOneMetric( doc, el, "pron_ref_count", toString(pronRefCnt) );
  addOneMetric( doc, el, "archaic_count", toString(archaicsCnt) );
  addOneMetric( doc, el, "content_count", toString(contentCnt) );
  addOneMetric( doc, el, "nominal_count", toString(nominalCnt) );
  addOneMetric( doc, el, "adj_count", toString(adjCnt) );
  addOneMetric( doc, el, "vg_count", toString(vgCnt) );
  addOneMetric( doc, el, "vnw_count", toString(vnwCnt) );
  addOneMetric( doc, el, "lid_count", toString(lidCnt) );
  addOneMetric( doc, el, "vz_count", toString(vzCnt) );
  addOneMetric( doc, el, "bw_count", toString(bwCnt) );
  addOneMetric( doc, el, "tw_count", toString(twCnt) );
  addOneMetric( doc, el, "noun_count", toString(nounCnt) );
  addOneMetric( doc, el, "verb_count", toString(verbCnt) );
  addOneMetric( doc, el, "tsw_count", toString(tswCnt) );
  addOneMetric( doc, el, "spec_count", toString(specCnt) );
  addOneMetric( doc, el, "let_count", toString(letCnt) );
  addOneMetric( doc, el, "subord_count", toString(onderCnt) );
  addOneMetric( doc, el, "rel_count", toString(betrCnt) );
  // addOneMetric( doc, el, "cnj_count", toString(cnjCnt) );
  // addOneMetric( doc, el, "crd_count", toString(crdCnt) );
  addOneMetric( doc, el, "temporal_connector_count", toString(tempConnCnt) );
  addOneMetric( doc, el, "reeks_connector_count", toString(opsomConnCnt) );
  addOneMetric( doc, el, "contrast_connector_count", toString(contConnCnt) );
  addOneMetric( doc, el, "comparatief_connector_count", toString(compConnCnt) );
  addOneMetric( doc, el, "causaal_connector_count", toString(causeConnCnt) );
  addOneMetric( doc, el, "prop_neg_count", toString(propNegCnt) );
  addOneMetric( doc, el, "morph_neg_count", toString(morphNegCnt) );
  addOneMetric( doc, el, "multiple_neg_count", toString(multiNegCnt) );
  addOneMetric( doc, el, "voorzetsel_expression_count", toString(prepExprCnt) );
  addOneMetric( doc, el,
		"word_overlap_count", toString( wordOverlapCnt ) );
  addOneMetric( doc, el,
		"lemma_overlap_count", toString( lemmaOverlapCnt ) );
  if ( lsa_opv )
    addOneMetric( doc, el, "lsa_" + category + "_suc", toString(lsa_opv) );
  if ( lsa_ctx )
    addOneMetric( doc, el, "lsa_" + category + "_ctx", toString(lsa_ctx) );
  if ( lsa_word_suc != NA )
    addOneMetric( doc, el, "lsa_word_suc_avg", toString(lsa_word_suc) );
  if ( lsa_word_net != NA )
    addOneMetric( doc, el, "lsa_word_net_avg", toString(lsa_word_net) );
  if ( lsa_sent_suc != NA )
    addOneMetric( doc, el, "lsa_sent_suc_avg", toString(lsa_sent_suc) );
  if ( lsa_sent_net != NA )
    addOneMetric( doc, el, "lsa_sent_net_avg", toString(lsa_sent_net) );
  if ( lsa_sent_ctx != NA )
    addOneMetric( doc, el, "lsa_sent_ctx_avg", toString(lsa_sent_ctx) );
  if ( lsa_par_suc != NA )
    addOneMetric( doc, el, "lsa_par_suc_avg", toString(lsa_par_suc) );
  if ( lsa_par_net != NA )
    addOneMetric( doc, el, "lsa_par_net_avg", toString(lsa_par_net) );
  if ( lsa_par_ctx != NA )
    addOneMetric( doc, el, "lsa_par_ctx_avg", toString(lsa_par_ctx) );
  addOneMetric( doc, el, "freq50", toString(f50Cnt) );
  addOneMetric( doc, el, "freq65", toString(f65Cnt) );
  addOneMetric( doc, el, "freq77", toString(f77Cnt) );
  addOneMetric( doc, el, "freq80", toString(f80Cnt) );
  addOneMetric( doc, el, "compound_count", toString(compCnt) );
  addOneMetric( doc, el, "compound_length", toString(compPartCnt) );
  addOneMetric( doc, el, "top1000", toString(top1000Cnt) );
  addOneMetric( doc, el, "top2000", toString(top2000Cnt) );
  addOneMetric( doc, el, "top3000", toString(top3000Cnt) );
  addOneMetric( doc, el, "top5000", toString(top5000Cnt) );
  addOneMetric( doc, el, "top10000", toString(top10000Cnt) );
  addOneMetric( doc, el, "top20000", toString(top20000Cnt) );
  addOneMetric( doc, el, "word_freq", toString(word_freq) );
  addOneMetric( doc, el, "word_freq_no_names", toString(word_freq_n) );
  if ( word_freq_log != NA  )
    addOneMetric( doc, el, "log_word_freq", toString(word_freq_log) );
  if ( word_freq_log_n != NA  )
    addOneMetric( doc, el, "log_word_freq_no_names", toString(word_freq_log_n) );
  addOneMetric( doc, el, "lemma_freq", toString(lemma_freq) );
  addOneMetric( doc, el, "lemma_freq_no_names", toString(lemma_freq_n) );
  if ( lemma_freq_log != NA  )
    addOneMetric( doc, el, "log_lemma_freq", toString(lemma_freq_log) );
  if ( lemma_freq_log_n != NA  )
    addOneMetric( doc, el, "log_lemma_freq_no_names", toString(lemma_freq_log_n) );
  if ( avg_prob10 != NA )
    addOneMetric( doc, el, "wopr_logprob", toString(avg_prob10) );
  if ( entropy != NA )
    addOneMetric( doc, el, "wopr_entropy", toString(entropy) );
  if ( perplexity != NA )
    addOneMetric( doc, el, "wopr_perplexity", toString(perplexity) );
  addOneMetric( doc, el, "concrete_broad_noun", toString(broadConcreteNounCnt) );
  addOneMetric( doc, el, "concrete_strict_noun", toString(strictConcreteNounCnt) );
  addOneMetric( doc, el, "abstract_broad_noun", toString(broadAbstractNounCnt) );
  addOneMetric( doc, el, "abstract_strict_noun", toString(strictAbstractNounCnt) );
  addOneMetric( doc, el, "concrete_broad_adj", toString(broadConcreteAdjCnt) );
  addOneMetric( doc, el, "concrete_strict_adj", toString(strictConcreteAdjCnt) );
  addOneMetric( doc, el, "abstract_broad_adj", toString(broadAbstractAdjCnt) );
  addOneMetric( doc, el, "abstract_strict_adj", toString(strictAbstractAdjCnt) );
  addOneMetric( doc, el, "state_count", toString(stateCnt) );
  addOneMetric( doc, el, "action_count", toString(actionCnt) );
  addOneMetric( doc, el, "process_count", toString(processCnt) );
  addOneMetric( doc, el, "weird_count", toString(weirdCnt) );
  addOneMetric( doc, el, "emo_count", toString(emoCnt) );
  addOneMetric( doc, el, "human_nouns_count", toString(humanCnt) );
  addOneMetric( doc, el, "indef_np_count", toString(indefNpCnt) );
  addOneMetric( doc, el, "np_count", toString(npCnt) );
  addOneMetric( doc, el, "np_size", toString(npSize) );
  addOneMetric( doc, el, "vc_modifier_count", toString(vcModCnt) );
  addOneMetric( doc, el, "adj_np_modifier_count", toString(adjNpModCnt) );
  addOneMetric( doc, el, "np_modifier_count", toString(npModCnt) );

  addOneMetric( doc, el, "character_count", toString(charCnt) );
  addOneMetric( doc, el, "character_count_min_names", toString(charCntExNames) );
  addOneMetric( doc, el, "morpheme_count", toString(morphCnt) );
  addOneMetric( doc, el, "morpheme_count_min_names", toString(morphCntExNames) );
  if ( dLevel >= 0 )
    addOneMetric( doc, el, "d_level", toString(dLevel) );
  else
    addOneMetric( doc, el, "d_level", "missing" );
  if ( dLevel_gt4 != 0 )
    addOneMetric( doc, el, "d_level_gt4", toString(dLevel_gt4) );
  if ( questCnt > 0 )
    addOneMetric( doc, el, "question_count", toString(questCnt) );
  if ( impCnt > 0 )
    addOneMetric( doc, el, "imperative_count", toString(impCnt) );
  addOneMetric( doc, el, "sub_verb_dist", MMtoString( distances, SUB_VERB ) );
  addOneMetric( doc, el, "obj_verb_dist", MMtoString( distances, OBJ1_VERB ) );
  addOneMetric( doc, el, "lijdend_verb_dist", MMtoString( distances, OBJ2_VERB ) );
  addOneMetric( doc, el, "verb_pp_dist", MMtoString( distances, VERB_PP ) );
  addOneMetric( doc, el, "noun_det_dist", MMtoString( distances, NOUN_DET ) );
  addOneMetric( doc, el, "prep_obj_dist", MMtoString( distances, PREP_OBJ1 ) );
  addOneMetric( doc, el, "verb_vc_dist", MMtoString( distances, VERB_VC ) );
  addOneMetric( doc, el, "comp_body_dist", MMtoString( distances, COMP_BODY ) );
  addOneMetric( doc, el, "crd_cnj_dist", MMtoString( distances, CRD_CNJ ) );
  addOneMetric( doc, el, "verb_comp_dist", MMtoString( distances, VERB_COMP ) );
  addOneMetric( doc, el, "noun_vc_dist", MMtoString( distances, NOUN_VC ) );
  addOneMetric( doc, el, "deplen", MMtoString( distances ) );
  addOneMetric( doc, el, "max_deplen", toMString( getHighest( distances )/sentCnt ) );
  for ( size_t i=0; i < sv.size(); ++i ){
    sv[i]->addMetrics();
  }
}

void structStats::CSVheader( ostream& os, const string& intro ) const {
  os << intro << ",parse_failure,";
  wordDifficultiesHeader( os );
  sentDifficultiesHeader( os );
  infoHeader( os );
  coherenceHeader( os );
  concreetHeader( os );
  persoonlijkheidHeader( os );
  wordSortHeader( os );
  miscHeader( os );
  os << endl;
}

void structStats::toCSV( ostream& os ) const {
  os << parseFailCnt << ",";
  wordDifficultiesToCSV( os );
  sentDifficultiesToCSV( os );
  informationDensityToCSV( os );
  coherenceToCSV( os );
  concreetToCSV( os );
  persoonlijkheidToCSV( os );
  wordSortToCSV( os );
  miscToCSV( os );
  os << endl;
}

void structStats::wordDifficultiesHeader( ostream& os ) const {
  os << "lpw,wpl,lpwzn,wplzn,mpw,wpm,mpwzn,wpmzn,"
     << "sdpw,sdens,freq50,freq65,freq77,freq80,"
     << "word_freq_log,word_freq_log_zn,"
     << "lemma_freq_log,lemma_freq_log_zn,"
     << "freq1000,freq2000,freq3000,freq5000,freq10000,freq20000,"
     << "so_word_suc,so_word_net,";
}

void structStats::wordDifficultiesToCSV( ostream& os ) const {
  os << std::showpoint
     << ratio( charCnt, wordCnt ) << ","
     << ratio( wordCnt, charCnt ) <<  ","
     << ratio( charCntExNames, (wordCnt-nameCnt) ) << ","
     << ratio( (wordCnt - nameCnt), charCntExNames ) <<  ","
     << ratio( morphCnt, wordCnt ) << ","
     << ratio( wordCnt, morphCnt ) << ","
     << ratio( morphCntExNames, (wordCnt-nameCnt) ) << ","
     << ratio( (wordCnt-nameCnt), morphCntExNames ) << ","
     << ratio( compPartCnt, wordCnt ) << ","
     << density( compCnt, wordCnt ) << ",";

  os << ratio( f50Cnt, wordCnt ) << ",";
  os << ratio( f65Cnt, wordCnt ) << ",";
  os << ratio( f77Cnt, wordCnt ) << ",";
  os << ratio( f80Cnt, wordCnt ) << ",";
  os << ratio( word_freq_log, wordCnt ) << ",";
  os << ratio( word_freq_log_n, (wordCnt-nameCnt) ) << ",";
  os << ratio( lemma_freq_log, wordCnt ) << ",";
  os << ratio( lemma_freq_log_n, (wordCnt-nameCnt) ) << ",";
  os << ratio( top1000Cnt, wordCnt ) << ",";
  os << ratio( top2000Cnt, wordCnt ) << ",";
  os << ratio( top3000Cnt, wordCnt ) << ",";
  os << ratio( top5000Cnt, wordCnt ) << ",";
  os << ratio( top10000Cnt, wordCnt ) << ",";
  os << ratio( top20000Cnt, wordCnt ) << ",";
  os << lsa_word_suc << "," << lsa_word_net << ",";
}

void structStats::sentDifficultiesHeader( ostream& os ) const {
  os << "wpz,pzw,wnp,subord_clause,rel_clause,clauses_d,clauses_g,"
     << "dlevel,dlevel_gt4_prop,dlevel_gt4_r,"
     << "nom,lv_d,lv_g,prop_negs,morph_negs,total_negs,multiple_negs,"
     << "deplen_subverb,deplen_dirobverb,deplen_indirobverb,deplen_verbpp,"
     << "deplen_noundet,deplen_prepobj,deplen_verbvc,"
     << "deplen_compbody,deplen_crdcnj,deplen_verbcp,deplen_noun_vc,"
     << "deplen,deplen_max,";
}

void structStats::sentDifficultiesToCSV( ostream& os ) const {
  os << ratio( wordCnt, sentCnt ) << ","
     << ratio( sentCnt * 100, wordCnt )  << ",";
  os << ratio( wordCnt, npCnt ) << ",";
  os << density( onderCnt, wordCnt ) << ","
     << density( betrCnt, wordCnt ) << ","
     << density( pastCnt + presentCnt, wordCnt ) << ","
     << ratio( pastCnt + presentCnt, sentCnt ) << ","
     << ratio( dLevel, sentCnt ) << ","
     << ratio( dLevel_gt4, sentCnt ) << ",";
  os << ratio( dLevel_gt4, sentCnt - dLevel_gt4 ) << ",";
  os << density( nominalCnt, wordCnt ) << ","
     << density( passiveCnt, wordCnt ) << ",";
  os << ratio( passiveCnt, pastCnt + presentCnt ) << ",";
  os << density( propNegCnt, wordCnt ) << ","
     << density( morphNegCnt, wordCnt ) << ","
     << density( propNegCnt+morphNegCnt, wordCnt ) << ","
     << density( multiNegCnt, wordCnt ) << ",";
  os << MMtoString( distances, SUB_VERB ) << ",";
  os << MMtoString( distances, OBJ1_VERB ) << ",";
  os << MMtoString( distances, OBJ2_VERB ) << ",";
  os << MMtoString( distances, VERB_PP ) << ",";
  os << MMtoString( distances, NOUN_DET ) << ",";
  os << MMtoString( distances, PREP_OBJ1 ) << ",";
  os << MMtoString( distances, VERB_VC ) << ",";
  os << MMtoString( distances, COMP_BODY ) << ",";
  os << MMtoString( distances, CRD_CNJ ) << ",";
  os << MMtoString( distances, VERB_COMP ) << ",";
  os << MMtoString( distances, NOUN_VC ) << ",";
  os << MMtoString( distances ) << ",";
  os << ratio( getHighest( distances ), sentCnt ) << ",";
}

void structStats::infoHeader( ostream& os ) const {
  os << "word_ttr,lemma_ttr,ttr_namen, ttr_inhoudswoorden,"
     << "content_words_r,content_words_d,content_words_g,"
     << "rar_index,vc_mods_d,vc_mods_g,"
     << "adj_np_mods_d,adj_np_mods_g, np_mods_d,np_mods_g,"
     << "ov_np_mods_d,ov_np_mods_g,np_dens,";
}

void structStats::informationDensityToCSV( ostream& os ) const {
  os << ratio( unique_words.size(), wordCnt ) << ",";
  os << ratio( unique_lemmas.size(), wordCnt ) << ",";
  os << ratio( unique_names.size(), nameCnt ) << ",";
  os << ratio( unique_contents.size(), contentCnt ) << ",";
  os << ratio( contentCnt, wordCnt - contentCnt ) << ",";
  os << density( contentCnt, wordCnt ) << ",";
  os << ratio( contentCnt, pastCnt + presentCnt ) << ",";
  os << rarity( settings.rarityLevel ) << ",";
  os << density( vcModCnt, wordCnt ) << ",";
  os << ratio( vcModCnt, pastCnt + presentCnt ) << ",";
  os << density( adjNpModCnt, wordCnt ) << ",";
  os << ratio( adjNpModCnt, pastCnt + presentCnt ) << ",";
  os << density( npModCnt, wordCnt ) << ",";
  os << ratio( npModCnt, pastCnt + presentCnt ) << ",";
  os << density( (npModCnt-adjNpModCnt), wordCnt ) << ",";
  os << ratio( (npModCnt-adjNpModCnt), pastCnt + presentCnt ) << ",";
  os << ratio( npCnt, wordCnt ) << ",";
}


void structStats::coherenceHeader( ostream& os ) const {
  os << "temporals,reeks,contrast,comparatief,causal,referential_prons,"
     << "argument_overlap_d,argument_overlap_g,lem_argument_overlap_d,"
     << "lem_argument_overlap_g,wordbuffer_argument_overlap_d,"
     << "wordbuffer_argument_overlap_g,lemmabuffer_argument_overlap_d,"
     << "lemmabuffer_argument_overlap_g,indef_nps_p,indef_nps_r,indef_nps_g,";
}

void structStats::coherenceToCSV( ostream& os ) const {
  os << density( tempConnCnt, wordCnt ) << ","
     << density( opsomConnCnt, wordCnt ) << ","
     << density( contConnCnt, wordCnt ) << ","
     << density( compConnCnt, wordCnt ) << ","
     << density( causeConnCnt, wordCnt ) << ","
     << density( pronRefCnt, wordCnt ) << ",";
  if ( isSentence() ){
    os << double(wordOverlapCnt) << ",NA,"
       << double(lemmaOverlapCnt) << ",NA,";
  }
  else {
    os << density( wordOverlapCnt, wordCnt ) << ","
       << ratio( wordOverlapCnt, sentCnt ) << ",";
    os << density( lemmaOverlapCnt, wordCnt ) << ","
       << ratio( lemmaOverlapCnt, sentCnt ) << ",";
  }
  if ( !isDocument() ){
    os << "NA,NA,NA,NA,";
  }
  else {
    os << density( word_overlapCnt(), wordCnt ) << ","
       << ratio( word_overlapCnt(), (presentCnt+pastCnt) ) << ","
       << density( lemma_overlapCnt(), wordCnt ) << ","
       << ratio( lemma_overlapCnt(), (presentCnt+pastCnt) )<< ",";
  }
  os << ratio( indefNpCnt, npCnt ) << ",";
  os << ratio( indefNpCnt, npCnt - indefNpCnt ) << ",";
  os << density( indefNpCnt, wordCnt ) << ",";
}

void structStats::concreetHeader( ostream& os ) const {
  os << "noun_conc_strict_p,noun_conc_strict_r,noun_conc_strict_d,";
  os << "noun_conc_broad_p,noun_conc_broad_r,noun_conc_broad_d,";
  os << "adj_conc_strict_p,adj_conc_strict_r,adj_conc_strict_d,";
  os << "adj_conc_broad_p,adj_conc_broad_r,adj_conc_broad_d,";
}

void structStats::concreetToCSV( ostream& os ) const {
  os << ratio( strictConcreteNounCnt, nounCnt+nameCnt ) << ",";
  os << ratio( strictConcreteNounCnt, broadAbstractNounCnt ) << ",";
  os << density( strictConcreteNounCnt, wordCnt ) << ",";
  os << ratio( broadConcreteNounCnt, nounCnt+nameCnt ) << ",";
  os << ratio( broadConcreteNounCnt, strictAbstractNounCnt ) << ",";
  os << density( broadConcreteNounCnt, wordCnt ) << ",";
  os << ratio( strictConcreteAdjCnt, adjCnt ) << ",";
  os << ratio( strictConcreteAdjCnt, broadAbstractAdjCnt ) << ",";
  os << density( strictConcreteAdjCnt, wordCnt ) << ",";
  os << ratio( broadConcreteAdjCnt, adjCnt ) << ",";
  os << ratio( broadConcreteAdjCnt, strictAbstractAdjCnt ) << ",";
  os << density( broadConcreteAdjCnt, wordCnt ) << ",";
}


void structStats::persoonlijkheidHeader( ostream& os ) const {
  os << "pers_ref_d,pers_pron_1,pers_pron_2,pers_pron3,pers_pron,"
     << "names_p,names_r,names_d,"
     << "pers_names_p, pers_names_d, loc_names_d, org_names_d, prod_names_d, event_names_d,"
     << "action_verbs_p,action_verbs_d,state_verbs_p,state_verbs_d,"
     << "process_verbs_p,process_verbs_d,human_nouns_p,human_nouns_d,"
     << "emo_adjs_p,emo_adjs_d,imperatives_p,imperatives_d,"
     << "questions_p,questions_d,";
}

void structStats::persoonlijkheidToCSV( ostream& os ) const {
  os << density( persRefCnt, wordCnt ) << ",";
  os << density( pron1Cnt, wordCnt ) << ",";
  os << density( pron2Cnt, wordCnt ) << ",";
  os << density( pron3Cnt, wordCnt ) << ",";
  os << density( pron1Cnt+pron2Cnt+pron3Cnt, wordCnt ) << ",";

  os << ratio( nameCnt, nounCnt ) << ",";
  os << ratio( nameCnt, (wordCnt - nameCnt) ) << ",";
  os << density( nameCnt, wordCnt ) << ",";

  int val = at( ners, PER_B );
  os << ratio( val, nameCnt ) << ",";
  os << density( val, wordCnt ) << ",";
  val = at( ners, LOC_B );
  os << density( val, wordCnt ) << ",";
  val = at( ners, ORG_B );
  os << density( val, wordCnt ) << ",";
  val = at( ners, PRO_B );
  os << density( val, wordCnt ) << ",";
  val = at( ners, EVE_B );
  os << density( val, wordCnt ) << ",";

  os << ratio( actionCnt, verbCnt ) << ",";
  os << density( actionCnt, wordCnt) << ",";
  os << ratio( stateCnt, verbCnt ) << ",";
  os << density( stateCnt, wordCnt ) << ",";
  os << ratio( processCnt, verbCnt ) << ",";
  os << density( processCnt, wordCnt ) << ",";
  os << ratio( humanCnt, nounCnt ) << ",";
  os << density( humanCnt, wordCnt ) << ",";
  os << ratio( emoCnt, adjCnt ) << ",";
  os << density( emoCnt, wordCnt ) << ",";
  os << ratio( impCnt, sentCnt ) << ",";
  os << density( impCnt, wordCnt ) << ",";
  os << ratio( questCnt, sentCnt ) << ",";
  os << density( questCnt, wordCnt ) << ",";
}

void structStats::wordSortHeader( ostream& os ) const {
  os << "adj,vg,vnw,lid,vz,bijw,tw,noun,verb,interjections,spec,let,"
     << "afk_tot,afk_gen,afk_int,afk_jur,afk_med,afk_ond,afk_pol,afk_ov,afk_zorg,";
}

void structStats::wordSortToCSV( ostream& os ) const {
  os << density(adjCnt, wordCnt ) << ","
     << density(vgCnt, wordCnt ) << ","
     << density(vnwCnt, wordCnt ) << ","
     << density(lidCnt, wordCnt ) << ","
     << density(vzCnt, wordCnt ) << ","
     << density(bwCnt, wordCnt ) << ","
     << density(twCnt, wordCnt ) << ","
     << density(nounCnt, wordCnt ) << ","
     << density(verbCnt, wordCnt ) << ","
     << density(tswCnt, wordCnt ) << ","
     << density(specCnt, wordCnt ) << ","
     << density(letCnt, wordCnt ) << ",";
  int pola = at( afks, OVERHEID_A );
  int jura = at( afks, JURIDISCH_A );
  int onda = at( afks, ONDERWIJS_A );
  int meda = at( afks, MEDIA_A );
  int gena = at( afks, GENERIEK_A );
  int ova = at( afks, OVERIGE_A );
  int zorga = at( afks, ZORG_A );
  int inta = at( afks, INTERNATIONAAL_A );
  os << density( gena+inta+jura+meda+onda+pola+ova+zorga, wordCnt ) << ","
     << density( gena, wordCnt ) << ","
     << density( inta, wordCnt ) << ","
     << density( jura, wordCnt ) << ","
     << density( meda, wordCnt ) << ","
     << density( onda, wordCnt ) << ","
     << density(  pola, wordCnt ) << ","
     << density( ova, wordCnt ) << ","
     << density( zorga, wordCnt ) << ",";
}

void structStats::miscHeader( ostream& os ) const {
  os << "VZ_d, VZ_g, present_verbs_r,present_verbs_d,modals_d_,modals_g,"
     << "time_verbs_d,time_verbs_g,copula_d,copula_g,"
     << "archaics,vol_deelw_d,vol_deelw_g,"
     << "onvol_deelw_d,onvol_deelw_g,infin_d,infin_g,"
     << "wopr_logprob,wopr_entropy,wopr_perplexity,";
}

void structStats::miscToCSV( ostream& os ) const {
  os << density( prepExprCnt, wordCnt ) << ",";
  os << ratio( prepExprCnt, sentCnt ) << ",";
  os << ratio( presentCnt, pastCnt ) << ",";
  os  << density( presentCnt, wordCnt ) << ","
      << density( modalCnt, wordCnt ) << ",";
  os << ratio( modalCnt, (presentCnt + pastCnt) ) << ",";
  os << density( timeCnt, wordCnt ) << ",";
  os << ratio( timeCnt, (presentCnt + pastCnt) ) << ",";
  os << density( koppelCnt, wordCnt ) << ",";
  os << ratio( koppelCnt, presentCnt + pastCnt ) << ",";
  os << density( archaicsCnt, wordCnt ) << ","
     << density( vdCnt, wordCnt ) << ",";
  os << ratio( vdCnt, (pastCnt + presentCnt) ) << ",";
  os << density( odCnt, wordCnt ) << ",";
  os << ratio( odCnt, pastCnt + presentCnt ) << ",";
  os << density( infCnt, wordCnt ) << ",";
  os << ratio( infCnt, (pastCnt + presentCnt) ) << ",";
  os << ratio( avg_prob10, sentCnt ) << ",";
  os << ratio( entropy, sentCnt ) << ",";
  os << ratio( perplexity, sentCnt ) << ",";
}

vector<const wordStats*> structStats::collectWords() const {
  vector<const wordStats*> result;
  vector<basicStats *>::const_iterator it = sv.begin();
  while ( it != sv.end() ){
    vector<const wordStats*> tmp = (*it)->collectWords();
    result.insert( result.end(), tmp.begin(), tmp.end() );
    ++it;
  }
  return result;
}

//#define DEBUG_LSA

void structStats::resolveLSA( const map<string,double>& LSA_dists ){
  if ( sv.size() < 1 )
    return;

  calculate_LSA_summary();
  double suc = 0;
  double net = 0;
  double ctx = 0;
  size_t node_count = 0;
  for ( size_t i=0; i < sv.size()-1; ++i ){
    double context = 0.0;
    for ( size_t j=0; j < sv.size(); ++j ){
      if ( j == i )
	continue;
      string word1 = sv[i]->id;
      string word2 = sv[j]->id;
      string call = word1 + "<==>" + word2;
      map<string,double>::const_iterator it = LSA_dists.find(call);
      if ( it != LSA_dists.end() ){
	context += it->second;
      }
    }
    sv[i]->setLSAcontext(context/(sv.size()-1));
    ctx += context;
    for ( size_t j=i+1; j < sv.size(); ++j ){
      ++node_count;
      string word1 = sv[i]->id;
      string word2 = sv[j]->id;
      string call = word1 + "<==>" + word2;
      double result = 0;
      map<string,double>::const_iterator it = LSA_dists.find(call);
      if ( it != LSA_dists.end() ){
	result = it->second;
	if ( j == i+1 ){
	  sv[i]->setLSAsuc(result);
	  suc += result;
	}
	net += result;
      }
#ifdef DEBUG_LSA
      LOG << "LSA: doc lookup '" << call << "' ==> " << result << endl;
#endif
    }
  }
#ifdef DEBUG_LSA
  LOG << "LSA-doc-NET sum = " << net << ", node count = " << node_count << endl;
#endif
  suc = suc/sv.size();
  net = net/node_count;
  ctx = ctx/sv.size();
#ifdef DEBUG_LSA
  LOG << "LSA-suc result = " << suc << endl;
  LOG << "LSA-NET result = " << net << endl;
  LOG << "LSA-ctx result = " << ctx << endl;
#endif
  setLSAvalues( suc, net, ctx );
}

void structStats::calculate_LSA_summary(){
  double word_suc=0;
  double word_net=0;
  double sent_suc=0;
  double sent_net=0;
  double sent_ctx=0;
  double par_suc=0;
  double par_net=0;
  double par_ctx=0;
  size_t size = sv.size();
  for ( size_t i=0; i != size; ++i ){
    structStats *ps = dynamic_cast<structStats*>(sv[i]);
    if ( ps->lsa_word_suc != NA ){
      word_suc += ps->lsa_word_suc;
    }
    if ( ps->lsa_word_net != NA ){
      word_net += ps->lsa_word_net;
    }
    if ( ps->lsa_sent_suc != NA ){
      sent_suc += ps->lsa_sent_suc;
    }
    if ( ps->lsa_sent_net != NA ){
      sent_net += ps->lsa_sent_net;
    }
    if ( ps->lsa_sent_ctx != NA ){
      sent_ctx += ps->lsa_sent_ctx;
    }
    if ( ps->lsa_par_suc != NA ){
      par_suc += ps->lsa_par_suc;
    }
    if ( ps->lsa_par_net != NA ){
      par_net += ps->lsa_par_net;
    }
    if ( ps->lsa_par_ctx != NA ){
      par_ctx += ps->lsa_par_ctx;
    }
  }
  if ( word_suc > 0 ){
    lsa_word_suc = word_suc/size;
  }
  if ( word_net > 0 ){
    lsa_word_net = word_net/size;
  }
  if ( sent_suc > 0 ){
    lsa_sent_suc = sent_suc/size;
  }
  if ( sent_net > 0 ){
    lsa_sent_net = sent_net/size;
  }
  if ( sent_ctx > 0 ){
    lsa_sent_ctx = sent_ctx/size;
  }
  if ( par_suc > 0 ){
    lsa_par_suc = par_suc/size;
  }
  if ( par_net > 0 ){
    lsa_par_net = par_net/size;
  }
  if ( par_ctx > 0 ){
    lsa_par_ctx = par_ctx/size;
  }
}

struct sentStats : public structStats {
  sentStats( Sentence *, const sentStats*, const map<string,double>& );
  bool isSentence() const { return true; };
  void resolveConnectives();
  void setLSAvalues( double, double, double = 0 );
  void resolveLSA( const map<string,double>& );
  void resolveMultiWordAfks();
  void incrementConnCnt( ConnType );
  void addMetrics( ) const;
  bool checkAls( size_t );
  ConnType checkMultiConnectives( const string& );
  void resolvePrepExpr();
};

void sentStats::setLSAvalues( double suc, double net, double ctx ){
  if ( suc > 0 )
    lsa_word_suc = suc;
  if ( net > 0 )
    lsa_word_net = net;
  if ( ctx > 0 )
    throw logic_error("context cannot be !=0 for sentstats");
}

void fill_word_lemma_buffers( const sentStats* ss,
			      vector<string>& wv,
			      vector<string>& lv ){
  vector<basicStats*> bv = ss->sv;
  for ( size_t i=0; i < bv.size(); ++i ){
    wordStats *w = dynamic_cast<wordStats*>(bv[i]);
    if ( w->isOverlapCandidate() ){
      wv.push_back( w->l_word );
      lv.push_back( w->l_lemma );
    }
  }
}

NerProp lookupNer( const Word *w, const Sentence * s ){
  NerProp result = NONER;
  vector<Entity*> v = s->select<Entity>(frog_ner_set);
  for ( size_t i=0; i < v.size(); ++i ){
    FoliaElement *e = v[i];
    for ( size_t j=0; j < e->size(); ++j ){
      if ( e->index(j) == w ){
	//	cerr << "hit " << e->index(j) << " in " << v[i] << endl;
	string cls = v[i]->cls();
	if ( cls == "org" ){
	  if ( j == 0 )
	    result = ORG_B;
	  else
	    result = ORG_I;
	}
	else if ( cls == "eve" ){
	  if ( j == 0 )
	    result = EVE_B;
	  else
	    result = EVE_I;
	}
	else if ( cls == "loc" ){
	  if ( j == 0 )
	    result = LOC_B;
	  else
	    result = LOC_I;
	}
	else if ( cls == "misc" ){
	  if ( j == 0 )
	    result = MISC_B;
	  else
	    result = MISC_I;
	}
	else if ( cls == "per" ){
	  if ( j == 0 )
	    result = PER_B;
	  else
	    result = PER_I;
	}
	else if ( cls == "pro" ){
	  if ( j == 0 )
	    result = PRO_B;
	  else
	    result = PRO_I;
	}
	else {
	  throw ValueError( "unknown NER class: " + cls );
	}
      }
    }
  }
  return result;
}

void np_length( Sentence *s, int& npcount, int& indefcount, int& size ) {
  vector<Chunk *> cv = s->select<Chunk>();
  size = 0 ;
  for( size_t i=0; i < cv.size(); ++i ){
    if ( cv[i]->cls() == "NP" ){
      ++npcount;
      size += cv[i]->size();
      FoliaElement *det = cv[i]->index(0);
      if ( det ){
	vector<PosAnnotation*> posV = det->select<PosAnnotation>(frog_pos_set);
	if ( posV.size() != 1 )
	  throw ValueError( "word doesn't have Frog POS tag info" );
	if ( posV[0]->feat("head") == "LID" ){
	  if ( det->text() == "een" )
	    ++indefcount;
	}
      }
    }
  }
}

bool sentStats::checkAls( size_t index ){
  static string compAlsList[] = { "net", "evenmin", "zo", "zomin" };
  static set<string> compAlsSet( compAlsList,
				 compAlsList + sizeof(compAlsList)/sizeof(string) );
  static string opsomAlsList[] = { "zowel" };
  static set<string> opsomAlsSet( opsomAlsList,
				  opsomAlsList + sizeof(opsomAlsList)/sizeof(string) );

  string als = sv[index]->ltext();
  if ( als == "als" ){
    if ( index == 0 ){
      // eerste woord, terugkijken kan dus niet
      sv[0]->setConnType( CAUSAAL );
    }
    else {
      for ( size_t i = index-1; i+1 != 0; --i ){
	string word = sv[i]->ltext();
	if ( compAlsSet.find( word ) != compAlsSet.end() ){
	  // kijk naar "evenmin ... als" constructies
	  sv[i]->setConnType( COMPARATIEF );
	  sv[index]->setConnType( COMPARATIEF );
	  //	cerr << "ALS comparatief:" << word << endl;
	  return true;
	}
	else if ( opsomAlsSet.find( word ) != opsomAlsSet.end() ){
	  // kijk naar "zowel ... als" constructies
	  sv[i]->setConnType( OPSOMMEND );
	  sv[index]->setConnType( OPSOMMEND );
	  //	cerr << "ALS opsommend:" << word << endl;
	  return true;
	}
      }
      if ( sv[index]->postag() == CGN::VG ){
	if ( sv[index-1]->postag() == CGN::ADJ ){
	  // "groter als"
	  //	cerr << "ALS comparatief: ADJ: " << sv[index-1]->text() << endl;
	  sv[index]->setConnType( COMPARATIEF );
	}
	else {
	  //	cerr << "ALS causaal: " << sv[index-1]->text() << endl;
	  sv[index]->setConnType( CAUSAAL );
	}
	return true;
      }
    }
    if ( index < sv.size() &&
	 sv[index+1]->postag() == CGN::TW ){
      // "als eerste" "als dertigste"
      sv[index]->setConnType( COMPARATIEF );
      return true;
    }
  }
  return false;
}

ConnType sentStats::checkMultiConnectives( const string& mword ){
  ConnType conn = NOCONN;
  if ( settings.multi_temporals.find( mword ) != settings.multi_temporals.end() ){
    conn = TEMPOREEL;
  }
  else if ( settings.multi_opsommers.find( mword ) != settings.multi_opsommers.end() ){
    conn = OPSOMMEND;
  }
  else if ( settings.multi_contrast.find( mword ) != settings.multi_contrast.end() ){
    conn = CONTRASTIEF;
  }
  else if ( settings.multi_compars.find( mword ) != settings.multi_compars.end() ){
    conn = COMPARATIEF;
  }
  else if ( settings.multi_causals.find( mword ) != settings.multi_causals.end() ){
    conn = CAUSAAL;
  }
  //  cerr << "multi-conn " << mword << " = " << conn << endl;
  return conn;
}

void sentStats::incrementConnCnt( ConnType t ){
  switch ( t ){
  case TEMPOREEL:
    tempConnCnt++;
    break;
  case OPSOMMEND:
    opsomConnCnt++;
    break;
  case CONTRASTIEF:
    contConnCnt++;
    break;
  case COMPARATIEF:
    compConnCnt++;
    break;
  case CAUSAAL:
    break;
  default:
    break;
  }
}

void sentStats::resolveConnectives(){
  if ( sv.size() > 1 ){
    for ( size_t i=0; i < sv.size()-2; ++i ){
      string word = sv[i]->ltext();
      string multiword2 = word + " " + sv[i+1]->ltext();
      if ( !checkAls( i ) ){
	// "als" is speciaal als het matcht met eerdere woorden.
	// (evenmin ... als) (zowel ... als ) etc.
	// In dat geval niet meer zoeken naar "als ..."
	//      cerr << "zoek op " << multiword2 << endl;
	ConnType conn = checkMultiConnectives( multiword2 );
	if ( conn != NOCONN ){
	  sv[i]->setMultiConn();
	  sv[i+1]->setMultiConn();
	  sv[i]->setConnType( conn );
	  sv[i+1]->setConnType( NOCONN );
	}
      }
      if ( negatives_long.find( multiword2 ) != negatives_long.end() ){
	propNegCnt++;
      }
      string multiword3 = multiword2 + " " + sv[i+2]->ltext();
      //      cerr << "zoek op " << multiword3 << endl;
      ConnType conn = checkMultiConnectives( multiword3 );
      if ( conn != NOCONN ){
	sv[i]->setMultiConn();
	sv[i+1]->setMultiConn();
	sv[i+2]->setMultiConn();
	sv[i]->setConnType( conn );
	sv[i+1]->setConnType( NOCONN );
	sv[i+2]->setConnType( NOCONN );
      }
      if ( negatives_long.find( multiword3 ) != negatives_long.end() )
	propNegCnt++;
    }
    // don't forget the last 2 words
    string multiword2 = sv[sv.size()-2]->ltext() + " "
      + sv[sv.size()-1]->ltext();
    //    cerr << "zoek op " << multiword2 << endl;
    ConnType conn = checkMultiConnectives( multiword2 );
    if ( conn != NOCONN ){
      sv[sv.size()-2]->setMultiConn();
      sv[sv.size()-1]->setMultiConn();
      sv[sv.size()-2]->setConnType( conn );
      sv[sv.size()-1]->setConnType( NOCONN );
    }
    if ( negatives_long.find( multiword2 ) != negatives_long.end() ){
      propNegCnt++;
    }
  }
  for ( size_t i=0; i < sv.size(); ++i ){
    switch( sv[i]->getConnType() ){
    case TEMPOREEL:
      tempConnCnt++;
      break;
    case OPSOMMEND:
      opsomConnCnt++;
      break;
    case CONTRASTIEF:
      contConnCnt++;
      break;
    case COMPARATIEF:
      compConnCnt++;
      break;
    case CAUSAAL:
      causeConnCnt++;
      break;
    default:
      break;
    }
  }
}

//#define DEBUG_LSA

void sentStats::resolveLSA( const map<string,double>& LSAword_dists ){
  if ( sv.size() < 1 )
    return;

  int lets = 0;
  double suc = 0;
  double net = 0;
  size_t node_count = 0;
  for ( size_t i=0; i < sv.size(); ++i ){
    double context = 0.0;
    for ( size_t j=0; j < sv.size(); ++j ){
      if ( j == i )
	continue;
      string word1 = sv[i]->ltext();
      string word2 = sv[j]->ltext();
      string call = word1 + "\t" + word2;
      map<string,double>::const_iterator it = LSAword_dists.find(call);
      if ( it != LSAword_dists.end() ){
	context += it->second;
      }
    }
    sv[i]->setLSAcontext(context/(sv.size()-1));
    for ( size_t j=i+1; j < sv.size(); ++j ){
      if ( sv[i]->wordProperty() == ISLET ){
	continue;
      }
      if ( sv[j]->wordProperty() == ISLET ){
	if ( i == 0 )
	  ++lets;
	continue;
      }
      ++node_count;
      string word1 = sv[i]->ltext();
      string word2 = sv[j]->ltext();
      string call = word1 + "\t" + word2;
      double result = 0;
      map<string,double>::const_iterator it = LSAword_dists.find(call);
      if ( it != LSAword_dists.end() ){
	result = it->second;
	if ( j == i+1 ){
	  sv[i]->setLSAsuc(result);
	  suc += result;
	}
	net += result;
      }
#ifdef DEBUG_LSA
      LOG << "LSA: lookup '" << call << "' ==> " << result << endl;
#endif
    }
  }
#ifdef DEBUG_LSA
  LOG << "LSA-NET sum = " << lsa_net << ", node count = " << node_count << endl;
#endif
  suc = suc/(sv.size()-lets);
  net = net/node_count;
#ifdef DEBUG_LSA
  LOG << "LSA-SUC result = " << suc << endl;
  LOG << "LSA-NET result = " << net << endl;
  LOG << "LETS = " << lets << endl;
#endif
  setLSAvalues( suc, net, 0 );
}

void sentStats::resolveMultiWordAfks(){
  if ( sv.size() > 1 ){
    for ( size_t i=0; i < sv.size()-2; ++i ){
      string word = sv[i]->text();
      string multiword2 = word + " " + sv[i+1]->text();
      string multiword3 = multiword2 + " " + sv[i+2]->text();
      AfkType at = NO_A;
      map<string,AfkType>::const_iterator sit
	= settings.afkos.find( multiword3 );
      if ( sit == settings.afkos.end() ){
	sit = settings.afkos.find( multiword2 );
      }
      else {
	cerr << "FOUND a 3-word AFK: '" << multiword3 << "'" << endl;
      }
      if ( sit != settings.afkos.end() ){
	cerr << "FOUND a 2-word AFK: '" << multiword2 << "'" << endl;
	at = sit->second;
      }
      if ( at != NO_A ){
	++afks[at];
      }
    }
    // don't forget the last 2 words
    string multiword2 = sv[sv.size()-2]->text() + " " + sv[sv.size()-1]->text();
    map<string,AfkType>::const_iterator sit
      = settings.afkos.find( multiword2 );
    if ( sit != settings.afkos.end() ){
      cerr << "FOUND a 2-word AFK: '" << multiword2 << "'" << endl;
      AfkType at = sit->second;
      ++afks[at];
    }
  }
}

void sentStats::resolvePrepExpr(){
  if ( sv.size() > 2 ){
    for ( size_t i=0; i < sv.size()-1; ++i ){
      string word = sv[i]->ltext();
      string mw2 = word + " " + sv[i+1]->ltext();
      if ( settings.vzexpr2.find( mw2 ) != settings.vzexpr2.end() ){
	++prepExprCnt;
	i += 1;
	continue;
      }
      if ( i < sv.size() - 2 ){
	string mw3 = mw2 + " " + sv[i+2]->ltext();
	if ( settings.vzexpr3.find( mw3 ) != settings.vzexpr3.end() ){
	  ++prepExprCnt;
	  i += 2;
	  continue;
	}
	if ( i < sv.size() - 3 ){
	  string mw4 = mw3 + " " + sv[i+3]->ltext();
	  if ( settings.vzexpr4.find( mw4 ) != settings.vzexpr4.end() ){
	    ++prepExprCnt;
	    i += 3;
	    continue;
	  }
	}
      }
    }
  }
}

void orderWopr( const string& txt, vector<double>& wordProbsV,
		double& sentProb, double& entropy, double& perplexity ){
  string host = config.lookUp( "host", "wopr" );
  string port = config.lookUp( "port", "wopr" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    LOG << "failed to open Wopr connection: "<< host << ":" << port << endl;
    LOG << "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  LOG << "calling Wopr" << endl;
  client.write( txt + "\n\n" );
  string result;
  string s;
  while ( client.read(s) ){
    result += s + "\n";
  }
  DBG << "received data [" << result << "]" << endl;
  Document *doc = 0;
  if ( !result.empty() && result.size() > 10 ){
    DBG << "start FoLiA parsing" << endl;
    doc = new Document();
    try {
      doc->readFromString( result );
      DBG << "finished parsing" << endl;
      vector<Word*> wv = doc->words();
      if ( wv.size() !=  wordProbsV.size() ){
	LOG << "unforseen mismatch between de number of words returned by WOPR"
	    << endl << " and the number of words in the input sentence. "
	    << endl;
	return;
      }
      for ( size_t i=0; i < wv.size(); ++i ){
	vector<MetricAnnotation*> mv = wv[i]->select<MetricAnnotation>();
	if ( mv.size() > 0 ){
	  for ( size_t j=0; j < mv.size(); ++j ){
	    if ( mv[j]->cls() == "lprob10" ){
	      wordProbsV[i] = TiCC::stringTo<double>( mv[j]->feat("value") );
	    }
	  }
	}
      }
      vector<Sentence*> sv = doc->sentences();
      if ( sv.size() != 1 ){
	throw logic_error( "The document returned by WOPR contains > 1 Sentence" );
	return;
      }
      vector<MetricAnnotation*> mv = sv[0]->select<MetricAnnotation>();
      if ( mv.size() > 0 ){
	for ( size_t j=0; j < mv.size(); ++j ){
	  if ( mv[j]->cls() == "avg_prob10" ){
	    string val = mv[j]->feat("value");
	    if ( val != "nan" ){
	      sentProb = TiCC::stringTo<double>( val );
	    }
	  }
	  else if ( mv[j]->cls() == "entropy" ){
	    string val = mv[j]->feat("value");
	    if ( val != "nan" ){
	      entropy = TiCC::stringTo<double>( val );
	    }
	  }
	  else if ( mv[j]->cls() == "perplexity" ){
	    string val = mv[j]->feat("value");
	    if ( val != "nan" ){
	      perplexity = TiCC::stringTo<double>( val );
	    }
	  }
	}
      }
    }
    catch ( std::exception& e ){
      LOG << "FoLiaParsing failed:" << endl
	  << e.what() << endl;
    }
  }
  LOG << "finished Wopr" << endl;
}

xmlDoc *AlpinoServerParse( Sentence *);

sentStats::sentStats( Sentence *s, const sentStats* pred,
		      const map<string,double>& LSAword_dists ):
  structStats( s, "sent" ){
  text = UnicodeToUTF8( s->toktext() );
  LOG << "analyse tokenized sentence=" << text << endl;
  vector<Word*> w = s->words();
  vector<double> woprProbsV(w.size(),NA);
  double sentProb = NA;
  double sentEntropy = NA;
  double sentPerplexity = NA;
  xmlDoc *alpDoc = 0;
  set<size_t> puncts;
  parseFailCnt = -1; // not parsed (yet)
#pragma omp parallel sections
  {
#pragma omp section
    {
      if ( settings.doAlpino || settings.doAlpinoServer ){
	if ( settings.doAlpinoServer ){
	  LOG << "calling Alpino Server" << endl;
	  alpDoc = AlpinoServerParse( s );
	  if ( !alpDoc ){
	    cerr << "alpino parser failed!" << endl;
	  }
	  LOG << "done with Alpino Server" << endl;
	}
	else if ( settings.doAlpino ){
	  LOG << "calling Alpino parser" << endl;
	  alpDoc = AlpinoParse( s, workdir_name );
	  if ( !alpDoc ){
	    cerr << "alpino parser failed!" << endl;
	  }
	  LOG << "done with Alpino parser" << endl;
	}
	if ( alpDoc ){
	  parseFailCnt = 0; // OK
	  for( size_t i=0; i < w.size(); ++i ){
	    vector<PosAnnotation*> posV = w[i]->select<PosAnnotation>(frog_pos_set);
	    if ( posV.size() != 1 )
	      throw ValueError( "word doesn't have Frog POS tag info" );
	    PosAnnotation *pa = posV[0];
	    string posHead = pa->feat("head");
	    if ( posHead == "LET" ){
	      puncts.insert( i );
	    }
	  }
	  dLevel = get_d_level( s, alpDoc );
	  if ( dLevel > 4 )
	    dLevel_gt4 = 1;
	  mod_stats( alpDoc, vcModCnt,
		     adjNpModCnt, npModCnt );
	}
	else {
	  parseFailCnt = 1; // failed
	}
      }
    } // omp section

#pragma omp section
    {
      if ( settings.doWopr ){
	orderWopr( text, woprProbsV, sentProb, sentEntropy, sentPerplexity );
      }
    } // omp section
  } // omp sections

  if ( parseFailCnt == 1 ){
    // glorious fail
    return;
  }
  sentCnt = 1; // so only count the sentence when not failed
  if ( sentProb != -99 ){
    avg_prob10 = sentProb;
  }
  entropy = sentEntropy;
  perplexity = sentPerplexity;
  //  cerr << "PUNCTS " << puncts << endl;
  bool question = false;
  vector<string> wordbuffer;
  vector<string> lemmabuffer;
  if ( pred ){
    fill_word_lemma_buffers( pred, wordbuffer, lemmabuffer );
#ifdef DEBUG_OL
    cerr << "call sentenceOverlap, wordbuffer " << wordbuffer << endl;
    cerr << "call sentenceOverlap, lemmabuffer " << lemmabuffer << endl;
#endif
  }
  for ( size_t i=0; i < w.size(); ++i ){
    xmlNode *alpWord = 0;
    if ( alpDoc ){
      alpWord = getAlpWordNode( alpDoc, w[i] );
    }
    wordStats *ws = new wordStats( w[i], alpWord, puncts );
    if ( woprProbsV[i] != -99 )
      ws->logprob10 = woprProbsV[i];
    if ( pred ){
      ws->getSentenceOverlap( wordbuffer, lemmabuffer );
    }

    if ( ws->lemma[ws->lemma.length()-1] == '?' ){
      question = true;
    }
    if ( ws->prop == ISLET ){
      letCnt++;
      sv.push_back( ws );
      continue;
    }
    else {
      NerProp ner = lookupNer( w[i], s );
      ws->nerProp = ner;
      switch( ner ){
      case LOC_B:
      case EVE_B:
      case MISC_B:
      case ORG_B:
      case PER_B:
      case PRO_B:
	ners[ner]++;
	break;
      default:
	;
      }
      wordCnt++;
      heads[ws->tag]++;
      if ( ws->afkType != NO_A ){
	++afks[ws->afkType];
      }
      wordOverlapCnt += ws->wordOverlapCnt;
      lemmaOverlapCnt += ws->lemmaOverlapCnt;
      charCnt += ws->charCnt;
      charCntExNames += ws->charCntExNames;
      morphCnt += ws->morphCnt;
      morphCntExNames += ws->morphCntExNames;
      unique_words[ws->l_word] += 1;
      unique_lemmas[ws->lemma] += 1;
      aggregate( distances, ws->distances );
      if ( ws->compPartCnt > 0 )
	++compCnt;
      compPartCnt += ws->compPartCnt;
      word_freq += ws->word_freq;
      lemma_freq += ws->lemma_freq;
      if ( ws->prop != ISNAME ){
	word_freq_n += ws->word_freq;
	lemma_freq_n += ws->lemma_freq;
      }
      switch ( ws->prop ){
      case ISNAME:
	nameCnt++;
	unique_names[ws->l_word] +=1;
	break;
      case ISVD:
	vdCnt++;
	break;
      case ISINF:
	infCnt++;
	break;
      case ISOD:
	odCnt++;
	break;
      case ISPVVERL:
	pastCnt++;
	break;
      case ISPVTGW:
	presentCnt++;
	break;
      case ISSUBJ:
	subjonctCnt++;
	break;
      case ISPPRON1:
	pron1Cnt++;
      case ISPPRON2:
	pron2Cnt++;
      case ISPPRON3:
	pron3Cnt++;
      default:
	;// ignore JUSTAWORD and ISAANW
      }
      if ( ws->wwform == PASSIVE_VERB )
	passiveCnt++;
      if ( ws->wwform == MODAL_VERB )
	modalCnt++;
      if ( ws->wwform == TIME_VERB )
	timeCnt++;
      if ( ws->wwform == COPULA )
	koppelCnt++;
      if ( ws->isPersRef )
	persRefCnt++;
      if ( ws->isPronRef )
	pronRefCnt++;
      if ( ws->archaic )
	archaicsCnt++;
      if ( ws->isContent ){
	contentCnt++;
	unique_contents[ws->l_word] +=1;
      }
      if ( ws->isNominal )
	nominalCnt++;
      switch ( ws->tag ){
      case CGN::N:
	nounCnt++;
	break;
      case CGN::ADJ:
	adjCnt++;
	break;
      case CGN::WW:
	verbCnt++;
	break;
      case CGN::VG:
	vgCnt++;
	break;
      case CGN::TSW:
	tswCnt++;
	break;
      case CGN::LET:
	letCnt++;
	break;
      case CGN::SPEC:
	specCnt++;
	break;
      case CGN::BW:
	bwCnt++;
	break;
      case CGN::VNW:
	vnwCnt++;
	break;
      case CGN::LID:
	lidCnt++;
	break;
      case CGN::TW:
	twCnt++;
	break;
      case CGN::VZ:
	vzCnt++;
	break;
      default:
	break;
      }
      if ( ws->isOnder )
	onderCnt++;
      if ( ws->isImperative )
	impCnt++;
      if ( ws->isBetr )
	betrCnt++;
      if ( ws->isPropNeg )
	propNegCnt++;
      if ( ws->isMorphNeg )
	morphNegCnt++;
      if ( ws->f50 )
	f50Cnt++;
      if ( ws->f65 )
	f65Cnt++;
      if ( ws->f77 )
	f77Cnt++;
      if ( ws->f80 )
	f80Cnt++;
      switch ( ws->top_freq ){
	// NO BREAKS
      case top1000:
	++top1000Cnt;
      case top2000:
	++top2000Cnt;
      case top3000:
	++top3000Cnt;
      case top5000:
	++top5000Cnt;
      case top10000:
	++top10000Cnt;
      case top20000:
	++top20000Cnt;
      default:
	break;
      }
      switch ( ws->sem_type ){
      case CONCRETE_HUMAN_NOUN:
	humanCnt++;
	// fall throug
      case CONCRETE_NOUN:
	strictConcreteNounCnt++;
	broadConcreteNounCnt++;
	break;
      case CONCRETE_ADJ:
	strictConcreteAdjCnt++;
	broadConcreteAdjCnt++;
	break;
      case ABSTRACT_NOUN:
	strictAbstractNounCnt++;
	broadAbstractNounCnt++;
	break;
      case ABSTRACT_ADJ:
	strictAbstractAdjCnt++;
	broadAbstractAdjCnt++;
	break;
      case BROAD_NOUN:
	broadConcreteNounCnt++;
	broadAbstractNounCnt++;
	break;
      case BROAD_ADJ:
	broadConcreteAdjCnt++;
	broadAbstractAdjCnt++;
	break;
      case STATE:
	stateCnt++;
	break;
      case ACTION:
	actionCnt++;
	break;
      case PROCESS:
	processCnt++;
	break;
      case WEIRD:
	weirdCnt++;
	break;
      case EMO_ADJ:
	emoCnt++;
	broadConcreteAdjCnt++;
	broadAbstractAdjCnt++;
	break;
      default:
	;
      }
      sv.push_back( ws );
    }
  }
  if ( alpDoc ){
    xmlFreeDoc( alpDoc );
  }
  resolveConnectives();
  if ( settings.doLsa ){
    resolveLSA( LSAword_dists );
  }
  // Disabled for now
  //  resolveMultiWordAfks();
  resolvePrepExpr();
  if ( question )
    questCnt = 1;
  if ( (morphNegCnt + propNegCnt) > 1 )
    multiNegCnt = 1;
  if ( word_freq == 0 || contentCnt == 0 )
    word_freq_log = NA;
  else
    word_freq_log = log10( word_freq / contentCnt );
  if ( lemma_freq == 0 || contentCnt == 0 )
    lemma_freq_log = NA;
  else
    lemma_freq_log = log10( lemma_freq / contentCnt );
  if ( contentCnt == nameCnt || word_freq_n == 0 )
    word_freq_log_n = NA;
  else
    word_freq_log_n = log10( word_freq_n / (contentCnt-nameCnt) );
  if ( contentCnt == nameCnt || lemma_freq_n == 0 )
    lemma_freq_log_n = NA;
  else
    lemma_freq_log_n = log10( lemma_freq_n / (contentCnt-nameCnt) );
  np_length( s, npCnt, indefNpCnt, npSize );
}

void sentStats::addMetrics( ) const {
  structStats::addMetrics( );
  FoliaElement *el = folia_node;
  Document *doc = el->doc();
  if ( passiveCnt > 0 )
    addOneMetric( doc, el, "isPassive", "true" );
  if ( questCnt > 0 )
    addOneMetric( doc, el, "isQuestion", "true" );
  if ( impCnt > 0 )
    addOneMetric( doc, el, "isImperative", "true" );
}

struct parStats: public structStats {
  parStats( Paragraph *, const map<string,double>&,
	    const map<string,double>& );
  void addMetrics( ) const;
  void setLSAvalues( double, double, double );
};

parStats::parStats( Paragraph *p,
		    const map<string,double>& LSA_word_dists,
		    const map<string,double>& LSA_sent_dists ):
  structStats( p, "par" )
{
  sentCnt = 0;
  vector<Sentence*> sents = p->sentences();
  sentStats *prev = 0;
  for ( size_t i=0; i < sents.size(); ++i ){
    sentStats *ss = new sentStats( sents[i], prev, LSA_word_dists );
    prev = ss;
    merge( ss );
  }
  if ( settings.doLsa ){
    resolveLSA( LSA_sent_dists );
  }
  if ( word_freq == 0 || contentCnt == 0 )
    word_freq_log = NA;
  else
    word_freq_log = log10( word_freq /contentCnt );
  if ( contentCnt == nameCnt || word_freq_n == 0 )
    word_freq_log_n = NA;
  else
    word_freq_log_n = log10( word_freq_n / (contentCnt-nameCnt) );
  if ( lemma_freq == 0 || contentCnt == 0 )
    lemma_freq_log = NA;
  else
    lemma_freq_log = log10( lemma_freq / contentCnt );
  if ( contentCnt == nameCnt || lemma_freq_n == 0 )
    lemma_freq_log_n = NA;
  else
    lemma_freq_log_n = log10( lemma_freq_n / (contentCnt-nameCnt) );
}


void parStats::addMetrics( ) const {
  FoliaElement *el = folia_node;
  structStats::addMetrics( );
  addOneMetric( el->doc(), el,
		"sentence_count", toString(sentCnt) );
}

void parStats::setLSAvalues( double suc, double net, double ctx ){
  if ( suc > 0 )
    lsa_sent_suc = suc;
  if ( net > 0 )
    lsa_sent_net = net;
  if ( ctx > 0 )
    lsa_sent_ctx = ctx;
}

struct docStats : public structStats {
  docStats( Document * );
  bool isDocument() const { return true; };
  void toCSV( const string&, csvKind ) const;
  string rarity( int level ) const;
  void addMetrics( ) const;
  int word_overlapCnt() const { return doc_word_overlapCnt; };
  int lemma_overlapCnt() const { return doc_lemma_overlapCnt; };
  void calculate_doc_overlap();
  void setLSAvalues( double, double, double );
  void gather_LSA_word_info( Document * );
  void gather_LSA_doc_info( Document * );
  int doc_word_overlapCnt;
  int doc_lemma_overlapCnt;
  map<string,double> LSA_word_dists;
  map<string,double> LSA_sentence_dists;
  map<string,double> LSA_paragraph_dists;
};

void docStats::setLSAvalues( double suc, double net, double ctx ){
  if ( suc > 0 )
    lsa_par_suc = suc;
  if ( net > 0 )
    lsa_par_net = net;
  if ( ctx > 0 )
    lsa_par_ctx = ctx;
}

//#define DEBUG_DOL

void docStats::calculate_doc_overlap( ){
  vector<const wordStats*> wv2 = collectWords();
  if ( wv2.size() < settings.overlapSize )
    return;
  vector<string> wordbuffer;
  vector<string> lemmabuffer;
  for ( vector<const wordStats*>::const_iterator it = wv2.begin();
	it != wv2.end();
	++it ){
    if ( (*it)->wordProperty() == ISLET )
      continue;
    string l_word =  (*it)->ltext();
    string l_lemma = (*it)->llemma();
    if ( wordbuffer.size() >= settings.overlapSize ){
#ifdef DEBUG_DOL
      cerr << "Document overlap" << endl;
      cerr << "wordbuffer= " << wordbuffer << endl;
      cerr << "lemmabuffer= " << lemmabuffer << endl;
      cerr << "test overlap: << " << l_word << " " << l_lemma << endl;
#endif
      if ( (*it)->isOverlapCandidate() ){
#ifdef DEBUG_DOL
	int tmp = doc_word_overlapCnt;
#endif
	argument_overlap( l_word, wordbuffer, doc_word_overlapCnt );
#ifdef DEBUG_DOL
	if ( doc_word_overlapCnt > tmp ){
	  cerr << "word OVERLAP " << l_word << endl;
	}
#endif
#ifdef DEBUG_DOL
	tmp = doc_lemma_overlapCnt;
#endif
	argument_overlap( l_lemma, lemmabuffer, doc_lemma_overlapCnt );
#ifdef DEBUG_DOL
	if ( doc_lemma_overlapCnt > tmp ){
	  cerr << "lemma OVERLAP " << l_lemma << endl;
	}
#endif
      }
#ifdef DEBUG_DOL
      else {
	cerr << "geen kandidaat" << endl;
      }
#endif
      wordbuffer.erase(wordbuffer.begin());
      lemmabuffer.erase(lemmabuffer.begin());
    }
    wordbuffer.push_back( l_word );
    lemmabuffer.push_back( l_lemma );
  }
}


//#define DEBUG_LSA_SERVER

void docStats::gather_LSA_word_info( Document *doc ){
  string host = config.lookUp( "host", "lsa_words" );
  string port = config.lookUp( "port", "lsa_words" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    LOG << "failed to open LSA connection: "<< host << ":" << port << endl;
    LOG << "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  vector<Word*> wv = doc->words();
  set<string> bow;
  for ( size_t i=0; i < wv.size(); ++i ){
    UnicodeString us = wv[i]->text();
    us.toLower();
    string s = UnicodeToUTF8( us );
    bow.insert(s);
  }
  while ( !bow.empty() ){
    set<string>::iterator it = bow.begin();
    string word = *it;
    bow.erase( it );
    it = bow.begin();
    while ( it != bow.end() ) {
      string call = word + "\t" + *it;
      string rcall = *it + "\t" + word;
      if ( LSA_word_dists.find( call ) == LSA_word_dists.end() ){
#ifdef DEBUG_LSA_SERVER
	LOG << "calling LSA: '" << call << "'" << endl;
#endif
	client.write( call + "\r\n" );
	string s;
	if ( !client.read(s) ){
	  LOG << "LSA connection failed " << endl;
	  exit( EXIT_FAILURE );
	}
#ifdef DEBUG_LSA_SERVER
	LOG << "received data [" << s << "]" << endl;
#endif
	double result = 0;
	if ( !stringTo( s , result ) ){
	  LOG << "LSA result conversion failed: " << s << endl;
	  result = 0;
	}
#ifdef DEBUG_LSA_SERVER
	LOG << "LSA result: " << result << endl;
#endif
#ifdef DEBUG_LSA
	LOG << "LSA: '" << call << "' ==> " << result << endl;
#endif
	if ( result != 0 ){
	  LSA_word_dists[call] = result;
	  LSA_word_dists[rcall] = result;
	}
      }
      ++it;
    }
  }
}

#define DEBUG_LSA_SERVER

void docStats::gather_LSA_doc_info( Document *doc ){
  string host = config.lookUp( "host", "lsa_docs" );
  string port = config.lookUp( "port", "lsa_docs" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    LOG << "failed to open LSA connection: "<< host << ":" << port << endl;
    LOG << "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  vector<Paragraph*> pv = doc->paragraphs();
  map<string,string> norm_pv;
  map<string,string> norm_sv;
  for ( size_t p=0; p < pv.size(); ++p ){
    vector<Sentence*> sv = pv[p]->sentences();
    string norm_p;
    for ( size_t s=0; s < sv.size(); ++s ){
      vector<Word*> wv = sv[s]->words();
      set<string> bow;
      for ( size_t i=0; i < wv.size(); ++i ){
	UnicodeString us = wv[i]->text();
	us.toLower();
	string s = UnicodeToUTF8( us );
	bow.insert(s);
      }
      string norm_s;
      set<string>::iterator it = bow.begin();
      while ( it != bow.end() ) {
	norm_s += *it++ + " ";
      }
      norm_sv[sv[s]->id()] = norm_s;
      norm_p += norm_s;
    }
    norm_pv[pv[p]->id()] = norm_p;
  }
#ifdef DEBUG_LSA_SERVER
  LOG << "LSA doc Paragraaf results" << endl;
  for ( map<string,string>::const_iterator it = norm_pv.begin();
	it != norm_pv.end();
	++it ){
    LOG << "paragraaf " << it->first << endl;
    LOG << it->second << endl;
  }
  LOG << "en de Zinnen" << endl;
  for ( map<string,string>::const_iterator it = norm_sv.begin();
	it != norm_sv.end();
	++it ){
    LOG << "zin " <<  it->first << endl;
    LOG << it->second << endl;
  }
#endif

  for ( map<string,string>::const_iterator it1 = norm_pv.begin();
	it1 != norm_pv.end();
	++it1 ){
    for ( map<string,string>::const_iterator it2 = it1;
	  it2 != norm_pv.end();
	  ++it2 ){
      if ( it2 == it1 )
	continue;
      string index = it1->first + "<==>" + it2->first;
      string rindex = it2->first + "<==>" + it1->first;
      LOG << "LSA combine paragraaf " << index << endl;
      string call = it1->second + "\t" + it2->second;
      if ( LSA_paragraph_dists.find( index ) == LSA_paragraph_dists.end() ){
#ifdef DEBUG_LSA_SERVER
	LOG << "calling LSA docs: '" << call << "'" << endl;
#endif
	client.write( call + "\r\n" );
	string s;
	if ( !client.read(s) ){
	  LOG << "LSA connection failed " << endl;
	  exit( EXIT_FAILURE );
	}
#ifdef DEBUG_LSA_SERVER
	LOG << "received data [" << s << "]" << endl;
#endif
	double result = 0;
	if ( !stringTo( s , result ) ){
	  LOG << "LSA result conversion failed: " << s << endl;
	  result = 0;
	}
#ifdef DEBUG_LSA_SERVER
	LOG << "LSA result: " << result << endl;
#endif
#ifdef DEBUG_LSA
	LOG << "LSA: '" << index << "' ==> " << result << endl;
#endif
	if ( result != 0 ){
	  LSA_paragraph_dists[index] = result;
	  LSA_paragraph_dists[rindex] = result;
	}
      }
    }
  }
  for ( map<string,string>::const_iterator it1 = norm_sv.begin();
	it1 != norm_sv.end();
	++it1 ){
    for ( map<string,string>::const_iterator it2 = it1;
	  it2 != norm_sv.end();
	  ++it2 ){
      if ( it2 == it1 )
	continue;
      string index = it1->first + "<==>" + it2->first;
      string rindex = it2->first + "<==>" + it1->first;
      LOG << "LSA combine sentence " << index << endl;
      string call = it1->second + "\t" + it2->second;
      if ( LSA_sentence_dists.find( index ) == LSA_sentence_dists.end() ){
#ifdef DEBUG_LSA_SERVER
	LOG << "calling LSA docs: '" << call << "'" << endl;
#endif
	client.write( call + "\r\n" );
	string s;
	if ( !client.read(s) ){
	  LOG << "LSA connection failed " << endl;
	  exit( EXIT_FAILURE );
	}
#ifdef DEBUG_LSA_SERVER
	LOG << "received data [" << s << "]" << endl;
#endif
	double result = 0;
	if ( !stringTo( s , result ) ){
	  LOG << "LSA result conversion failed: " << s << endl;
	  result = 0;
	}
#ifdef DEBUG_LSA_SERVER
	LOG << "LSA result: " << result << endl;
#endif
#ifdef DEBUG_LSA
	LOG << "LSA: '" << index << "' ==> " << result << endl;
#endif
	if ( result != 0 ){
	  LSA_sentence_dists[index] = result;
	  LSA_sentence_dists[rindex] = result;
	}
      }
    }
  }
}

docStats::docStats( Document *doc ):
  structStats( 0, "document" ),
  doc_word_overlapCnt(0), doc_lemma_overlapCnt(0)
{
  sentCnt = 0;
  doc->declare( AnnotationType::METRIC,
		"metricset",
		"annotator='tscan'" );
  doc->declare( AnnotationType::POS,
		"tscan-set",
		"annotator='tscan'" );
  if ( !settings.style.empty() ){
    doc->replaceStyle( "text/xsl", settings.style );
  }
  if ( settings.doLsa ){
    gather_LSA_word_info( doc );
    gather_LSA_doc_info( doc );
  }
  vector<Paragraph*> pars = doc->paragraphs();
  if ( pars.size() > 0 )
    folia_node = pars[0]->parent();
  for ( size_t i=0; i != pars.size(); ++i ){
    parStats *ps = new parStats( pars[i], LSA_word_dists, LSA_sentence_dists );
      merge( ps );
  }
  if ( settings.doLsa ){
    resolveLSA( LSA_paragraph_dists );
  }
  if ( word_freq == 0 || contentCnt == 0 )
    word_freq_log = NA;
  else
    word_freq_log = log10( word_freq / contentCnt );
  if ( contentCnt == nameCnt || word_freq_n == 0 )
    word_freq_log_n = NA;
  else
    word_freq_log_n = log10( word_freq_n / (contentCnt-nameCnt) );
  if ( lemma_freq == 0 || contentCnt == 0 )
    lemma_freq_log = NA;
  else
    lemma_freq_log = log10( lemma_freq / contentCnt );
  if ( contentCnt == nameCnt || lemma_freq_n == 0 )
    lemma_freq_log_n = NA;
  else
    lemma_freq_log_n = log10( lemma_freq_n / (contentCnt-nameCnt) );
  calculate_doc_overlap( );

}

string docStats::rarity( int level ) const {
  map<string,int>::const_iterator it = unique_lemmas.begin();
  int rare = 0;
  while ( it != unique_lemmas.end() ){
    if ( it->second <= level )
      ++rare;
    ++it;
  }
  double result = rare/double( unique_lemmas.size() );
  return toString( result );
}

void docStats::addMetrics( ) const {
  FoliaElement *el = folia_node;
  structStats::addMetrics( );
  addOneMetric( el->doc(), el,
		"sentence_count", toString( sentCnt ) );
  addOneMetric( el->doc(), el,
		"paragraph_count", toString( sv.size() ) );
  addOneMetric( el->doc(), el,
		"word_ttr", toString( unique_words.size()/double(wordCnt) ) );
  addOneMetric( el->doc(), el,
		"lemma_ttr", toString( unique_lemmas.size()/double(wordCnt) ) );
  if ( nameCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "names_ttr", toString( unique_names.size()/double(nameCnt) ) );
  }
  if ( contentCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "content_word_ttr", toString( unique_contents.size()/double(contentCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"rar_index", rarity( settings.rarityLevel ) );
  addOneMetric( el->doc(), el,
		"document_word_argument_overlap_count", toString( doc_word_overlapCnt ) );
  addOneMetric( el->doc(), el,
		"document_lemma_argument_overlap_count", toString( doc_lemma_overlapCnt ) );
}

void docStats::toCSV( const string& name,
		      csvKind what ) const {
  if ( what == DOC_CSV ){
    string fname = name + ".document.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      CSVheader( out, "File" );
      out << name << ",";
      structStats::toCSV( out );
      LOG << "stored document statistics in " << fname << endl;
    }
    else {
      LOG << "storing document statistics in " << fname << " FAILED!" << endl;
    }
  }
  else if ( what == PAR_CSV ){
    string fname = name + ".paragraphs.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      for ( size_t par=0; par < sv.size(); ++par ){
	if ( par == 0 )
	  sv[0]->CSVheader( out, "File,FoLiA-ID" );
	out << name << "," << sv[par]->id << ",";
	sv[par]->toCSV( out );
      }
      LOG << "stored paragraph statistics in " << fname << endl;
    }
    else {
      LOG << "storing paragraph statistics in " << fname << " FAILED!" << endl;
    }
  }
  else if ( what == SENT_CSV ){
    string fname = name + ".sentences.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      for ( size_t par=0; par < sv.size(); ++par ){
	for ( size_t sent=0; sent < sv[par]->sv.size(); ++sent ){
	  if ( par == 0 && sent == 0 )
	    sv[0]->sv[0]->CSVheader( out, "File,FoLiA-ID" );
	  out << name << "," << sv[par]->sv[sent]->id << ",";
	  sv[par]->sv[sent]->toCSV( out );
	}
      }
      LOG << "stored sentence statistics in " << fname << endl;
    }
    else {
      LOG << "storing sentence statistics in " << fname << " FAILED!" << endl;
    }
  }
  else if ( what == WORD_CSV ){
    string fname = name + ".words.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      for ( size_t par=0; par < sv.size(); ++par ){
	for ( size_t sent=0; sent < sv[par]->sv.size(); ++sent ){
	  for ( size_t word=0; word < sv[par]->sv[sent]->sv.size(); ++word ){
	    if ( par == 0 && sent == 0 && word == 0 )
	      sv[0]->sv[0]->sv[0]->CSVheader( out, "File,FoLiA-ID,word" );
	    out << name << "," << sv[par]->sv[sent]->sv[word]->id << ",";
	    sv[par]->sv[sent]->sv[word]->toCSV( out );
	  }
	}
      }
      LOG << "stored word statistics in " << fname << endl;
    }
    else {
      LOG << "storing word statistics in " << fname << " FAILED!" << endl;
    }
  }
}

Document *getFrogResult( istream& is ){
  string host = config.lookUp( "host", "frog" );
  string port = config.lookUp( "port", "frog" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    LOG << "failed to open Frog connection: "<< host << ":" << port << endl;
    LOG << "Reason: " << client.getMessage() << endl;
    return 0;
  }
  DBG << "start input loop" << endl;
  string line;
  while ( getline( is, line ) ){
    DBG << "read: " << line << endl;
    client.write( line + "\n" );
  }
  client.write( "\nEOT\n" );
  string result;
  string s;
  while ( client.read(s) ){
    if ( s == "READY" )
      break;
    result += s + "\n";
  }
  DBG << "received data [" << result << "]" << endl;
  Document *doc = 0;
  if ( !result.empty() && result.size() > 10 ){
    DBG << "start FoLiA parsing" << endl;
    doc = new Document();
    try {
      doc->readFromString( result );
      DBG << "finished" << endl;
    }
    catch ( std::exception& e ){
      LOG << "FoLiaParsing failed:" << endl
	   << e.what() << endl;
    }
  }
  return doc;
}

xmlDoc *AlpinoServerParse( Sentence *sent ){
  string host = config.lookUp( "host", "alpino" );
  string port = config.lookUp( "port", "alpino" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    LOG << "failed to open Alpino connection: "<< host << ":" << port << endl;
    LOG << "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  DBG << "start input loop" << endl;
  string txt = folia::UnicodeToUTF8(sent->toktext());
  client.write( txt + "\n\n" );
  string result;
  string s;
  while ( client.read(s) ){
    result += s + "\n";
  }
  DBG << "received data [" << result << "]" << endl;
  xmlDoc *doc = xmlReadMemory( result.c_str(), result.length(),
			       0, 0, XML_PARSE_NOBLANKS );
  string txtfile = workdir_name + "1.xml";
  xmlSaveFormatFileEnc( txtfile.c_str(), doc, "UTF8", 1 );
  return doc;
}

int main(int argc, char *argv[]) {
  struct stat sbuf;
  pid_t pid = getpid();
  workdir_name = "/tmp/tscan-" + toString( pid ) + "/";
  int res = stat( workdir_name.c_str(), &sbuf );
  if ( res == -1 || !S_ISDIR(sbuf.st_mode) ){
    res = mkdir( workdir_name.c_str(), S_IRWXU|S_IRWXG );
    if ( res ){
      cerr << "problem creating working dir '" << workdir_name
	   << "' : " << res << endl;
      exit( EXIT_FAILURE );
    }
  }
  LOG << "TScan " << VERSION << endl;
  LOG << "working dir " << workdir_name << endl;
  Timbl::TimblOpts opts( argc, argv );
  string val;
  bool mood;
  if ( opts.Find( "h", val, mood ) ||
       opts.Find( "help", val, mood ) ){
    usage();
    exit( EXIT_SUCCESS );
  }
  if ( opts.Find( "V", val, mood ) ||
       opts.Find( "version", val, mood ) ){
    exit( EXIT_SUCCESS );
  }
  string t_option;
  if ( opts.Find( 't', val, mood ) ){
    t_option = val;
  }
  string d_option;
  if ( opts.Find( 'd', val, mood ) ){
    d_option = val;
  }
  string e_option;
  if ( opts.Find( 'e', val, mood ) ){
    e_option = val;
    if ( e_option.size() > 0 && e_option[e_option.size()-1] != '$' )
      e_option += "$";
  }
  if ( !t_option.empty() && !d_option.empty() ){
    cerr << "-t and -d options cannot be combined" << endl;
    exit( EXIT_FAILURE );
  }
  if ( t_option.empty() && d_option.empty() ){
    if ( e_option.empty() ){
      cerr << "missing one of -t, -d or -e options" << endl;
      exit( EXIT_FAILURE );
    }
    else {
      d_option = ".";
    }
  }

  vector<string> inputnames;
  if ( !t_option.empty() ){
    inputnames = searchFiles( t_option );
  }
  else {
    cerr << "search files matching pattern: " << d_option << e_option << endl;
    inputnames = searchFilesMatch( d_option, e_option, false );
  }
  // for ( size_t i = 0; i < inputnames.size(); ++i ){
  //   cerr << i << " - " << inputnames[i] << endl;
  // }
  if ( inputnames.size() == 0 ){
    cerr << "no input file(s) found" << endl;
    exit(EXIT_FAILURE);
  }

  string o_option;
  if ( opts.Find( 'o', val, mood ) ){
    if ( inputnames.size() > 0 ){
      cerr << "-o option not supported for multiple input files" << endl;
      exit(EXIT_FAILURE);
    }
    o_option = val;
  }

  if ( opts.Find( "threads", val, mood ) ){
#ifdef HAVE_OPENMP
    int num = TiCC::stringTo<int>( val );
    if ( num < 1 || num > 4 ){
      cerr << "wrong value for 'threads' option. (must be >=1 and <= 4 )"
	   << endl;
      exit(EXIT_FAILURE);
    }
    else {
      omp_set_num_threads( num );
    }
#else
    cerr << "No OPEN_MP support available. 'threads' option ignored." << endl;
#endif
  }

  if ( opts.Find( "config", val, mood ) ){
    configFile = val;
    opts.Delete( "config" );
  }
  if ( !configFile.empty() &&
       config.fill( configFile ) ){
    settings.init( config );
  }
  else {
    cerr << "invalid configuration" << endl;
    exit( EXIT_FAILURE );
  }
  if ( opts.Find( 'D', val, mood ) ){
    if ( val == "Normal" )
      cur_log.setlevel( LogNormal );
    else if ( val == "Debug" )
      cur_log.setlevel( LogDebug );
    else if ( val == "Heavy" )
      cur_log.setlevel( LogHeavy );
    else if ( val == "Extreme" )
      cur_log.setlevel( LogExtreme );
    else {
      cerr << "Unknown Debug mode! (-D " << val << ")" << endl;
    }
    opts.Delete( 'D' );
  }
  if ( opts.Find( "skip", val, mood)) {
    string skip = val;
    if ( skip.find_first_of("wW") != string::npos ){
      settings.doWopr = false;
    }
    if ( skip.find_first_of("lL") != string::npos ){
      settings.doLsa = false;
    }
    if ( skip.find_first_of("aA") != string::npos ){
      settings.doAlpino = false;
      settings.doAlpinoServer = false;
    }
    if ( skip.find_first_of("dD") != string::npos ){
      settings.doDecompound = false;
    }
    if ( skip.find_first_of("cC") != string::npos ){
      settings.doXfiles = false;
    }
    opts.Delete("skip");
  };

  if ( inputnames.size() > 1 ){
    LOG << "processing " << inputnames.size() << " files." << endl;
  }
  for ( size_t i = 0; i < inputnames.size(); ++i ){
    string inName = inputnames[i];
    LOG << "file: " << inName << endl;
    string outName;
    if ( !o_option.empty() ){
      // just 1 inputfile
      outName = o_option;
    }
    else {
      outName = inName + ".tscan.xml";
    }
    ifstream is( inName.c_str() );
    if ( !is ){
      cerr << "failed to open file '" << inName << "'" << endl;
      exit(EXIT_FAILURE);
    }
    else {
      LOG << "opened file " <<  inName << endl;
      Document *doc = getFrogResult( is );
      if ( !doc ){
	cerr << "big trouble: no FoLiA document created " << endl;
	exit(EXIT_FAILURE);
      }
      else {
	docStats analyse( doc );
	analyse.addMetrics(); // add metrics info to doc
	doc->save( outName );
	if ( settings.doXfiles ){
	  analyse.toCSV( inName, DOC_CSV );
	  analyse.toCSV( inName, PAR_CSV );
	  analyse.toCSV( inName, SENT_CSV );
	  analyse.toCSV( inName, WORD_CSV );
	}
	delete doc;
	LOG << "saved output in " << outName << endl;
      }
    }
  }
  exit(EXIT_SUCCESS);
}

