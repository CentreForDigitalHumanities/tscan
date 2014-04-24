/*
  $Id$
  $URL$

  Copyright (c) 1998 - 2014

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
#include <fstream>
#include <cmath>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

#include "timbl/TimblAPI.h"
#include "timblserver/FdStream.h"
#include "timblserver/ServerBase.h"
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

enum SemType { NO_SEMTYPE,
	       UNFOUND_NOUN, UNFOUND_ADJ, UNFOUND_VERB,
	       UNDEFINED_NOUN, UNDEFINED_ADJ,
	       ABSTRACT_DYNAMIC_NOUN, ABSTRACT_NONDYNAMIC_NOUN,
	       BROAD_CONCRETE_PLACE_NOUN,
	       BROAD_CONCRETE_TIME_NOUN,
	       BROAD_CONCRETE_MEASURE_NOUN,
	       CONCRETE_OTHER_NOUN, INSTITUT_NOUN,
	       CONCRETE_SUBSTANCE_NOUN, CONCRETE_ARTEFACT_NOUN,
	       CONCRETE_HUMAN_NOUN, CONCRETE_NONHUMAN_NOUN,
	       HUMAN_ADJ, EMO_ADJ,
	       NONHUMAN_SHAPE_ADJ, NONHUMAN_COLOR_ADJ, NONHUMAN_MATTER_ADJ,
	       NONHUMAN_SOUND_ADJ, NONHUMAN_OTHER_ADJ,
	       TECH_ADJ, TIME_ADJ, PLACE_ADJ,
	       SPEC_POS_ADJ, SPEC_NEG_ADJ,
	       POS_ADJ, NEG_ADJ,
	       EPI_POS_ADJ, EPI_NEG_ADJ,
	       MORE_ADJ, LESS_ADJ,
	       ABSTRACT_ADJ,
	       ABSTRACT_VERB, CONCRETE_VERB, UNDEFINED_VERB,
	       ABSTRACT_STATE, CONCRETE_STATE, UNDEFINED_STATE,
	       ABSTRACT_ACTION, CONCRETE_ACTION, UNDEFINED_ACTION,
	       ABSTRACT_PROCESS, CONCRETE_PROCESS, UNDEFINED_PROCESS };

string toString( const SemType st ){
  switch ( st ){
  case NO_SEMTYPE:
    return "semtype_error";
    break;
  case UNFOUND_NOUN:
    return "noun_not_found";
    break;
  case UNFOUND_ADJ:
    return "adj_not_found";
    break;
  case UNFOUND_VERB:
    return "verb_not_found";
    break;
  case UNDEFINED_NOUN:
    return "undefined-noun";
    break;
  case UNDEFINED_ADJ:
    return "undefined-adj";
    break;
  case ABSTRACT_DYNAMIC_NOUN:
    return "abstract-dynamic_noun";
    break;
  case ABSTRACT_NONDYNAMIC_NOUN:
    return "abstract-nondynamic-noun";
    break;
  case BROAD_CONCRETE_PLACE_NOUN:
    return "broad-place-noun";
    break;
  case BROAD_CONCRETE_TIME_NOUN:
    return "broad-time-noun";
    break;
  case BROAD_CONCRETE_MEASURE_NOUN:
    return "broad-measure-noun";
    break;
  case CONCRETE_OTHER_NOUN:
    return "concrete-other-noun";
    break;
  case INSTITUT_NOUN:
    return "institut-noun";
    break;
  case CONCRETE_SUBSTANCE_NOUN:
    return "concrete-substance-noun";
    break;
  case CONCRETE_ARTEFACT_NOUN:
    return "concrete-artefact-noun";
    break;
  case CONCRETE_HUMAN_NOUN:
    return "concrete-human-noun";
    break;
  case CONCRETE_NONHUMAN_NOUN:
    return "concrete-nonhuman-noun";
    break;

  case ABSTRACT_ADJ:
    return "abstract-adj";
    break;
  case HUMAN_ADJ:
    return "human-adj";
    break;
  case EMO_ADJ:
    return "emo-adj";
    break;
  case NONHUMAN_SHAPE_ADJ:
    return "shape-adj";
    break;
  case NONHUMAN_COLOR_ADJ:
    return "color-adj";
    break;
  case NONHUMAN_MATTER_ADJ:
    return "matter-adj";
    break;
  case NONHUMAN_SOUND_ADJ:
    return "sound-adj";
    break;
  case NONHUMAN_OTHER_ADJ:
    return "nonhuman-other-adj";
    break;
  case TECH_ADJ:
    return "tech-adj";
    break;
  case TIME_ADJ:
    return "time-adj";
    break;
  case PLACE_ADJ:
    return "place-adj";
    break;
  case SPEC_POS_ADJ:
    return "spec-pos-adj";
    break;
  case SPEC_NEG_ADJ:
    return "spec-neg-adj";
    break;
  case POS_ADJ:
    return "pos-adj";
    break;
  case NEG_ADJ:
    return "neg-adj";
    break;
  case EPI_POS_ADJ:
    return "epi-pos-adj";
    break;
  case EPI_NEG_ADJ:
    return "epi-neg-adj";
    break;
  case MORE_ADJ:
    return "stronger-adj";
    break;
  case LESS_ADJ:
    return "weaker-adj";
    break;

  case ABSTRACT_VERB:
    return "abstract-verb";
    break;
  case CONCRETE_VERB:
    return "concrete-verb";
    break;
  case UNDEFINED_VERB:
    return "undefined-verb";
    break;
  case ABSTRACT_STATE:
    return "abstract-state";
    break;
  case CONCRETE_STATE:
    return "concrete-state";
    break;
  case UNDEFINED_STATE:
    return "undefined-state";
    break;
  case ABSTRACT_ACTION:
    return "abstract-action";
    break;
  case CONCRETE_ACTION:
    return "concrete-action";
    break;
  case UNDEFINED_ACTION:
    return "undefined-action";
    break;
  case ABSTRACT_PROCESS:
    return "abstract-process";
    break;
  case CONCRETE_PROCESS:
    return "concrete-process";
    break;
  case UNDEFINED_PROCESS:
    return "undefined-process";
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
  double mtld_threshold;
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
  map<CGN::Type, set<string> > causal_sits;
  set<string> multi_causal_sits;
  map<CGN::Type, set<string> > space_sits;
  set<string> multi_space_sits;
  map<CGN::Type, set<string> > time_sits;
  set<string> multi_time_sits;
  map<CGN::Type, set<string> > emotion_sits;
  set<string> multi_emotion_sits;
  set<string> vzexpr2;
  set<string> vzexpr3;
  set<string> vzexpr4;
  map<string,AfkType> afkos;
};

settingData settings;

SemType classifyNoun( const string& s ){
  if ( s == "undefined" )
    return UNDEFINED_NOUN;
  else if ( s == "concrother" )
    return CONCRETE_OTHER_NOUN;
  else if ( s == "institut" )
    return INSTITUT_NOUN;
  else if ( s == "substance" )
    return CONCRETE_SUBSTANCE_NOUN;
  else if ( s == "artefact" )
    return CONCRETE_ARTEFACT_NOUN;
  else if ( s == "nonhuman" )
    return CONCRETE_NONHUMAN_NOUN;
  else if ( s == "human" )
    return CONCRETE_HUMAN_NOUN;
  else if ( s == "dynamic" )
    return ABSTRACT_DYNAMIC_NOUN;
  else if ( s == "nondynamic" )
    return ABSTRACT_NONDYNAMIC_NOUN;
  else if ( s == "place" )
    return BROAD_CONCRETE_PLACE_NOUN;
  else if ( s == "time" )
    return BROAD_CONCRETE_TIME_NOUN;
  else if ( s == "measure" )
    return BROAD_CONCRETE_MEASURE_NOUN;
  else
    return UNFOUND_NOUN;
}

SemType classifyWW( const string& s, const string& c = "" ){
  if ( s == "undefined" ){
    if ( c == "abstract" )
      return ABSTRACT_VERB;
    else if ( c == "concreet" )
      return CONCRETE_VERB;
    else if ( c == "undefined" )
      return UNDEFINED_VERB;
  }
  else if ( s == "state" ){
    if ( c == "abstract" )
      return ABSTRACT_STATE;
    else if ( c == "concreet" )
      return CONCRETE_STATE;
    else if ( c == "undefined" )
      return UNDEFINED_STATE;
  }
  else if ( s == "action" ){
    if ( c == "abstract" )
      return ABSTRACT_ACTION;
    else if ( c == "concreet" )
      return CONCRETE_ACTION;
    else if ( c == "undefined" )
      return UNDEFINED_ACTION;
  }
  else if (s == "process" ){
    if ( c == "abstract" )
      return ABSTRACT_PROCESS;
    else if ( c == "concreet" )
      return CONCRETE_PROCESS;
    else if ( c == "undefined" )
      return UNDEFINED_PROCESS;
  }
  else if (s == "undefined" ){
    if ( c == "abstract" )
      return ABSTRACT_VERB;
    else if ( c == "concreet" )
      return CONCRETE_VERB;
    else if ( c == "undefined" )
      return UNDEFINED_VERB;
  }
  return UNFOUND_VERB;
}

SemType classifyADJ( const string& s, const string& sub = "" ){
  SemType result = UNFOUND_ADJ;
  if ( s == "undefined" ){
    result = UNDEFINED_ADJ;
  }
  else if ( s == "waarn_mens" ){
    result = HUMAN_ADJ;
  }
  else if ( s == "emosoc" ){
    result = EMO_ADJ;
  }
  else if ( s == "waarn_niet_mens" ){
    if ( sub == "vorm_omvang" ){
      result = NONHUMAN_SHAPE_ADJ;
    }
    else if ( sub == "kleur" ){
      result = NONHUMAN_COLOR_ADJ;
    }
    else if ( sub == "stof" ){
      result = NONHUMAN_MATTER_ADJ;
    }
    else if ( sub == "geluid" ){
      result = NONHUMAN_SOUND_ADJ;
    }
    else if ( sub == "waarn_niet_mens_ov" ){
      result = NONHUMAN_OTHER_ADJ;
    }
  }
  else if ( s == "technisch" ){
    result = TECH_ADJ;
  }
  else if ( s == "time" ){
    result = TIME_ADJ;
  }
  else if ( s == "place" ){
    result = PLACE_ADJ;
  }
  else if ( s == "spec_positief" ){
    result = SPEC_POS_ADJ;
  }
  else if ( s == "spec_negatief" ){
    result = SPEC_NEG_ADJ;
  }
  else if ( s == "alg_positief" ){
    result = POS_ADJ;
  }
  else if ( s == "alg_negatief" ){
    result = NEG_ADJ;
  }
  else if ( s == "ep_positief" ){
    result = EPI_POS_ADJ;
  }
  else if ( s == "ep_negatief" ){
    result = EPI_NEG_ADJ;
  }
  else if ( s == "versterker" ){
    result = MORE_ADJ;
  }
  else if ( s == "verzwakker" ){
    result = LESS_ADJ;
  }
  else if ( s == "abstract_ov" ){
    result = ABSTRACT_ADJ;
  }
  if ( result == UNFOUND_ADJ ){
    cerr << "classify ADJ " << s << "," << sub << endl;
  }
  return result;
}


bool fillN( map<string,SemType>& m, istream& is ){
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
      m[parts[0]] = classifyNoun( vals[0] );
    }
    else if ( n == 0 ){
      cerr << "skip line: " << line << " (expected some values, got none."
	   << endl;
      continue;
    }
    else {
      SemType topval = NO_SEMTYPE;
      map<SemType,int> stats;
      set<SemType> values;
      for ( size_t i=0; i< vals.size(); ++i ){
	SemType val = classifyNoun( vals[i] );
	stats[val]++;
	values.insert(val);
      }
      SemType res = UNFOUND_NOUN;
      if ( values.size() == 1 ){
	// just 1 semtype
	res = *values.begin();
      }
      else {
	// possible values are (from high to low)
	// CONCRETE_SUBSTANCE_NOUN
	// CONCRETE_ARTEFACT_NOUN
	// CONCRETE_NONHUMAN_NOUN
	// ABSTRACT_DYNAMIC_NOUN
	// ABSTRACT_NONDYNAMIC_NOUN
	// CONCRETE_HUMAN_NOUN
	// INSTITUT_NOUN
	// CONCRETE_OTHER_NOUN
	// UNFOUND
	if ( values.find(CONCRETE_ARTEFACT_NOUN) != values.end() ) {
	  if ( values.find(CONCRETE_SUBSTANCE_NOUN) != values.end() ){
	    res = CONCRETE_SUBSTANCE_NOUN;
	  }
	  else {
	    res = CONCRETE_ARTEFACT_NOUN;
	  }
	}
	else if ( values.find(CONCRETE_NONHUMAN_NOUN) != values.end() ){
	  res = CONCRETE_NONHUMAN_NOUN;
	}
	else if ( values.find(INSTITUT_NOUN) != values.end() ){
	  res = INSTITUT_NOUN;
	}
	else if ( values.find(CONCRETE_OTHER_NOUN) != values.end() ){
	  if ( values.find(ABSTRACT_DYNAMIC_NOUN) != values.end() ){
	    res = CONCRETE_OTHER_NOUN;
	  }
	  else if ( values.find(ABSTRACT_NONDYNAMIC_NOUN) != values.end() ){
	    res = CONCRETE_OTHER_NOUN;
	  }
	  else if ( values.find(CONCRETE_SUBSTANCE_NOUN) != values.end() ){
	    res = CONCRETE_SUBSTANCE_NOUN;
	  }
	  else if ( values.find(CONCRETE_HUMAN_NOUN) != values.end() ){
	    res = CONCRETE_HUMAN_NOUN;
	  }
	}
	else if ( values.find(ABSTRACT_DYNAMIC_NOUN) != values.end() ){
	  if ( values.find(ABSTRACT_NONDYNAMIC_NOUN) != values.end() ){
	    res = ABSTRACT_DYNAMIC_NOUN;
	  }
	}
	if ( res == UNFOUND_NOUN ){
	  cerr << "unable to determine semtype from: " << values << endl;
	}
      }
      topval = res;
      if ( m.find(parts[0]) != m.end() ){
	cerr << "multiple entry '" << parts[0] << "' in N lex" << endl;
      }
      if ( topval != UNFOUND_NOUN ){
	// no use to store undefined values
	m[parts[0]] = topval;
      }
    }
  }
  return true;
}

bool fillWW( map<string,SemType>& m, istream& is ){
  string line;
  while( getline( is, line ) ){
    vector<string> parts;
    int n = split_at( line, parts, "\t" ); // split at tab
    if ( n != 3 ){
      cerr << "skip line: " << line << " (expected 3 values, got "
	   << n << ")" << endl;
      continue;
    }
    SemType res = classifyWW( parts[1], parts[2] );
    if ( res != UNFOUND_VERB ){
      // no use to store undefined values
      m[parts[0]] = res;
    }
  }
  return true;
}

bool fillADJ( map<string,SemType>& m, istream& is ){
  string line;
  while( getline( is, line ) ){
    vector<string> parts;
    int n = split_at( line, parts, "\t" ); // split at tab
    if ( n <2 || n > 3 ){
      cerr << "skip line: " << line << " (expected 2 or 3 values, got "
	   << n << ")" << endl;
      continue;
    }
    SemType res = UNFOUND_ADJ;
    if ( n == 2 ){
      res = classifyADJ( parts[1] );
    }
    else {
      res = classifyADJ( parts[1], parts[2] );
    }
    if ( m.find(parts[0]) != m.end() ){
      cerr << "multiple entry '" << parts[0] << "' in ADJ lex" << endl;
    }
    if ( res != UNFOUND_ADJ ){
      // no use to store undefined values
      m[parts[0]] = res;
    }
  }
  return true;
}

bool fill( CGN::Type tag, map<string,SemType>& m, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    if ( tag == CGN::N )
      return fillN( m, is );
    else if ( tag == CGN::WW )
      return fillWW( m, is );
    if ( tag == CGN::ADJ )
      return fillADJ( m, is );
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
  cM.clear();
  string line;
  while( getline( is, line ) ){
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
    if ( n < 1 || n > 4 ){
      cerr << "skip line: " << line
	   << " (expected 1, to 4 values in the first part: " << vec[0]
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
  doWopr = false;
  val = cf.lookUp( "useWopr" );
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
  val = cf.lookUp( "mtldThreshold" );
  if ( val.empty() ){
    mtld_threshold = 0.720;
  }
  else if ( !TiCC::stringTo( val, mtld_threshold )
	    || (mtld_threshold < 0) || (mtld_threshold > 1.0) ){
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
  val = cf.lookUp( "causal_situation" );
  if ( !val.empty() ){
    if ( !fill_connectors( causal_sits, multi_causal_sits, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "space_situation" );
  if ( !val.empty() ){
    if ( !fill_connectors( space_sits, multi_space_sits, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "time_situation" );
  if ( !val.empty() ){
    if ( !fill_connectors( time_sits, multi_time_sits, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }

  val = cf.lookUp( "emotion_situation" );
  if ( !val.empty() ){
    if ( !fill_connectors( emotion_sits, multi_emotion_sits, cf.configDir() + "/" + val ) )
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

struct proportion {
  proportion( double d1, double d2 ){
    if ( d1 < 0 || d2 == 0 ||
	 d1 == NA || d2 == NA )
      p = NA;
    else
      p = d1/d2;
  };
  double p;
};

ostream& operator<<( ostream& os, const proportion& p ){
  if ( p.p == NA )
    os << "NA";
  else
    os << p.p;
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

struct ratio {
  ratio( double d1, double d2 ){
    if ( d1 < 0 || d2 == 0 ||
	 d1 == NA || d2 == NA || d1 == d2 )
      r = NA;
    else
      r = d1/(d2-d1);
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
    return "no";
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

ostream& operator<<( ostream& os, const ConnType& s ){
  os << toString(s);
  return os;
}

enum SituationType { NO_SIT, TIME_SIT, CAUSAL_SIT, SPACE_SIT, EMO_SIT };

string toString( const SituationType& c ){
  switch ( c ){
  case NO_SIT:
    return "Geen_situatie";
    break;
  case TIME_SIT:
    return "tijd";
    break;
  case SPACE_SIT:
    return "ruimte";
    break;
  case CAUSAL_SIT:
    return "causaliteit";
    break;
  case EMO_SIT:
    return "emotie";
    break;
  default:
    throw "no translation for SituationType";
  }
}

ostream& operator<<( ostream& os, const SituationType& s ){
  os << toString(s);
  return os;
}

struct sentStats;
struct wordStats;

struct basicStats {
  basicStats( int pos, FoliaElement *el, const string& cat ):
    folia_node( el ),
    category( cat ),
    index(pos),
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
  virtual string Lemma() const { return ""; };
  virtual string llemma() const { return ""; };
  virtual CGN::Type postag() const { return CGN::UNASS; };
  virtual WordProp wordProperty() const { return NOTAWORD; };
  virtual ConnType getConnType() const { return NOCONN; };
  virtual void setConnType( ConnType ){
    throw logic_error("setConnType() only valid for words" );
  };
  virtual void setMultiConn(){
    throw logic_error("setMultiConn() only valid for words" );
  };
  virtual void setSitType( SituationType ){
    throw logic_error("setSitType() only valid for words" );
  };
  virtual SituationType getSitType() const { return NO_SIT; };
  virtual vector<const wordStats*> collectWords() const = 0;
  virtual double get_al_gem() const { return NA; };
  virtual double get_al_max() const { return NA; };
  void setLSAsuc( double d ){ lsa_opv = d; };
  void setLSAcontext( double d ){ lsa_ctx = d; };
  FoliaElement *folia_node;
  string category;
  string id;
  int index;
  int charCnt;
  int charCntExNames;
  int morphCnt;
  int morphCntExNames;
  double lsa_opv;
  double lsa_ctx;
  vector<basicStats *> sv;
};

struct wordStats : public basicStats {
  wordStats( int, Word *, const xmlNode *, const set<size_t>&, bool );
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
  string Lemma() const { return lemma; };
  string llemma() const { return l_lemma; };
  CGN::Type postag() const { return tag; };
  ConnType getConnType() const { return connType; };
  void setConnType( ConnType t ){ connType = t; };
  void setMultiConn(){ isMultiConn = true; };
  void setPersRef();
  void setSitType( SituationType t ){ sitType = t; };
  SituationType getSitType() const { return sitType; };
  void addMetrics( ) const;
  bool checkContent() const;
  ConnType checkConnective( const xmlNode * ) const;
  ConnType check_small_connector( const xmlNode * ) const;
  SituationType checkSituation() const;
  bool checkNominal( const xmlNode * ) const;
  WordProp checkProps( const PosAnnotation* );
  WordProp wordProperty() const { return prop; };
  SemType checkSemProps( ) const;
  bool isBroadAdj() const;
  bool isStrictAdj() const;
  bool isBroadNoun() const;
  bool isStrictNoun() const;
  AfkType checkAfk( ) const;
  bool checkPropNeg() const;
  bool checkMorphNeg() const;
  void staphFreqLookup();
  void topFreqLookup();
  void freqLookup();
  void getSentenceOverlap( const vector<string>&, const vector<string>& );
  bool isOverlapCandidate() const;
  vector<const wordStats*> collectWords() const;
  bool parseFail;
  string word;
  string l_word;
  string pos;
  CGN::Type tag;
  string lemma;
  string full_lemma; // scheidbare ww hebben dit
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
  SituationType sitType;
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

SituationType wordStats::checkSituation() const {
  if ( settings.time_sits[tag].find( lemma )
       != settings.time_sits[tag].end() ){
    return TIME_SIT;
  }
  else if ( settings.time_sits[CGN::UNASS].find( lemma )
	    != settings.time_sits[CGN::UNASS].end() ){
    return TIME_SIT;
  }
  else if ( settings.causal_sits[tag].find( lemma )
	    != settings.causal_sits[tag].end() ){
    return CAUSAL_SIT;
  }
  else if ( settings.causal_sits[CGN::UNASS].find( lemma )
	    != settings.causal_sits[CGN::UNASS].end() ){
    return CAUSAL_SIT;
  }
  else if ( settings.space_sits[tag].find( lemma )
	    != settings.space_sits[tag].end() ){
    return SPACE_SIT;
  }
  else if ( settings.space_sits[CGN::UNASS].find( lemma )
	    != settings.space_sits[CGN::UNASS].end() ){
    return SPACE_SIT;
  }
  else if ( settings.emotion_sits[tag].find( lemma )
	    != settings.emotion_sits[tag].end() ){
    return EMO_SIT;
  }
  else if ( settings.emotion_sits[CGN::UNASS].find( lemma )
	    != settings.emotion_sits[CGN::UNASS].end() ){
    return EMO_SIT;
  }
  return NO_SIT;
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
    SemType sem2 = UNFOUND_NOUN;
    //    cerr << "lookup " << lemma << endl;
    map<string,SemType>::const_iterator sit = settings.noun_sem.find( lemma );
    if ( sit != settings.noun_sem.end() ){
      sem2 = sit->second;
    }
    //    cerr << "semtype=" << sem2 << endl;
    return sem2;
  }
  else if ( prop == ISNAME ){
    // Names are te be looked up in the Noun list too
    SemType sem2 = UNFOUND_NOUN;
    map<string,SemType>::const_iterator sit = settings.noun_sem.find( lemma );
    if ( sit != settings.noun_sem.end() ){
      sem2 = sit->second;
    }
    return sem2;
  }
  else if ( tag == CGN::ADJ ) {
    SemType sem2 = UNFOUND_ADJ;
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
    SemType sem2 = UNFOUND_VERB;
    map<string,SemType>::const_iterator sit = settings.verb_sem.end();
    if ( !full_lemma.empty() ) {
      sit = settings.verb_sem.find( full_lemma );
    }
    if ( sit == settings.verb_sem.end() ){
      sit = settings.verb_sem.find( lemma );
    }
    if ( sit != settings.verb_sem.end() ){
      sem2 = sit->second;
    }
    return sem2;
  }
  return NO_SEMTYPE;
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
  it = settings.lemma_freq_lex.end();
  if ( !full_lemma.empty() ){
    // scheidbaar ww
    it = settings.lemma_freq_lex.find( full_lemma );
  }
  if ( it == settings.lemma_freq_lex.end() ){
    it = settings.lemma_freq_lex.find( l_lemma );
  }
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

#define DEBUG_COMPOUNDS
enum CompoundType { NOCOMP, NN, PN, VN, PV, BB, BV };

ostream& operator<<( ostream&os, const CompoundType& cc ){
  switch( cc ){
  case NN:
    os << "NN-compound";
    break;
  case PN:
    os << "PN-compound";
    break;
  case VN:
    os << "VN-compound";
    break;
  case PV:
    os << "PV-compound";
    break;
  case BB:
    os << "BB-compound";
    break;
  case BV:
    os << "BV-compound";
    break;
  case NOCOMP:
    os << "no-compound";
    break;
  }
  return os;
}

CompoundType detect_compound( const Morpheme *m ){
  CompoundType result = NOCOMP;
#ifdef DEBUG_COMPOUNDS
  cerr << "top morph: " << m << endl;
#endif
  vector<PosAnnotation*> posV1 = m->select<PosAnnotation>(false);
  if ( posV1.size() == 1 ){
#ifdef DEBUG_COMPOUNDS
    cerr << "pos " << posV1[0] << endl;
#endif
    string topPos = posV1[0]->cls();
    map<string,int> counts;
    if ( topPos == "N"
	 || topPos == "B"
	 || topPos == "V" ){
#ifdef DEBUG_COMPOUNDS
      cerr << "top-pos: " << topPos << endl;
#endif
      vector<Morpheme*> mV = m->select<Morpheme>( frog_morph_set, false );
      for( size_t i=0; i < mV.size(); ++i ){
	if ( mV[i]->cls() == "stem" ){
#ifdef DEBUG_COMPOUNDS
	  cerr << "bekijk stem kind morph: " << mV[i] << endl;
#endif
	  vector<PosAnnotation*> posV2 = mV[i]->select<PosAnnotation>(false);
	  for( size_t j=0; j < posV2.size(); ++j ){
	    string childPos = posV2[j]->cls();
	    if ( childPos == "N"
		 || childPos == "P"
		 || childPos == "V"
		 || childPos == "A"
		 || childPos == "B" ){
#ifdef DEBUG_COMPOUNDS
	      cerr << "kind pos = " << childPos << endl;
#endif
	      ++counts[childPos];
	    }
	    else {
#ifdef DEBUG_COMPOUNDS
	      cerr << "onbekend kind pos = " << childPos << endl;
#endif
	      counts.clear();
	      break;
	    }
	  }
	}
	else if ( mV[i]->cls() == "complex" ) {
#ifdef DEBUG_COMPOUNDS
	  cerr << "recurse: " << endl;
#endif
	  CompoundType cmp = detect_compound( mV[i] );
	  if ( cmp == PN
	       || cmp == VN
	       || cmp == NN ){
	    if ( counts["N"] == 0 ){
	      counts["N"] = 2;
	    }
	    else {
	      ++counts["N"];
	    }
	  }
	  else if ( cmp == PV
		    || cmp == BV ){
	    if ( counts["V"] == 0 ){
	      counts["V"] = 2;
	    }
	    else {
	      ++counts["V"];
	    }
	  }
	  else if ( cmp == BB ){
	    if ( counts["B"] == 0 ){
	      counts["B"] = 2;
	    }
	    else {
	      ++counts["B"];
	    }
	  }
	  else {
	    counts.clear();
	    break;
	  }
	}
	else {
#ifdef DEBUG_COMPOUNDS
	  cerr << "skip a " << mV[i]->cls() << " morpheme" << endl;
#endif
	}
      }
      if ( topPos == "N" ){
	if ( counts["V"] > 0 && counts["N"] > 0 ){
	  result = VN;
	}
	else if ( counts["P"] > 0 && counts["N"] > 0 ){
	  result = PN;
	}
	else if ( counts["N"] > 1 ){
	  result = NN;
	}
      }
      else if ( topPos == "V" ){
	if ( counts["P"] > 0 && counts["V"] > 0 ){
	  result = PV;
	}
	else if ( counts["B"] > 0 && counts["V"] > 0 ){
	  result = BV;
	}
      }
      else if ( topPos == "B" ){
	if ( counts["B"] > 1 ){
	  result = BB;
	}
      }
    }
  }
#ifdef DEBUG_COMPOUNDS
  cerr << "detected " << result << endl;
#endif
  return result;
}


wordStats::wordStats( int index,
		      Word *w,
		      const xmlNode *alpWord,
		      const set<size_t>& puncts,
		      bool fail ):
  basicStats( index, w, "word" ), parseFail(fail), wwform(::NO_VERB),
  isPersRef(false), isPronRef(false),
  archaic(false), isContent(false), isNominal(false),isOnder(false), isImperative(false),
  isBetr(false), isPropNeg(false), isMorphNeg(false),
  nerProp(NONER), connType(NOCONN), isMultiConn(false), sitType(NO_SIT),
  f50(false), f65(false), f77(false), f80(false),  compPartCnt(0),
  top_freq(notFound), word_freq(0), lemma_freq(0),
  wordOverlapCnt(0), lemmaOverlapCnt(0),
  word_freq_log(NA), lemma_freq_log(NA),
  logprob10(NA), prop(JUSTAWORD), sem_type(NO_SEMTYPE),afkType(NO_A)
{
  UnicodeString us = w->text();
  charCnt = us.length();
  word = UnicodeToUTF8( us );
  l_word = UnicodeToUTF8( us.toLower() );
  if ( fail )
    return;
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
      string full;
      wwform = classifyVerb( alpWord, lemma, full );
      if ( !full.empty() ){
	to_lower( full );
	//	cerr << "scheidbaar WW: " << full << endl;
	full_lemma = full;
      }
      if ( (prop == ISPVTGW || prop == ISPVVERL) &&
	   wwform != PASSIVE_VERB ){
	isImperative = checkImp( alpWord );
      }
    }
  }
  isContent = checkContent();
  if ( prop != ISLET ){
    CompoundType comp = NOCOMP;
    vector<MorphologyLayer*> ml = w->annotations<MorphologyLayer>();
    size_t max = 0;
    vector<Morpheme*> morphemeV;
    for ( size_t q=0; q < ml.size(); ++q ){
      vector<Morpheme*> m = ml[q]->select<Morpheme>( frog_morph_set, false );
      if ( m.size() == 1  && m[0]->cls() == "complex" ){
	//	comp = detect_compound( m[0] );
	// nested morphemes
	string desc = m[0]->description();
	vector<string> parts;
	TiCC::split_at_first_of( desc, parts, "[]" );
	if ( parts.size() > max ){
	  // a hack: we assume the longest morpheme list to
	  // be the best choice.
	  morphemes = parts;
	  max = parts.size();
	}
      }
      else {
	// flat morphemes
	m = ml[q]->select<Morpheme>( frog_morph_set );
	if ( m.size() > max ){
	  // a hack: we assume the longest morpheme list to
	  // be the best choice.
	  morphemes.clear();
	  for ( size_t i=0; i <m.size(); ++i ){
	    morphemes.push_back( m[i]->str() );
	  }
	  max = m.size();
	}
      }
    }
    if ( morphemes.size() == 0 ){
      cerr << "unable to retrieve morphemes from folia." << endl;
    }
    //    cerr << "Morphemes " << word << "= " << morphemes << endl;
    isPropNeg = checkPropNeg();
    isMorphNeg = checkMorphNeg();
    connType = checkConnective( alpWord );
    sitType = checkSituation();
    morphCnt = morphemes.size();
    if ( prop != ISNAME ){
      charCntExNames = charCnt;
      morphCntExNames = morphCnt;
    }
    sem_type = checkSemProps();
    afkType = checkAfk();
    if ( alpWord )
      isNominal = checkNominal( alpWord );
    topFreqLookup();
    staphFreqLookup();
    if ( isContent ){
      freqLookup();
    }
    if ( settings.doDecompound ){
      compPartCnt = runDecompoundWord( word, workdir_name,
				       settings.decompounderPath );
      if ( compPartCnt > 0 || comp != NOCOMP ){
	//	cerr << morphemes << " is a " << comp << " - " << compPartCnt << endl;
      }
    }
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


void wordStats::setPersRef() {
  if ( sem_type == CONCRETE_HUMAN_NOUN ||
       nerProp == PER_B ||
       prop == ISPPRON1 || prop == ISPPRON2 || prop == ISPPRON3 ){
    isPersRef = true;
  }
  else {
    isPersRef = false;
  }
}

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
//#undef DEBUG_OL

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
  if ( !full_lemma.empty() ){
    addOneMetric( doc, el, "full-lemma", full_lemma );
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
  if ( sitType != NO_SIT )
    addOneMetric( doc, el, "situation", toString(sitType) );
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
  if ( sem_type != NO_SEMTYPE )
    addOneMetric( doc, el, "semtype", toString(sem_type) );
  if ( afkType != NO_A )
    addOneMetric( doc, el, "afktype", toString(afkType) );
}

void wordStats::CSVheader( ostream& os, const string& intro ) const {
  os << intro << ",";
  wordSortHeader( os );
  wordDifficultiesHeader( os );
  coherenceHeader( os );
  concreetHeader( os );
  persoonlijkheidHeader( os );
  miscHeader( os );
  os << endl;
}

void wordStats::wordDifficultiesHeader( ostream& os ) const {
  os << "Let_per_wrd,Wrd_per_let,Let_per_wrd_zn,Wrd_per_let_zn,"
     << "Morf_per_wrd,Wrd_per_morf,Morf_per_wrd_zn,Wrd_per_morf_zn,"
     << "Sam_delen_per_wrd,Sam_d,"
     << "Freq50_staph,Freq65_Staph,Freq77_Staph,Feq80_Staph,"
     << "Wrd_freq_log,Wrd_freq_zn_log,Lem_freq_log,Lem_freq_zn_log,"
     << "Freq1000,Freq2000,Freq3000,Freq5000,Freq10000,Freq20000,"
     <<" so_suc,so_ctx,";
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
  os << "connector_type,multiword,Vnw_ref,";
}

void wordStats::coherenceToCSV( ostream& os ) const {
  if ( connType == NOCONN )
    os << "0,";
  else
    os << connType << ",";
  os << isMultiConn << ","
     << isPronRef << ",";
}

void wordStats::concreetHeader( ostream& os ) const {
  os << "semtype_nw,";
  os << "Conc_nw_strikt,";
  os << "Conc_nw_ruim,";
  os << "semtype_bvnw,";
  os << "Conc_bvnw_strikt,";
  os << "Conc_bvnw_ruim,";
  os << "semtype_ww,";
}

bool wordStats::isStrictNoun() const {
  switch ( sem_type ){
  case CONCRETE_HUMAN_NOUN:
  case ABSTRACT_DYNAMIC_NOUN:
  case ABSTRACT_NONDYNAMIC_NOUN:
  case CONCRETE_NONHUMAN_NOUN:
  case CONCRETE_ARTEFACT_NOUN:
  case CONCRETE_SUBSTANCE_NOUN:
  case CONCRETE_OTHER_NOUN:
    return true;
    break;
  default:
    return false;
  }
}

bool wordStats::isBroadNoun() const {
  switch ( sem_type ){
  case CONCRETE_HUMAN_NOUN:
  case ABSTRACT_DYNAMIC_NOUN:
  case ABSTRACT_NONDYNAMIC_NOUN:
  case CONCRETE_NONHUMAN_NOUN:
  case CONCRETE_ARTEFACT_NOUN:
  case CONCRETE_SUBSTANCE_NOUN:
  case CONCRETE_OTHER_NOUN:
  case BROAD_CONCRETE_PLACE_NOUN:
  case BROAD_CONCRETE_TIME_NOUN:
  case BROAD_CONCRETE_MEASURE_NOUN:
    return true;
    break;
  default:
    return false;
  }
}

bool wordStats::isStrictAdj() const {
  switch( sem_type ){
  case HUMAN_ADJ:
  case EMO_ADJ:
  case NONHUMAN_SHAPE_ADJ:
  case NONHUMAN_COLOR_ADJ:
  case NONHUMAN_MATTER_ADJ:
  case NONHUMAN_SOUND_ADJ:
  case NONHUMAN_OTHER_ADJ:
    return true;
    break;
  default:
    return false;
  }
}

bool wordStats::isBroadAdj() const {
  switch( sem_type ){
  case HUMAN_ADJ:
  case EMO_ADJ:
  case NONHUMAN_SHAPE_ADJ:
  case NONHUMAN_COLOR_ADJ:
  case NONHUMAN_MATTER_ADJ:
  case NONHUMAN_SOUND_ADJ:
  case NONHUMAN_OTHER_ADJ:
  case TIME_ADJ:
  case PLACE_ADJ:
    return true;
    break;
  default:
    return false;
  }
}

void wordStats::concreetToCSV( ostream& os ) const {
  if ( tag == CGN::N ){
    os << sem_type << ",";
  }
  else {
    os << "0,";
  }
  os << isStrictNoun() << "," << isBroadNoun() << ",";
  if ( tag == CGN::ADJ ){
    os << sem_type << ",";
  }
  else {
    os << "0,";
  }
  os << isStrictAdj() << "," << isBroadAdj() << ",";
  if ( tag == CGN::WW ){
    os << sem_type << ",";
  }
  else
    os << "0,";
}

void wordStats::persoonlijkheidHeader( ostream& os ) const {
  os << "Pers_ref,Pers_vnw1,Pers_vnw2,Pers_vnw3,Pers_vnw,"
     << "Namen, NER,"
     << "Pers_nw,"
     << "Emo_bvn,Imp,";
}

void wordStats::persoonlijkheidToCSV( ostream& os ) const {
  os << isPersRef << ","
     << (prop == ISPPRON1 ) << ","
     << (prop == ISPPRON2 ) << ","
     << (prop == ISPPRON3 ) << ","
     << (prop == ISPPRON1 || prop == ISPPRON2 || prop == ISPPRON3) << ","
     << (prop == ISNAME) << ",";
  if ( nerProp == NONER )
    os << "0,";
  else
    os << nerProp << ",";

  os << (sem_type == CONCRETE_HUMAN_NOUN ) << ","
     << (sem_type == EMO_ADJ ) << ","
     << isImperative << ",";
}

void wordStats::wordSortHeader( ostream& os ) const {
  os << "POS,Afk,";
}

void wordStats::wordSortToCSV( ostream& os ) const {
  os << tag << ",";
  if ( afkType == NO_A ) {
    os << "0,";
  }
  else {
    os << afkType << ",";
  }
}

void wordStats::miscHeader( ostream& os ) const {
  os << "Ww_vorm,Ww_tt,Vol_dw,Onvol_dw,Infin,Archaisch,Log_prob";
}

void wordStats::miscToCSV( ostream& os ) const {
  if ( wwform == ::NO_VERB ){
    os << "0,";
  }
  else {
    os << wwform << ",";
  }
  os << (prop == ISPVTGW) << ",";
  os << (prop == ISVD ) << ","
     << (prop == ISOD) << ","
     << (prop == ISINF ) << ",";
  os << archaic << ",";
  if ( logprob10 == NA )
    os << "NA,";
  else
    os << logprob10 << ",";
}

void na( ostream& os, int cnt ){
  for ( int i=0; i < cnt; ++i ){
    os << "NA,";
  }
  os << endl;
}

void wordStats::toCSV( ostream& os ) const {
  os << '"' << word << '"';
  os << ",";
  if ( parseFail ){
    na( os, 59 );
    return;
  }
  os << '"' << lemma << '"';
  os << ",";
  os << '"' << full_lemma << '"';
  os << ",";
  os << '"' << morphemes << '"';
  os << ",";
  wordSortToCSV( os );
  wordDifficultiesToCSV( os );
  coherenceToCSV( os );
  concreetToCSV( os );
  persoonlijkheidToCSV( os );
  miscToCSV( os );
  os << endl;
}

struct structStats: public basicStats {
  structStats( int index, FoliaElement *el, const string& cat ):
    basicStats( index, el, cat ),
    wordCnt(0),
    sentCnt(0),
    parseFailCnt(0),
    vdCnt(0),odCnt(0),
    infCnt(0), presentCnt(0), pastCnt(0), subjonctCnt(0),
    nameCnt(0),
    pron1Cnt(0), pron2Cnt(0), pron3Cnt(0),
    passiveCnt(0),modalCnt(0),timeVCnt(0),koppelCnt(0),
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
    tempConnCnt(0),
    opsomConnCnt(0),
    contrastConnCnt(0),
    compConnCnt(0),
    causeConnCnt(0),
    timeSitCnt(0),
    spaceSitCnt(0),
    causeSitCnt(0),
    emoSitCnt(0),
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
    al_gem(NA),
    al_max(NA),
    broadNounCnt(0),
    strictNounCnt(0),
    broadAdjCnt(0),
    strictAdjCnt(0),
    subjectiveAdjCnt(0),
    abstractWwCnt(0),
    concreteWwCnt(0),
    undefinedWwCnt(0),
    stateCnt(0),
    actionCnt(0),
    processCnt(0),
    humanAdjCnt(0),
    emoAdjCnt(0),
    nonhumanAdjCnt(0),
    shapeAdjCnt(0),
    colorAdjCnt(0),
    matterAdjCnt(0),
    soundAdjCnt(0),
    nonhumanOtherAdjCnt(0),
    techAdjCnt(0),
    timeAdjCnt(0),
    placeAdjCnt(0),
    specPosAdjCnt(0),
    specNegAdjCnt(0),
    posAdjCnt(0),
    negAdjCnt(0),
    epiPosAdjCnt(0),
    epiNegAdjCnt(0),
    moreAdjCnt(0),
    lessAdjCnt(0),
    abstractAdjCnt(0),
    undefinedNounCnt(0),
    uncoveredNounCnt(0),
    undefinedAdjCnt(0),
    uncoveredAdjCnt(0),
    uncoveredVerbCnt(0),
    humanCnt(0),
    nonHumanCnt(0),
    artefactCnt(0),
    substanceCnt(0),
    timeCnt(0),
    placeCnt(0),
    measureCnt(0),
    dynamicCnt(0),
    nonDynamicCnt(0),
    institutCnt(0),
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
    prepExprCnt(0),
    word_mtld(0),
    lemma_mtld(0),
    content_mtld(0),
    name_mtld(0),
    temp_conn_mtld(0),
    reeks_conn_mtld(0),
    contr_conn_mtld(0),
    comp_conn_mtld(0),
    cause_conn_mtld(0),
    tijd_sit_mtld(0),
    ruimte_sit_mtld(0),
    cause_sit_mtld(0),
    emotion_sit_mtld(0),
    nerCnt(0)
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
  double get_al_gem() const { return al_gem; };
  double get_al_max() const { return al_max; };
  virtual double getMeanAL() const;
  virtual double getHighestAL() const;
  void calculate_MTLDs();
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
  int timeVCnt;
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
  int tempConnCnt;
  int opsomConnCnt;
  int contrastConnCnt;
  int compConnCnt;
  int causeConnCnt;
  int timeSitCnt;
  int spaceSitCnt;
  int causeSitCnt;
  int emoSitCnt;
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
  double al_gem;
  double al_max;
  int broadNounCnt;
  int strictNounCnt;
  int broadAdjCnt;
  int strictAdjCnt;
  int subjectiveAdjCnt;
  int abstractWwCnt;
  int concreteWwCnt;
  int undefinedWwCnt;
  int stateCnt;
  int actionCnt;
  int processCnt;
  int humanAdjCnt;
  int emoAdjCnt;
  int nonhumanAdjCnt;
  int shapeAdjCnt;
  int colorAdjCnt;
  int matterAdjCnt;
  int soundAdjCnt;
  int nonhumanOtherAdjCnt;
  int techAdjCnt;
  int timeAdjCnt;
  int placeAdjCnt;
  int specPosAdjCnt;
  int specNegAdjCnt;
  int posAdjCnt;
  int negAdjCnt;
  int epiPosAdjCnt;
  int epiNegAdjCnt;
  int moreAdjCnt;
  int lessAdjCnt;
  int abstractAdjCnt;
  int undefinedNounCnt;
  int uncoveredNounCnt;
  int undefinedAdjCnt;
  int uncoveredAdjCnt;
  int uncoveredVerbCnt;
  int humanCnt;
  int nonHumanCnt;
  int artefactCnt;
  int substanceCnt;
  int timeCnt;
  int placeCnt;
  int measureCnt;
  int dynamicCnt;
  int nonDynamicCnt;
  int institutCnt;
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
  map<string,int> unique_tijd_sits;
  map<string,int> unique_ruimte_sits;
  map<string,int> unique_cause_sits;
  map<string,int> unique_emotion_sits;
  map<string,int> unique_temp_conn;
  map<string,int> unique_reeks_conn;
  map<string,int> unique_contr_conn;
  map<string,int> unique_comp_conn;
  map<string,int> unique_cause_conn;
  map<string,int> unique_words;
  map<string,int> unique_lemmas;
  double word_mtld;
  double lemma_mtld;
  double content_mtld;
  double name_mtld;
  double temp_conn_mtld;
  double reeks_conn_mtld;
  double contr_conn_mtld;
  double comp_conn_mtld;
  double cause_conn_mtld;
  double tijd_sit_mtld;
  double ruimte_sit_mtld;
  double cause_sit_mtld;
  double emotion_sit_mtld;
  map<NerProp, int> ners;
  int nerCnt;
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

double structStats::getMeanAL() const {
  double sum = 0;
  for ( size_t i=0; i < sv.size(); ++i ){
    double val = sv[i]->get_al_gem();
    if ( val != NA ){
      sum += val;
    }
  }
  if ( sum == 0 )
    return NA;
  else
    return sum/sv.size();
}

double structStats::getHighestAL() const {
  double sum = 0;
  for ( size_t i=0; i < sv.size(); ++i ){
    double val = sv[i]->get_al_max();
    if ( val != NA ){
      sum += val;
    }
  }
  if ( sum == 0 )
    return NA;
  else
    return sum/sv.size();
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
  timeVCnt += ss->timeVCnt;
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
  tempConnCnt += ss->tempConnCnt;
  opsomConnCnt += ss->opsomConnCnt;
  contrastConnCnt += ss->contrastConnCnt;
  compConnCnt += ss->compConnCnt;
  causeConnCnt += ss->causeConnCnt;
  timeSitCnt += ss->timeSitCnt;
  spaceSitCnt += ss->spaceSitCnt;
  causeSitCnt += ss->causeSitCnt;
  emoSitCnt += ss->emoSitCnt;
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
  strictNounCnt += ss->strictNounCnt;
  broadNounCnt += ss->broadNounCnt;
  strictAdjCnt += ss->strictAdjCnt;
  broadAdjCnt += ss->broadAdjCnt;
  subjectiveAdjCnt += ss->subjectiveAdjCnt;
  abstractWwCnt += ss->abstractWwCnt;
  concreteWwCnt += ss->concreteWwCnt;
  undefinedWwCnt += ss->undefinedWwCnt;
  stateCnt += ss->stateCnt;
  actionCnt += ss->actionCnt;
  processCnt += ss->processCnt;
  humanAdjCnt += ss->humanAdjCnt;
  emoAdjCnt += ss->emoAdjCnt;
  nonhumanAdjCnt += ss->nonhumanAdjCnt;
  shapeAdjCnt += ss->shapeAdjCnt;
  colorAdjCnt += ss->colorAdjCnt;
  matterAdjCnt += ss->matterAdjCnt;
  soundAdjCnt += ss->soundAdjCnt;
  nonhumanOtherAdjCnt += ss->nonhumanOtherAdjCnt;
  techAdjCnt += ss->techAdjCnt;
  timeAdjCnt += ss->timeAdjCnt;
  placeAdjCnt += ss->placeAdjCnt;
  specPosAdjCnt += ss->specPosAdjCnt;
  specNegAdjCnt += ss->specNegAdjCnt;
  posAdjCnt += ss->posAdjCnt;
  negAdjCnt += ss->negAdjCnt;
  epiPosAdjCnt += ss->epiPosAdjCnt;
  epiNegAdjCnt += ss->epiNegAdjCnt;
  moreAdjCnt += ss->moreAdjCnt;
  lessAdjCnt += ss->lessAdjCnt;
  abstractAdjCnt += ss->abstractAdjCnt;
  undefinedNounCnt += ss->undefinedNounCnt;
  uncoveredNounCnt += ss->uncoveredNounCnt;
  undefinedAdjCnt += ss->undefinedAdjCnt;
  uncoveredAdjCnt += ss->uncoveredAdjCnt;
  uncoveredVerbCnt += ss->uncoveredVerbCnt;
  humanCnt += ss->humanCnt;
  nonHumanCnt += ss->nonHumanCnt;
  artefactCnt += ss->artefactCnt;
  substanceCnt += ss->substanceCnt;
  timeCnt += ss->timeCnt;
  placeCnt += ss->placeCnt;
  measureCnt += ss->measureCnt;
  dynamicCnt += ss->dynamicCnt;
  nonDynamicCnt += ss->nonDynamicCnt;
  institutCnt += ss->institutCnt;
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
  nerCnt += ss->nerCnt;
  sv.push_back( ss );
  aggregate( heads, ss->heads );
  aggregate( unique_names, ss->unique_names );
  aggregate( unique_contents, ss->unique_contents );
  aggregate( unique_words, ss->unique_words );
  aggregate( unique_lemmas, ss->unique_lemmas );
  aggregate( unique_tijd_sits, ss->unique_tijd_sits );
  aggregate( unique_ruimte_sits, ss->unique_ruimte_sits );
  aggregate( unique_cause_sits, ss->unique_cause_sits );
  aggregate( unique_emotion_sits, ss->unique_emotion_sits );
  aggregate( unique_temp_conn, ss->unique_temp_conn );
  aggregate( unique_reeks_conn, ss->unique_reeks_conn );
  aggregate( unique_contr_conn, ss->unique_contr_conn );
  aggregate( unique_comp_conn, ss->unique_comp_conn );
  aggregate( unique_cause_conn, ss->unique_cause_conn );
  aggregate( ners, ss->ners );
  aggregate( afks, ss->afks );
  aggregate( distances, ss->distances );
  al_gem = getMeanAL();
  al_max = getHighestAL();
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
      if ( pos->second != NA )
	result += pos->second;
    }
    cerr << "MM to string " << result << "/" << len << endl;
    return toString( result/double(len) );
  }
  else
    return "NA";
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
  addOneMetric( doc, el, "pers_pron_3_count", toString(pron3Cnt) );
  addOneMetric( doc, el, "passive_count", toString(passiveCnt) );
  addOneMetric( doc, el, "modal_count", toString(modalCnt) );
  addOneMetric( doc, el, "time_count", toString(timeVCnt) );
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
  addOneMetric( doc, el, "temporal_connector_count", toString(tempConnCnt) );
  addOneMetric( doc, el, "reeks_connector_count", toString(opsomConnCnt) );
  addOneMetric( doc, el, "contrast_connector_count", toString(contrastConnCnt) );
  addOneMetric( doc, el, "comparatief_connector_count", toString(compConnCnt) );
  addOneMetric( doc, el, "causaal_connector_count", toString(causeConnCnt) );
  addOneMetric( doc, el, "time_situation_count", toString(timeSitCnt) );
  addOneMetric( doc, el, "space_situation_count", toString(spaceSitCnt) );
  addOneMetric( doc, el, "cause_situation_count", toString(causeSitCnt) );
  addOneMetric( doc, el, "emotion_situation_count", toString(emoSitCnt) );
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

  addOneMetric( doc, el, "broad_adj", toString(broadAdjCnt) );
  addOneMetric( doc, el, "strict_adj", toString(strictAdjCnt) );
  addOneMetric( doc, el, "human_adj_count", toString(humanAdjCnt) );
  addOneMetric( doc, el, "emo_adj_count", toString(emoAdjCnt) );
  addOneMetric( doc, el, "nonhuman_adj_count", toString(nonhumanAdjCnt) );
  addOneMetric( doc, el, "shape_adj_count", toString(shapeAdjCnt) );
  addOneMetric( doc, el, "color_adj_count", toString(colorAdjCnt) );
  addOneMetric( doc, el, "matter_adj_count", toString(matterAdjCnt) );
  addOneMetric( doc, el, "sound_adj_count", toString(soundAdjCnt) );
  addOneMetric( doc, el, "other_nonhuman_adj_count", toString(nonhumanOtherAdjCnt) );
  addOneMetric( doc, el, "techn_adj_count", toString(techAdjCnt) );
  addOneMetric( doc, el, "time_adj_count", toString(timeAdjCnt) );
  addOneMetric( doc, el, "place_adj_count", toString(placeAdjCnt) );
  addOneMetric( doc, el, "pos_spec_adj_count", toString(specPosAdjCnt) );
  addOneMetric( doc, el, "neg_spec_adj_count", toString(specNegAdjCnt) );
  addOneMetric( doc, el, "pos_adj_count", toString(posAdjCnt) );
  addOneMetric( doc, el, "neg_adj_count", toString(negAdjCnt) );
  addOneMetric( doc, el, "pos_epi_adj_count", toString(epiPosAdjCnt) );
  addOneMetric( doc, el, "neg_epi_adj_count", toString(epiNegAdjCnt) );
  addOneMetric( doc, el, "versterker_adj_count", toString(moreAdjCnt) );
  addOneMetric( doc, el, "verzwakker_adj_count", toString(lessAdjCnt) );
  addOneMetric( doc, el, "abstract_adj", toString(abstractAdjCnt) );
  addOneMetric( doc, el, "undefined_adj_count", toString(undefinedAdjCnt) );
  addOneMetric( doc, el, "covered_adj_count", toString(adjCnt-uncoveredAdjCnt) );
  addOneMetric( doc, el, "uncovered_adj_count", toString(uncoveredAdjCnt) );

  addOneMetric( doc, el, "broad_noun", toString(broadNounCnt) );
  addOneMetric( doc, el, "strict_noun", toString(strictNounCnt) );
  addOneMetric( doc, el, "human_nouns_count", toString(humanCnt) );
  addOneMetric( doc, el, "nonhuman_nouns_count", toString(nonHumanCnt) );
  addOneMetric( doc, el, "artefact_nouns_count", toString(artefactCnt) );
  addOneMetric( doc, el, "substance_nouns_count", toString(substanceCnt) );
  addOneMetric( doc, el, "time_nouns_count", toString(timeCnt) );
  addOneMetric( doc, el, "place_nouns_count", toString(placeCnt) );
  addOneMetric( doc, el, "measure_nouns_count", toString(measureCnt) );
  addOneMetric( doc, el, "dynamic_nouns_count", toString(dynamicCnt) );
  addOneMetric( doc, el, "nondynamic_nouns_count", toString(nonDynamicCnt) );
  addOneMetric( doc, el, "institut_nouns_count", toString(institutCnt) );
  addOneMetric( doc, el, "undefined_nouns_count", toString(undefinedNounCnt) );
  addOneMetric( doc, el, "covered_nouns_count", toString(nounCnt+nameCnt-uncoveredNounCnt) );
  addOneMetric( doc, el, "uncovered_nouns_count", toString(uncoveredNounCnt) );

  addOneMetric( doc, el, "abstract_ww", toString(abstractWwCnt) );
  addOneMetric( doc, el, "concrete_ww", toString(concreteWwCnt) );
  addOneMetric( doc, el, "undefined_ww", toString(undefinedWwCnt) );
  addOneMetric( doc, el, "state_count", toString(stateCnt) );
  addOneMetric( doc, el, "action_count", toString(actionCnt) );
  addOneMetric( doc, el, "process_count", toString(processCnt) );
  addOneMetric( doc, el, "covered_verb_count", toString(verbCnt-uncoveredVerbCnt) );
  addOneMetric( doc, el, "uncovered_verb_count", toString(uncoveredVerbCnt) );
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
  addOneMetric( doc, el, "verb_svp_dist", MMtoString( distances, VERB_SVP ) );
  addOneMetric( doc, el, "verb_cop_dist", MMtoString( distances, VERB_PREDC_N ) );
  addOneMetric( doc, el, "verb_adj_dist", MMtoString( distances, VERB_PREDC_A ) );
  addOneMetric( doc, el, "verb_bw_mod_dist", MMtoString( distances, VERB_MOD_BW ) );
  addOneMetric( doc, el, "verb_adv_mod_dist", MMtoString( distances, VERB_MOD_A ) );
  addOneMetric( doc, el, "verb_noun_dist", MMtoString( distances, VERB_NOUN ) );

  addOneMetric( doc, el, "deplen", toMString( al_gem ) );
  addOneMetric( doc, el, "max_deplen", toMString( al_max ) );
  for ( size_t i=0; i < sv.size(); ++i ){
    sv[i]->addMetrics();
  }
}

void structStats::CSVheader( ostream& os, const string& intro ) const {
  os << intro << ",Alpino_status,";
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
  os << "Let_per_wrd,Wrd_per_let,Let_per_wrd_zn,Wrd_per_let_zn,"
     << "Morf_per_wrd,Wrd_per_morf,Morf_per_wrd_zn,Wrd_per_morf_zn,"
     << "Sam_delen_per_wrd,Sam_d,"
     << "Freq50_staph,Freq65_Staph,Freq77_Staph,Feq80_Staph,"
     << "Wrd_freq_log,Wrd_freq_zn_log,Lem_freq_log,Lem_freq_zn_log,"
     << "Freq1000,Freq2000,Freq3000,Freq5000,Freq10000,Freq20000,"
     << "so_word_suc,so_word_net,"
     << "so_sent_suc,so_sent_net,so_sent_ctx,"
     << "so_par_suc,so_par_net,so_par_ctx,";
}

void structStats::wordDifficultiesToCSV( ostream& os ) const {
  os << std::showpoint
     << proportion( charCnt, wordCnt ) << ","
     << proportion( wordCnt, charCnt ) <<  ","
     << proportion( charCntExNames, (wordCnt-nameCnt) ) << ","
     << proportion( (wordCnt - nameCnt), charCntExNames ) <<  ","
     << proportion( morphCnt, wordCnt ) << ","
     << proportion( wordCnt, morphCnt ) << ","
     << proportion( morphCntExNames, (wordCnt-nameCnt) ) << ","
     << proportion( (wordCnt-nameCnt), morphCntExNames ) << ","
     << proportion( compPartCnt, wordCnt ) << ","
     << density( compCnt, wordCnt ) << ",";

  os << proportion( f50Cnt, wordCnt ) << ",";
  os << proportion( f65Cnt, wordCnt ) << ",";
  os << proportion( f77Cnt, wordCnt ) << ",";
  os << proportion( f80Cnt, wordCnt ) << ",";
  os << proportion( word_freq_log, wordCnt ) << ",";
  os << proportion( word_freq_log_n, (wordCnt-nameCnt) ) << ",";
  os << proportion( lemma_freq_log, wordCnt ) << ",";
  os << proportion( lemma_freq_log_n, (wordCnt-nameCnt) ) << ",";
  os << proportion( top1000Cnt, wordCnt ) << ",";
  os << proportion( top2000Cnt, wordCnt ) << ",";
  os << proportion( top3000Cnt, wordCnt ) << ",";
  os << proportion( top5000Cnt, wordCnt ) << ",";
  os << proportion( top10000Cnt, wordCnt ) << ",";
  os << proportion( top20000Cnt, wordCnt ) << ",";
  os << toMString(lsa_word_suc) << "," << toMString(lsa_word_net) << ",";
  os << toMString(lsa_sent_suc) << "," << toMString(lsa_sent_net) << ","
     << toMString(lsa_sent_ctx) << ",";
  os << toMString(lsa_par_suc) << "," << toMString(lsa_par_net) << ","
     << toMString(lsa_par_ctx) << ",";
}

void structStats::sentDifficultiesHeader( ostream& os ) const {
  os << "Wrd_per_zin,Wrd_per_dz,Zin_per_wrd,Dzin_per_wrd,"
     << "Wrd_per_nwg,Ov_bijzin_d,Ov_bijzin_per_zin,"
     << "Betr_bijzin_d,Betr_bijzin_dz,"
     << "Pv_d,Pv_per_zin,";
  if ( isSentence() ){
    os << "D_level,";
  }
  else {
    os  << "D_level,D_level_gt4_p,D_level_gt4_r,";
  }
  os << "Nom_d,Lijdv_d,Lijdv_dz,Ontk_zin_d,Ontk_zin_dz,"
     << "Ontk_morf_d,Ont_morf_dz,Ontk_tot_d,Ontk_tot_dz,"
     << "Meerv_ont_d,Meerv_ont_dz,"
     << "AL_sub_ww,AL_ob_ww,AL_indirob_ww,AL_ww_vzg,"
     << "AL_lidw_znw,AL_vz_znw,AL_pv_hww,"
     << "AL_vg_wwbijzin,AL_vg_conj,AL_vg_wwhoofdzin,AL_znw_bijzin,AL_ww_schdw,"
     << "AL_ww_znwpred,AL_ww_bnwpred,AL_ww_bnwbwp,AL_ww_bwbwp,AL_ww_znwbwp,"
     << "AL_gem,AL_max,";
}

void structStats::sentDifficultiesToCSV( ostream& os ) const {
  double clauseCnt = pastCnt + presentCnt;
  os << proportion( wordCnt, sentCnt ) << ","
     << proportion( wordCnt, clauseCnt ) << ","
     << proportion( sentCnt, wordCnt )  << ","
     << proportion( clauseCnt, wordCnt )  << ",";
  os << proportion( wordCnt, npCnt ) << ",";
  os << density( onderCnt, wordCnt ) << ","
     << density( onderCnt, sentCnt ) << ","
     << density( betrCnt, wordCnt ) << ","
     << proportion( betrCnt, clauseCnt ) << ","
     << density( clauseCnt, wordCnt ) << ","
     << proportion( clauseCnt, sentCnt ) << ","
     << proportion( dLevel, sentCnt ) << ",";
  if ( !isSentence() ){
    os << proportion( dLevel_gt4, sentCnt ) << ",";
    os << ratio( dLevel_gt4, sentCnt ) << ",";
  }
  os << density( nominalCnt, wordCnt ) << ","
     << density( passiveCnt, wordCnt ) << ",";
  os << proportion( passiveCnt, clauseCnt ) << ",";
  os << density( propNegCnt, wordCnt ) << ","
     << proportion( propNegCnt, clauseCnt ) << ","
     << density( morphNegCnt, wordCnt ) << ","
     << proportion( morphNegCnt, clauseCnt ) << ","
     << density( propNegCnt+morphNegCnt, wordCnt ) << ","
     << proportion( propNegCnt+morphNegCnt, clauseCnt ) << ","
     << density( multiNegCnt, wordCnt ) << ","
     << proportion( multiNegCnt, clauseCnt ) << ",";
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
  os << MMtoString( distances, VERB_SVP ) << ",";
  os << MMtoString( distances, VERB_PREDC_N ) << ",";
  os << MMtoString( distances, VERB_PREDC_A ) << ",";
  os << MMtoString( distances, VERB_MOD_A ) << ",";
  os << MMtoString( distances, VERB_MOD_BW ) << ",";
  os << MMtoString( distances, VERB_NOUN ) << ",";
  os << toMString( al_gem ) << ",";
  os << toMString( al_max ) << ",";
}

void structStats::infoHeader( ostream& os ) const {
  os << "TTR_wrd,MTLD_wrd,TTR_lem,MTLD_lem,"
     << "TTR_namen,MTLD_namen,TTR_inhwrd,MTLD_inhwrd,"
     << "Inhwrd_r,Inhwrd_d,Inhwrd_dz,"
     << "Zeldz_index,Bijw_bep_d,Bijw_bep_dz,"
     << "Attr_bijv_nw_d,Attr_bijv_nw_dz,Bijv_bep_d,Bijv_bep_dz,"
     << "Ov_bijv_bep_d,Ov_bijv_bep_dz,Nwg_d,Nwg_dz,";
}

void structStats::informationDensityToCSV( ostream& os ) const {
  double clauseCnt = pastCnt + presentCnt;
  os << proportion( unique_words.size(), wordCnt ) << ",";
  os << word_mtld << ",";
  os << proportion( unique_lemmas.size(), wordCnt ) << ",";
  os << lemma_mtld << ",";
  os << proportion( unique_names.size(), nameCnt ) << ",";
  os << name_mtld << ",";
  os << proportion( unique_contents.size(), contentCnt ) << ",";
  os << content_mtld << ",";
  os << ratio( contentCnt, wordCnt ) << ",";
  os << density( contentCnt, wordCnt ) << ",";
  os << proportion( contentCnt, clauseCnt ) << ",";
  os << rarity( settings.rarityLevel ) << ",";
  os << density( vcModCnt, wordCnt ) << ",";
  os << proportion( vcModCnt, clauseCnt ) << ",";
  os << density( adjNpModCnt, wordCnt ) << ",";
  os << proportion( adjNpModCnt, clauseCnt ) << ",";
  os << density( npModCnt, wordCnt ) << ",";
  os << proportion( npModCnt, clauseCnt ) << ",";
  os << density( (npModCnt-adjNpModCnt), wordCnt ) << ",";
  os << proportion( (npModCnt-adjNpModCnt), clauseCnt ) << ",";
  os << proportion( npCnt, wordCnt ) << ",";
  os << proportion( npCnt, clauseCnt ) << ",";
}


void structStats::coherenceHeader( ostream& os ) const {
  os << "Conn_temp_d,Conn_temp_dz,Conn_temp_TTR,Conn_temp_MTLD,"
     << "Conn_reeks_d,Conn_reeks_dz,Conn_reeks_TTR,Conn_reeks_MTLD,"
     << "Conn_contr_d,Conn_contr_dz,Conn_contr_TTR,Conn_contr_MTLD,"
     << "Conn_comp_d,Conn_comp_dz,Conn_comp_TTR,Conn_comp_MTLD,"
     << "Conn_caus_d,Conn_caus_dz,Conn_caus_TTR,Conn_caus_MTLD,"
     << "Causaal_d,Ruimte_d,Tijd_d,Emotie_d,"
     << "Causaal_TTR,Causaal_MTLD,"
     << "Ruimte_TTR,Ruimte_MTLD,"
     << "Tijd_TTR,Tijd_MTLD,"
     << "Emotie_TTR,Emotie_MTLD,"
     << "Vnw_ref_d,Vnw_ref_dz,"
     << "Arg_over_vzin_d,Arg_over_vzin_dz,Lem_over_vzin_d,Lem_over_vzin_dz,"
     << "Arg_over_buf_d,Arg_over_buf_dz,Lem_over_buf_d,Lem_over_buf_dz,"
     << "Onbep_nwg_p,Onbep_nwg_r,Onbep_nwg_dz,";
}

void structStats::coherenceToCSV( ostream& os ) const {
  double clauseCnt = pastCnt + presentCnt;
  os << density( tempConnCnt, wordCnt ) << ","
     << proportion( tempConnCnt, clauseCnt ) << ","
     << proportion( unique_temp_conn.size(), tempConnCnt ) << ","
     << temp_conn_mtld << ","
     << density( opsomConnCnt, wordCnt ) << ","
     << proportion( opsomConnCnt, clauseCnt ) << ","
     << proportion( unique_reeks_conn.size(), opsomConnCnt ) << ","
     << reeks_conn_mtld << ","
     << density( contrastConnCnt, wordCnt ) << ","
     << proportion( contrastConnCnt, clauseCnt ) << ","
     << proportion( unique_contr_conn.size(), contrastConnCnt ) << ","
     << contr_conn_mtld << ","
     << density( compConnCnt, wordCnt ) << ","
     << proportion( compConnCnt, clauseCnt ) << ","
     << proportion( unique_comp_conn.size(), compConnCnt ) << ","
     << comp_conn_mtld << ","
     << density( causeConnCnt, wordCnt ) << ","
     << proportion( causeConnCnt, clauseCnt ) << ","
     << proportion( unique_cause_conn.size(), causeConnCnt ) << ","
     << cause_conn_mtld << ","
     << density( causeSitCnt, wordCnt ) << ","
     << density( spaceSitCnt, wordCnt ) << ","
     << density( timeSitCnt, wordCnt ) << ","
     << density( emoSitCnt, wordCnt ) << ","
     << proportion( unique_cause_sits.size(), causeSitCnt ) << ","
     << cause_sit_mtld << ","
     << proportion( unique_ruimte_sits.size(), spaceSitCnt ) << ","
     << ruimte_sit_mtld << ","
     << proportion( unique_tijd_sits.size(), timeSitCnt ) << ","
     << tijd_sit_mtld << ","
     << proportion( unique_emotion_sits.size(), emoSitCnt ) << ","
     << emotion_sit_mtld << ","
     << density( pronRefCnt, wordCnt ) << ","
     << proportion( pronRefCnt, clauseCnt ) << ",";
  if ( isSentence() ){
    if ( index == 0 ){
      os << "NA,NA,"
	 << "NA,NA,";
    }
    else {
      os << double(wordOverlapCnt) << ",NA,"
	 << double(lemmaOverlapCnt) << ",NA,";
    }
  }
  else {
    os << density( wordOverlapCnt, wordCnt ) << ","
       << proportion( wordOverlapCnt, sentCnt ) << ",";
    os << density( lemmaOverlapCnt, wordCnt ) << ","
       << proportion( lemmaOverlapCnt, sentCnt ) << ",";
  }
  if ( !isDocument() ){
    os << "NA,NA,NA,NA,";
  }
  else {
    os << density( word_overlapCnt(), wordCnt - settings.overlapSize ) << ","
       << proportion( word_overlapCnt(), clauseCnt ) << ","
       << density( lemma_overlapCnt(), wordCnt  - settings.overlapSize ) << ","
       << proportion( lemma_overlapCnt(), clauseCnt )<< ",";
  }
  os << proportion( indefNpCnt, npCnt ) << ",";
  os << ratio( indefNpCnt, npCnt ) << ",";
  os << density( indefNpCnt, wordCnt ) << ",";
}

void structStats::concreetHeader( ostream& os ) const {
  os << "Conc_nw_strikt_p,Conc_nw_strikt_d,";
  os << "Conc_nw_ruim_p,Conc_nw_ruim_d,";
  os << "PlantDier_nw_p,PlantDier_nw_d,";
  os << "Gebr_nw_p,Gebr_nw_d,";
  os << "Subst_nw_p,Subst_nw_d,";
  os << "Plaats_nw_p,Plaats_nw_d,";
  os << "Tijd_nw_p,Tijd_nw_d,";
  os << "Maat_nw_p,Maat_nw_d,";
  os << "Gebeuren_nw_p,Gebeuren_nw_d,";
  os << "Organisatie_nw_p,Organisatie_nw_d,";
  os << "Abstract_nw_p,Abstract_nw_d,";
  os << "Undefined_nw_p,";
  os << "Gedekte_nw_p,";
  os << "Waarn_mens_bvnw_p,Waarn_mens_bvnw_d,";
  os << "Emosoc_bvnw_p,Emosoc_bvnw_d,";
  os << "Waarn_nmens_bvnw_p,Waarn_nmens_bvnw_d,";
  os << "Vorm_omvang_bvnw_p,Vorm_omvan_bvnw_d,";
  os << "Kleur_bvnw_p,Kleur_bvnw_d,";
  os << "Stof_bvnw_p,Stof_bvnw_d,";
  os << "Geluid_bvnw_p,Geluid_bvnw_d,";
  os << "Waarn_nmens_ov_bvnw_p,Waarn_nmens_ov_bvnw_d,";
  os << "Technisch_bvnw_p,Technisch_bvnw_d,";
  os << "Time_bvnw_p,Time_bvnw_d,";
  os << "Place_bvnw_p,Place_bvnw_d,";
  os << "Spec_positief_bvnw_p,Spec_positief_bvnw_d,";
  os << "Spec_negatief_bvnw_p,Spec_negatief_bvnw_d,";
  os << "Alg_positief_bvnw_p,Alg_positief_bvnw_d,";
  os << "Alg_negatief_bvnw_p,Alg_negatief_bvnw_d,";
  os << "Ep_positief_bvnw_p,Ep_positief_bvnw_d,";
  os << "Ep_negatief_bvnw_p,Ep_negatief_bvnw_d,";
  os << "Versterker_bvnw_p,Versterker_bvnw_d,";
  os << "Verzwakker_bvnw_p,Verzwakker_bvnw_d,";
  os << "Abstract_bvnw_p,Abstract_bvnw_d,";
  os << "Spec_ev_bvnw_p,Spec_ev_bvnw_d,";
  os << "Alg_ev_bvnw_p,Alg_ev_bvnw_d,";
  os << "Ep_ev_bvnw_p,Ep_ev_bvnw_d,";
  os << "Conc_bvnw_strikt_p,Conc_bvnw_strikt_d,";
  os << "Conc_bvnw_ruim_p,Conc_bvnw_ruim_d,";
  os << "Subj_bvnw_p,Subj_bvnw_d,";
  os << "Undefined_bvnw_p,";
  os << "Gelabeld_bvnw_p,";
  os << "Gedekt_bvnw_p,";
  os << "Conc_ww_p,Conc_ww_d,";
  os << "Abstr_ww_p,Abstr_ww_d,";
  os << "Undefined_ww_p,";
  os << "Gedekte_ww_p,";
  os << "Conc_tot_p,Conc_tot_d,";
}

void structStats::concreetToCSV( ostream& os ) const {
  os << proportion( strictNounCnt, nounCnt+nameCnt ) << ",";
  os << density( strictNounCnt, wordCnt ) << ",";
  os << proportion( broadNounCnt, nounCnt+nameCnt ) << ",";
  os << density( broadNounCnt, wordCnt ) << ",";
  os << proportion( nonHumanCnt, nounCnt+nameCnt ) << ",";
  os << density( nonHumanCnt, wordCnt ) << ",";
  os << proportion( artefactCnt, nounCnt+nameCnt ) << ",";
  os << density( artefactCnt, wordCnt ) << ",";
  os << proportion( substanceCnt, nounCnt+nameCnt ) << ",";
  os << density( substanceCnt, wordCnt ) << ",";
  os << proportion( placeCnt, nounCnt+nameCnt ) << ",";
  os << density( placeCnt, wordCnt ) << ",";
  os << proportion( timeCnt, nounCnt+nameCnt ) << ",";
  os << density( timeCnt, wordCnt ) << ",";
  os << proportion( measureCnt, nounCnt+nameCnt ) << ",";
  os << density( measureCnt, wordCnt ) << ",";
  os << proportion( institutCnt, nounCnt+nameCnt ) << ",";
  os << density( institutCnt, wordCnt ) << ",";
  os << proportion( dynamicCnt, nounCnt+nameCnt ) << ",";
  os << density( dynamicCnt, wordCnt ) << ",";
  os << proportion( nonDynamicCnt, nounCnt+nameCnt ) << ",";
  os << density( nonDynamicCnt, wordCnt ) << ",";
  os << proportion( undefinedNounCnt, nounCnt + nameCnt ) << ",";
  os << proportion( (nounCnt+nameCnt-uncoveredNounCnt), nounCnt + nameCnt ) << ",";

  os << proportion( humanAdjCnt,adjCnt ) << ",";
  os << density( humanAdjCnt,wordCnt ) << ",";
  os << proportion( emoAdjCnt,adjCnt ) << ",";
  os << density( emoAdjCnt,wordCnt ) << ",";
  os << proportion( nonhumanAdjCnt,adjCnt ) << ",";
  os << density( nonhumanAdjCnt,wordCnt ) << ",";
  os << proportion( shapeAdjCnt,adjCnt ) << ",";
  os << density( shapeAdjCnt,wordCnt ) << ",";
  os << proportion( colorAdjCnt,adjCnt ) << ",";
  os << density( colorAdjCnt,wordCnt ) << ",";
  os << proportion( matterAdjCnt,adjCnt ) << ",";
  os << density( matterAdjCnt,wordCnt ) << ",";
  os << proportion( soundAdjCnt,adjCnt ) << ",";
  os << density( soundAdjCnt,wordCnt ) << ",";
  os << proportion( nonhumanOtherAdjCnt,adjCnt ) << ",";
  os << density( nonhumanOtherAdjCnt,wordCnt ) << ",";
  os << proportion( techAdjCnt,adjCnt ) << ",";
  os << density( techAdjCnt,wordCnt ) << ",";
  os << proportion( timeAdjCnt,adjCnt ) << ",";
  os << density( timeAdjCnt,wordCnt ) << ",";
  os << proportion( placeAdjCnt,adjCnt ) << ",";
  os << density( placeAdjCnt,wordCnt ) << ",";
  os << proportion( specPosAdjCnt,adjCnt ) << ",";
  os << density( specPosAdjCnt,wordCnt ) << ",";
  os << proportion( specNegAdjCnt,adjCnt ) << ",";
  os << density( specNegAdjCnt,wordCnt ) << ",";
  os << proportion( posAdjCnt,adjCnt ) << ",";
  os << density( posAdjCnt,wordCnt ) << ",";
  os << proportion( negAdjCnt,adjCnt ) << ",";
  os << density( negAdjCnt,wordCnt ) << ",";
  os << proportion( epiPosAdjCnt,adjCnt ) << ",";
  os << density( epiPosAdjCnt,wordCnt ) << ",";
  os << proportion( epiNegAdjCnt,adjCnt ) << ",";
  os << density( epiNegAdjCnt,wordCnt ) << ",";
  os << proportion( moreAdjCnt,adjCnt ) << ",";
  os << density( moreAdjCnt,wordCnt ) << ",";
  os << proportion( lessAdjCnt,adjCnt ) << ",";
  os << density( lessAdjCnt,wordCnt ) << ",";
  os << proportion( abstractAdjCnt,adjCnt ) << ",";
  os << density( abstractAdjCnt,wordCnt ) << ",";
  os << proportion( specPosAdjCnt + specNegAdjCnt ,adjCnt ) << ",";
  os << density( specPosAdjCnt + specNegAdjCnt, wordCnt ) << ",";
  os << proportion( posAdjCnt + negAdjCnt,adjCnt ) << ",";
  os << density( posAdjCnt + negAdjCnt, wordCnt ) << ",";
  os << proportion( epiPosAdjCnt + epiNegAdjCnt ,adjCnt ) << ",";
  os << density( epiPosAdjCnt + epiNegAdjCnt ,wordCnt ) << ",";
  os << proportion( strictAdjCnt, adjCnt ) << ",";
  os << density( strictAdjCnt, wordCnt ) << ",";
  os << proportion( broadAdjCnt, adjCnt ) << ",";
  os << density( broadAdjCnt, wordCnt ) << ",";
  os << proportion( subjectiveAdjCnt ,adjCnt ) << ",";
  os << density( subjectiveAdjCnt, wordCnt ) << ",";
  os << proportion( undefinedAdjCnt, adjCnt ) << ",";
  os << proportion( adjCnt - uncoveredAdjCnt ,adjCnt ) << ",";
  os << proportion( adjCnt-uncoveredAdjCnt+undefinedAdjCnt ,adjCnt ) << ",";
  os << proportion( concreteWwCnt,pastCnt + presentCnt ) << ",";
  os << density( concreteWwCnt,pastCnt + presentCnt ) << ",";
  os << proportion( abstractWwCnt,pastCnt + presentCnt ) << ",";
  os << density( abstractWwCnt,pastCnt + presentCnt ) << ",";
  os << proportion( undefinedWwCnt,pastCnt + presentCnt ) << ",";
  os << proportion( pastCnt+presentCnt - uncoveredVerbCnt,pastCnt + presentCnt ) << ",";
  os << proportion( concreteWwCnt + strictAdjCnt + strictNounCnt, wordCnt ) << ",";
  os << density( concreteWwCnt + strictAdjCnt + strictNounCnt, wordCnt ) << ",";
}

void structStats::persoonlijkheidHeader( ostream& os ) const {
  os << "Pers_ref_d,Pers_vnw1_d,Pers_vnw2_d,Pers_vnw3_d,Pers_vnw_d,"
     << "Namen_p,Names_r,Names_d,"
     << "Pers_namen_p, Pers_namen_d, Plaatsnamen_d, Org_namen_d, Prod_namen_d, Event_namen_d,"
     << "Actieww_p,Actieww_d,Toestww_p,Toestww_d,"
     << "Processww_p,Processww_d,Pers_nw_p,Pers_nw_d,"
     << "Emo_bvnw_p,Emo_bvnw_d,Imp_p,Imp_d,"
     << "Vragen_p,Vragen_d,";
}

void structStats::persoonlijkheidToCSV( ostream& os ) const {
  double clauseCnt = pastCnt + presentCnt;
  os << density( persRefCnt, wordCnt ) << ",";
  os << density( pron1Cnt, wordCnt ) << ",";
  os << density( pron2Cnt, wordCnt ) << ",";
  os << density( pron3Cnt, wordCnt ) << ",";
  os << density( pron1Cnt+pron2Cnt+pron3Cnt, wordCnt ) << ",";

  os << proportion( nameCnt, nounCnt ) << ",";
  os << ratio( nameCnt, wordCnt ) << ",";
  os << density( nameCnt, wordCnt ) << ",";

  int val = at( ners, PER_B );
  os << proportion( val, nerCnt ) << ",";
  os << density( val, wordCnt ) << ",";
  val = at( ners, LOC_B );
  os << density( val, wordCnt ) << ",";
  val = at( ners, ORG_B );
  os << density( val, wordCnt ) << ",";
  val = at( ners, PRO_B );
  os << density( val, wordCnt ) << ",";
  val = at( ners, EVE_B );
  os << density( val, wordCnt ) << ",";

  os << proportion( actionCnt, verbCnt ) << ",";
  os << density( actionCnt, wordCnt) << ",";
  os << proportion( stateCnt, verbCnt ) << ",";
  os << density( stateCnt, wordCnt ) << ",";
  os << proportion( processCnt, verbCnt ) << ",";
  os << density( processCnt, wordCnt ) << ",";
  os << proportion( humanCnt, nounCnt+nameCnt ) << ",";
  os << density( humanCnt, wordCnt ) << ",";
  os << proportion( emoAdjCnt, adjCnt ) << ",";
  os << density( emoAdjCnt, wordCnt ) << ",";
  os << proportion( impCnt, clauseCnt ) << ",";
  os << density( impCnt, wordCnt ) << ",";
  os << proportion( questCnt, sentCnt ) << ",";
  os << density( questCnt, wordCnt ) << ",";
}

void structStats::wordSortHeader( ostream& os ) const {
  os << "Bvnw_d,Vg_d,Vnw_d,Lidw_d,Vz_d,Bijw_d,Tw_d,Nw_d,Ww_d,Tuss_d,Spec_d,"
     << "Interp_d,"
     << "Afk_d,Afk_gen_d,Afk_int_d,Afk_jur_d,Afk_med_d,"
     << "Afk_ond_d,Afk_pol_d,Afk_ov_d,Afk_zorg_d,";
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
  os << "Vzu_d, Vzu_dz, Ww_tt_r,Ww_tt_dz,Ww_mod_d_,Ww_mod_dz,"
     << "Huww_tijd_d,Huww_tijd_dz,Koppelww_d,Koppelww_dz,"
     << "Arch_d,Vol_dw_d,Vol_dw_dz,"
     << "Onvol_dw_d,Onvol_dw_dz,Infin_d,Infin_g,"
     << "Log_prob,Entropie,Perplexiteit,";
}

void structStats::miscToCSV( ostream& os ) const {
  double clauseCnt = pastCnt + presentCnt;
  os << density( prepExprCnt, wordCnt ) << ",";
  os << proportion( prepExprCnt, sentCnt ) << ",";
  os << proportion( presentCnt, pastCnt ) << ",";
  os  << density( presentCnt, wordCnt ) << ","
      << density( modalCnt, wordCnt ) << ",";
  os << proportion( modalCnt, clauseCnt ) << ",";
  os << density( timeVCnt, wordCnt ) << ",";
  os << proportion( timeVCnt, clauseCnt ) << ",";
  os << density( koppelCnt, wordCnt ) << ",";
  os << proportion( koppelCnt, clauseCnt ) << ",";
  os << density( archaicsCnt, wordCnt ) << ","
     << density( vdCnt, wordCnt ) << ",";
  os << proportion( vdCnt, clauseCnt ) << ",";
  os << density( odCnt, wordCnt ) << ",";
  os << proportion( odCnt, clauseCnt ) << ",";
  os << density( infCnt, wordCnt ) << ",";
  os << proportion( infCnt, clauseCnt ) << ",";
  os << proportion( avg_prob10, sentCnt ) << ",";
  os << proportion( entropy, sentCnt ) << ",";
  os << proportion( perplexity, sentCnt ) << ",";
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

// #define DEBUG_LSA

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
      LOG << "LSA: " << category << " lookup '" << call << "' ==> " << result << endl;
#endif
    }
  }
#ifdef DEBUG_LSA
  LOG << "LSA-" << category << "-SUC sum = " << suc << ", size = " << sv.size() << endl;
  LOG << "LSA-" << category << "-NET sum = " << net << ", node count = " << node_count << endl;
  LOG << "LSA-" << category << "-CTX sum = " << ctx << ", size = " << sv.size() << endl;
#endif
  suc = suc/sv.size();
  net = net/node_count;
  ctx = ctx/sv.size();
#ifdef DEBUG_LSA
  LOG << "LSA-" << category << "-SUC result = " << suc << endl;
  LOG << "LSA-" << category << "-NET result = " << net << endl;
  LOG << "LSA-" << category << "-CTX result = " << ctx << endl;
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
#ifdef DEBUG_LSA
  LOG << category << " calculate summary, for " << size << " items" << endl;
  LOG << "word_suc = " << word_suc << endl;
  LOG << "word_net = " << word_net << endl;
  LOG << "sent_suc = " << sent_suc << endl;
  LOG << "sent_net = " << sent_net << endl;
  LOG << "sent_ctx = " << sent_ctx << endl;
  LOG << "par_suc = " << par_suc << endl;
  LOG << "par_net = " << par_net << endl;
  LOG << "par_ctx = " << par_ctx << endl;
#endif
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
  sentStats( int, Sentence *, const sentStats*, const map<string,double>& );
  bool isSentence() const { return true; };
  void resolveConnectives();
  void resolveSituations();
  void setLSAvalues( double, double, double = 0 );
  void resolveLSA( const map<string,double>& );
  void resolveMultiWordAfks();
  void incrementConnCnt( ConnType );
  void addMetrics( ) const;
  bool checkAls( size_t );
  double getMeanAL() const;
  double getHighestAL() const;
  ConnType checkMultiConnectives( const string& );
  SituationType checkMultiSituations( const string& );
  void resolvePrepExpr();
};

//#define DEBUG_MTLD

double calculate_mtld( const vector<string>& v ){
  if ( v.size() == 0 ){
    return 0.0;
  }
  int token_count = 0;
  set<string> unique_tokens;
  double token_factor = 0.0;
  double token_ttr = 1.0;
  for ( size_t i=0; i < v.size(); ++i ){
    ++token_count;
    unique_tokens.insert(v[i]);
    token_ttr = unique_tokens.size() / double(token_count);
#ifdef DEBUG_MTLD
    cerr << v[i] << " types " << unique_tokens.size() << " token "
	 << token_count << " ttr " << token_ttr << endl;
#endif
    if ( token_ttr <= settings.mtld_threshold ){
#ifdef KOIZUMI
      if ( token_count >=10 ){
	++token_factor;
      }
#else
      ++token_factor;
#endif
      token_count = 0;
      token_ttr = 1.0;
      unique_tokens.clear();
    }
  }
  if ( token_count > 0 ){
    // partial result
    if ( token_ttr == 1.0 ){
      ++token_factor;
    }
    else {
      double threshold = ( 1 - token_ttr ) / (1 - settings.mtld_threshold);
      token_factor += threshold;
    }
  }
#ifdef DEBUG_MTLD
  cerr << "Factor = " << token_factor << " #words = " << v.size() << endl;
#endif
  return v.size() / token_factor;
}

double average_mtld( vector<string>& tokens ){
  double mtld1 = calculate_mtld( tokens );
#ifdef DEBUG_MTLD
  cerr << "forward = " << mtld1 << endl;
#endif
  reverse( tokens.begin(), tokens.end() );
  double mtld2 = calculate_mtld( tokens );
#ifdef DEBUG_MTLD
  cerr << "backward = " << mtld1 << endl;
#endif
  double result = (mtld1 + mtld2)/2.0;
#ifdef DEBUG_MTLD
  cerr << "average mtld = " << result << endl;
#endif
  return result;
}

void structStats::calculate_MTLDs() {
  const vector<const wordStats*> wordNodes = collectWords();
  vector<string> words;
  vector<string> lemmas;
  vector<string> conts;
  vector<string> names;
  vector<string> temp_conn;
  vector<string> reeks_conn;
  vector<string> contr_conn;
  vector<string> comp_conn;
  vector<string> cause_conn;
  vector<string> tijd_sits;
  vector<string> ruimte_sits;
  vector<string> cause_sits;
  vector<string> emotion_sits;
  for ( size_t i=0; i < wordNodes.size(); ++i ){
    if ( wordNodes[i]->wordProperty() == ISLET ){
      continue;
    }
    string word = wordNodes[i]->ltext();
    words.push_back( word );
    string lemma = wordNodes[i]->llemma();
    lemmas.push_back( lemma );
    if ( wordNodes[i]->isContent ){
      conts.push_back( wordNodes[i]->ltext() );
    }
    if ( wordNodes[i]->prop == ISNAME ){
      names.push_back( wordNodes[i]->ltext() );
    }
    switch( wordNodes[i]->getConnType() ){
    case TEMPOREEL:
      temp_conn.push_back( wordNodes[i]->ltext() );
      break;
    case OPSOMMEND:
      reeks_conn.push_back( wordNodes[i]->ltext() );
      break;
    case CONTRASTIEF:
      contr_conn.push_back( wordNodes[i]->ltext() );
      break;
    case COMPARATIEF:
      comp_conn.push_back( wordNodes[i]->ltext() );
      break;
    case CAUSAAL:
      cause_conn.push_back( wordNodes[i]->ltext() );
      break;
    default:
      break;
    }
    switch( wordNodes[i]->getSitType() ){
    case TIME_SIT:
      tijd_sits.push_back(wordNodes[i]->Lemma());
      break;
    case CAUSAL_SIT:
      cause_sits.push_back(wordNodes[i]->Lemma());
      break;
    case SPACE_SIT:
      ruimte_sits.push_back(wordNodes[i]->Lemma());
      break;
    case EMO_SIT:
      emotion_sits.push_back(wordNodes[i]->Lemma());
      break;
    default:
      break;
    }
  }

  word_mtld = average_mtld( words );
  lemma_mtld = average_mtld( lemmas );
  content_mtld = average_mtld( conts );
  name_mtld = average_mtld( names );
  temp_conn_mtld = average_mtld( temp_conn );
  reeks_conn_mtld = average_mtld( reeks_conn );
  contr_conn_mtld = average_mtld( contr_conn );
  comp_conn_mtld = average_mtld( comp_conn );
  cause_conn_mtld = average_mtld( cause_conn );
  tijd_sit_mtld = average_mtld( tijd_sits );
  ruimte_sit_mtld = average_mtld( ruimte_sits );
  cause_sit_mtld = average_mtld( cause_sits );
  emotion_sit_mtld = average_mtld( emotion_sits );
}

void sentStats::setLSAvalues( double suc, double net, double ctx ){
  if ( suc > 0 )
    lsa_word_suc = suc;
  if ( net > 0 )
    lsa_word_net = net;
  if ( ctx > 0 )
    throw logic_error("context cannot be !=0 for sentstats");
}

double sentStats::getMeanAL() const {
  double result = NA;
  size_t len = distances.size();
  if ( len > 0 ){
    result = 0;
    for( multimap<DD_type, int>::const_iterator pos = distances.begin();
	 pos != distances.end();
	 ++pos ){
      result += pos->second;
    }
    result = result/len;
  }
  return result;
}

double sentStats::getHighestAL() const {
  double result = NA;
  for( multimap<DD_type, int>::const_iterator pos = distances.begin();
       pos != distances.end();
       ++pos ){
    if ( pos->second > result )
      result = pos->second;
  }
  return result;
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

SituationType sentStats::checkMultiSituations( const string& mword ){
  //  cerr << "check multi-sit '" << mword << "'" << endl;
  SituationType sit = NO_SIT;
  if ( settings.multi_time_sits.find( mword ) != settings.multi_time_sits.end() ){
    sit = TIME_SIT;
  }
  else if ( settings.multi_space_sits.find( mword ) != settings.multi_space_sits.end() ){
    sit = SPACE_SIT;
  }
  else if ( settings.multi_causal_sits.find( mword ) != settings.multi_causal_sits.end() ){
    sit = CAUSAL_SIT;
  }
  else if ( settings.multi_emotion_sits.find( mword ) != settings.multi_emotion_sits.end() ){
    sit = EMO_SIT;
  }
  //  cerr << "multi-sit " << mword << " = " << sit << endl;
  return sit;
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
    contrastConnCnt++;
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
      unique_temp_conn[sv[i]->ltext()]++;
      tempConnCnt++;
      break;
    case OPSOMMEND:
      unique_reeks_conn[sv[i]->ltext()]++;
      opsomConnCnt++;
      break;
    case CONTRASTIEF:
      unique_contr_conn[sv[i]->ltext()]++;
      contrastConnCnt++;
      break;
    case COMPARATIEF:
      unique_comp_conn[sv[i]->ltext()]++;
      compConnCnt++;
      break;
    case CAUSAAL:
      unique_cause_conn[sv[i]->ltext()]++;
      causeConnCnt++;
      break;
    default:
      break;
    }
  }
}

void sentStats::resolveSituations(){
  if ( sv.size() > 1 ){
    for ( size_t i=0; (i+3) < sv.size(); ++i ){
      string word = sv[i]->Lemma();
      string multiword2 = word + " " + sv[i+1]->Lemma();
      string multiword3 = multiword2 + " " + sv[i+2]->Lemma();
      string multiword4 = multiword3 + " " + sv[i+3]->Lemma();
      //      cerr << "zoek 4 op '" << multiword4 << "'" << endl;
      SituationType sit = checkMultiSituations( multiword4 );
      if ( sit != NO_SIT ){
	//	cerr << "found " << sit << "-situation: " << multiword4 << endl;
	sv[i]->setSitType( NO_SIT );
	sv[i+1]->setSitType( NO_SIT );
	sv[i+2]->setSitType( NO_SIT );
	sv[i+3]->setSitType( sit );
	i += 3;
      }
      else {
	//cerr << "zoek 3 op '" << multiword3 << "'" << endl;
	sit = checkMultiSituations( multiword3 );
	if ( sit != NO_SIT ){
	  // cerr << "found " << sit << "-situation: " << multiword3 << endl;
	  sv[i]->setSitType( NO_SIT );
	  sv[i+1]->setSitType( NO_SIT );
	  sv[i+2]->setSitType( sit );
	  i += 2;
	}
	else {
	  //cerr << "zoek 2 op '" << multiword2 << "'" << endl;
	  sit = checkMultiSituations( multiword2 );
	  if ( sit != NO_SIT ){
	    //	    cerr << "found " << sit << "-situation: " << multiword2 << endl;
	    sv[i]->setSitType( NO_SIT);
	    sv[i+1]->setSitType( sit );
	    i += 1;
	  }
	}
      }
    }
    // don't forget the last 2 and 3 words
    SituationType sit = NO_SIT;
    if ( sv.size() > 2 ){
      string multiword3 = sv[sv.size()-3]->Lemma() + " "
	+ sv[sv.size()-2]->Lemma() + " " + sv[sv.size()-1]->Lemma();
      //cerr << "zoek final 3 op '" << multiword3 << "'" << endl;
      sit = checkMultiSituations( multiword3 );
      if ( sit != NO_SIT ){
	//	cerr << "found " << sit << "-situation: " << multiword3 << endl;
	sv[sv.size()-3]->setSitType( NO_SIT );
	sv[sv.size()-2]->setSitType( NO_SIT );
	sv[sv.size()-1]->setSitType( sit );
      }
      else {
	string multiword2 = sv[sv.size()-3]->Lemma() + " "
	  + sv[sv.size()-2]->Lemma();
	//cerr << "zoek first final 2 op '" << multiword2 << "'" << endl;
	sit = checkMultiSituations( multiword2 );
	if ( sit != NO_SIT ){
	  //	  cerr << "found " << sit << "-situation: " << multiword2 << endl;
	  sv[sv.size()-3]->setSitType( NO_SIT);
	  sv[sv.size()-2]->setSitType( sit );
	}
	else {
	  string multiword2 = sv[sv.size()-2]->Lemma() + " "
	    + sv[sv.size()-1]->Lemma();
	  //cerr << "zoek second final 2 op '" << multiword2 << "'" << endl;
	  sit = checkMultiSituations( multiword2 );
	  if ( sit != NO_SIT ){
	    //	    cerr << "found " << sit << "-situation: " << multiword2 << endl;
	    sv[sv.size()-2]->setSitType( NO_SIT);
	    sv[sv.size()-1]->setSitType( sit );
	  }
	}
      }
    }
    else {
      string multiword2 = sv[sv.size()-2]->Lemma() + " "
	+ sv[sv.size()-1]->Lemma();
      // cerr << "zoek second final 2 op '" << multiword2 << "'" << endl;
      sit = checkMultiSituations( multiword2 );
      if ( sit != NO_SIT ){
	//	cerr << "found " << sit << "-situation: " << multiword2 << endl;
	sv[sv.size()-2]->setSitType( NO_SIT);
	sv[sv.size()-1]->setSitType( sit );
      }
    }
  }
  for ( size_t i=0; i < sv.size(); ++i ){
    switch( sv[i]->getSitType() ){
    case TIME_SIT:
      unique_tijd_sits[sv[i]->Lemma()]++;
      timeSitCnt++;
      break;
    case CAUSAL_SIT:
      unique_cause_sits[sv[i]->Lemma()]++;
      causeSitCnt++;
      break;
    case SPACE_SIT:
      unique_ruimte_sits[sv[i]->Lemma()]++;
      spaceSitCnt++;
      break;
    case EMO_SIT:
      unique_emotion_sits[sv[i]->Lemma()]++;
      emoSitCnt++;
      break;
    default:
      break;
    }
  }
}

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
      LOG << "LSA-sent lookup '" << call << "' ==> " << result << endl;
#endif
    }
  }
#ifdef DEBUG_LSA
  LOG << "size = " << sv.size() << ", LETS = " << lets << endl;
  LOG << "LSA-sent-SUC sum = " << suc << endl;
  LOG << "LSA-sent=NET sum = " << net << ", node count = " << node_count << endl;
#endif
  suc = suc/(sv.size()-lets);
  net = net/node_count;
#ifdef DEBUG_LSA
  LOG << "LSA-sent-SUC result = " << suc << endl;
  LOG << "LSA-sent-NET result = " << net << endl;
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
  if ( !result.empty() && result.size() > 10 ){
    DBG << "start FoLiA parsing" << endl;
    Document *doc = new Document();
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
  else {
    LOG << "No usable FoLia date retrieved from Wopr. Got '"
	<< result << "'" << endl;
  }
  LOG << "finished Wopr" << endl;
}

xmlDoc *AlpinoServerParse( Sentence *);

sentStats::sentStats( int index, Sentence *s, const sentStats* pred,
		      const map<string,double>& LSAword_dists ):
  structStats( index, s, "sent" ){
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

  // if ( parseFailCnt == 1 ){
  //   // glorious fail
  //   return;
  // }
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
      alpWord = getAlpNodeWord( alpDoc, w[i] );
    }
    wordStats *ws = new wordStats( i, w[i], alpWord, puncts, parseFailCnt==1 );
    if ( parseFailCnt ){
      sv.push_back( ws );
      continue;
    }
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
	nerCnt++;
	break;
      default:
	;
      }
      ws->setPersRef(); // need NER Info for this
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
	break;
      case ISPPRON2:
	pron2Cnt++;
	break;
      case ISPPRON3:
	pron3Cnt++;
	break;
      default:
	;// ignore JUSTAWORD and ISAANW
      }
      if ( ws->wwform == PASSIVE_VERB )
	passiveCnt++;
      if ( ws->wwform == MODAL_VERB )
	modalCnt++;
      if ( ws->wwform == TIME_VERB )
	timeVCnt++;
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
      case UNDEFINED_NOUN:
	++undefinedNounCnt;
	break;
      case UNDEFINED_ADJ:
	++undefinedAdjCnt;
	break;
      case UNFOUND_NOUN:
	++uncoveredNounCnt;
	break;
      case UNFOUND_ADJ:
	++uncoveredAdjCnt;
	break;
      case UNFOUND_VERB:
	++uncoveredVerbCnt;
	break;
      case CONCRETE_HUMAN_NOUN:
	humanCnt++;
	strictNounCnt++;
	broadNounCnt++;
	break;
      case CONCRETE_NONHUMAN_NOUN:
	nonHumanCnt++;
	strictNounCnt++;
	broadNounCnt++;
	break;
      case CONCRETE_ARTEFACT_NOUN:
	artefactCnt++;
	strictNounCnt++;
	broadNounCnt++;
	break;
      case CONCRETE_SUBSTANCE_NOUN:
	substanceCnt++;
	strictNounCnt++;
	broadNounCnt++;
	break;
      case CONCRETE_OTHER_NOUN:
	strictNounCnt++;
	broadNounCnt++;
	break;
      case BROAD_CONCRETE_PLACE_NOUN:
	++placeCnt;
	broadNounCnt++;
	break;
      case BROAD_CONCRETE_TIME_NOUN:
	++timeCnt;
	broadNounCnt++;
	break;
      case BROAD_CONCRETE_MEASURE_NOUN:
	++measureCnt;
	broadNounCnt++;
	break;
      case ABSTRACT_DYNAMIC_NOUN:
	++dynamicCnt;
	strictNounCnt++;
	broadNounCnt++;
	break;
      case ABSTRACT_NONDYNAMIC_NOUN:
	++nonDynamicCnt;
	strictNounCnt++;
	broadNounCnt++;
	break;
      case INSTITUT_NOUN:
	institutCnt++;
	break;
      case HUMAN_ADJ:
	humanAdjCnt++;
	broadAdjCnt++;
	strictAdjCnt++;
	break;
      case EMO_ADJ:
	emoAdjCnt++;
	broadAdjCnt++;
	strictAdjCnt++;
	break;
      case NONHUMAN_SHAPE_ADJ:
	nonhumanAdjCnt++;
	shapeAdjCnt++;
	broadAdjCnt++;
	strictAdjCnt++;
	break;
      case NONHUMAN_COLOR_ADJ:
	nonhumanAdjCnt++;
	colorAdjCnt++;
	broadAdjCnt++;
	strictAdjCnt++;
	break;
      case NONHUMAN_MATTER_ADJ:
	nonhumanAdjCnt++;
	matterAdjCnt++;
	broadAdjCnt++;
	strictAdjCnt++;
	break;
      case NONHUMAN_SOUND_ADJ:
	nonhumanAdjCnt++;
	soundAdjCnt++;
	broadAdjCnt++;
	strictAdjCnt++;
	break;
      case NONHUMAN_OTHER_ADJ:
	nonhumanAdjCnt++;
	nonhumanOtherAdjCnt++;
	broadAdjCnt++;
	strictAdjCnt++;
	break;
      case TECH_ADJ:
	techAdjCnt++;
	break;
      case TIME_ADJ:
	timeAdjCnt++;
	broadAdjCnt++;
	break;
      case PLACE_ADJ:
	placeAdjCnt++;
	broadAdjCnt++;
	break;
      case SPEC_POS_ADJ:
	specPosAdjCnt++;
	subjectiveAdjCnt++;
	break;
      case SPEC_NEG_ADJ:
	specNegAdjCnt++;
	subjectiveAdjCnt++;
	break;
      case POS_ADJ:
	posAdjCnt++;
	subjectiveAdjCnt++;
	break;
      case NEG_ADJ:
	negAdjCnt++;
	subjectiveAdjCnt++;
	break;
      case EPI_POS_ADJ:
	epiPosAdjCnt++;
	subjectiveAdjCnt++;
	break;
      case EPI_NEG_ADJ:
	epiNegAdjCnt++;
	subjectiveAdjCnt++;
	break;
      case MORE_ADJ:
	moreAdjCnt++;
	subjectiveAdjCnt++;
	break;
      case LESS_ADJ:
	lessAdjCnt++;
	subjectiveAdjCnt++;
	break;
      case ABSTRACT_ADJ:
	abstractAdjCnt++;
	break;
      case ABSTRACT_STATE:
	abstractWwCnt++;
	stateCnt++;
	break;
      case CONCRETE_STATE:
	concreteWwCnt++;
	stateCnt++;
	break;
      case UNDEFINED_STATE:
	undefinedWwCnt++;
	stateCnt++;
	break;
      case ABSTRACT_ACTION:
	abstractWwCnt++;
	actionCnt++;
	break;
      case CONCRETE_ACTION:
	concreteWwCnt++;
	actionCnt++;
	break;
      case UNDEFINED_ACTION:
	undefinedWwCnt++;
	actionCnt++;
	break;
      case ABSTRACT_PROCESS:
	abstractWwCnt++;
	processCnt++;
	break;
      case CONCRETE_PROCESS:
	concreteWwCnt++;
	processCnt++;
	break;
      case UNDEFINED_PROCESS:
	undefinedWwCnt++;
	processCnt++;
	break;
      case ABSTRACT_VERB:
	abstractWwCnt++;
	break;
      case CONCRETE_VERB:
	concreteWwCnt++;
	break;
      case UNDEFINED_VERB:
	undefinedWwCnt++;
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
  al_gem = getMeanAL();
  al_max = getHighestAL();
  resolveConnectives();
  resolveSituations();
  calculate_MTLDs();
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
  parStats( int, Paragraph *, const map<string,double>&,
	    const map<string,double>& );
  void addMetrics( ) const;
  void setLSAvalues( double, double, double );
};

parStats::parStats( int index,
		    Paragraph *p,
		    const map<string,double>& LSA_word_dists,
		    const map<string,double>& LSA_sent_dists ):
  structStats( index, p, "par" )
{
  sentCnt = 0;
  vector<Sentence*> sents = p->sentences();
  sentStats *prev = 0;
  for ( size_t i=0; i < sents.size(); ++i ){
    sentStats *ss = new sentStats( i, sents[i], prev, LSA_word_dists );
    prev = ss;
    merge( ss );
  }
  if ( settings.doLsa ){
    resolveLSA( LSA_sent_dists );
  }
  calculate_MTLDs();
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
#ifdef DEBUG_LSA
      LOG << "LSA combine paragraaf " << index << endl;
#endif
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
#ifdef DEBUG_LSA
      LOG << "LSA combine sentence " << index << endl;
#endif
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
  structStats( 0, 0, "document" ),
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
    parStats *ps = new parStats( i, pars[i], LSA_word_dists, LSA_sentence_dists );
      merge( ps );
  }
  if ( settings.doLsa ){
    resolveLSA( LSA_paragraph_dists );
  }
  calculate_MTLDs();
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
		"word_mtld", toString( word_mtld ) );
  addOneMetric( el->doc(), el,
		"lemma_ttr", toString( unique_lemmas.size()/double(wordCnt) ) );
  addOneMetric( el->doc(), el,
		"lemma_mtld", toString( lemma_mtld ) );
  if ( nameCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "names_ttr", toString( unique_names.size()/double(nameCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"name_mtld", toString( name_mtld ) );

  if ( contentCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "content_word_ttr", toString( unique_contents.size()/double(contentCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"content_mtld", toString( content_mtld ) );

  if ( timeSitCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "time_sit_ttr", toString( unique_tijd_sits.size()/double(timeSitCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"tijd_sit_mtld", toString( tijd_sit_mtld ) );

  if ( spaceSitCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "space_sit_ttr", toString( unique_ruimte_sits.size()/double(spaceSitCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"ruimte_sit_mtld", toString( ruimte_sit_mtld ) );

  if ( causeSitCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "cause_sit_ttr", toString( unique_cause_sits.size()/double(causeSitCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"cause_sit_mtld", toString( cause_sit_mtld ) );

  if ( emoSitCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "emotion_sit_ttr", toString( unique_emotion_sits.size()/double(emoSitCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"emotion_sit_mtld", toString( emotion_sit_mtld ) );

  if ( tempConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "temp_conn_ttr", toString( unique_temp_conn.size()/double(tempConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"temp_conn_mtld", toString(temp_conn_mtld) );

  if ( opsomConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "opsom_conn_ttr", toString( unique_reeks_conn.size()/double(opsomConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"opsom_conn_mtld", toString(reeks_conn_mtld) );

  if ( contrastConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "contrast_conn_ttr", toString( unique_contr_conn.size()/double(contrastConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"contrast_conn_mtld", toString(contr_conn_mtld) );

  if ( compConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "comp_conn_ttr", toString( unique_comp_conn.size()/double(compConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"comp_conn_mtld", toString(comp_conn_mtld) );


  if ( causeConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "cause_conn_ttr", toString( unique_cause_conn.size()/double(causeConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"cause_conn_mtld", toString(cause_conn_mtld) );


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
      CSVheader( out, "Inputfile" );
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
	  sv[0]->CSVheader( out, "Inputfile,Segment" );
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
	    sv[0]->sv[0]->CSVheader( out, "Inputfile,Segment" );
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
	      sv[0]->sv[0]->sv[0]->CSVheader( out, "Inputfile,Segment,word,lemma,full_lemma,morfemen" );
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
  bool incomment = false;
  string line;
  while ( getline( is, line ) ){
    DBG << "read: " << line << endl;
    if ( line.length() > 2 ){
      string start = line.substr(0,3);
      if ( start == "###" )
	continue;
      else if ( start == "<<<" ){
	if ( incomment ){
	  LOG << "Nested comment (<<<) not allowed!" << endl;
	  return 0;
	}
	else {
	  incomment = true;
	}
      }
      else if ( start == ">>>" ){
	if ( !incomment ){
	  LOG << "end of comment (>>>) found without start." << endl;
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
    if ( inputnames.size() > 1 ){
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

