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
#include "tscan/Alpino.h"
#include "tscan/decomp.h"
#include "tscan/surprise.h"

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
 
struct settingData {
  void init( const Configuration& );
  bool doAlpino;
  bool doAlpinoServer;
  bool doDecompound;
  bool doSurprisal;
  bool doWopr;
  bool doXfiles;
  string decompounderPath;
  string surprisalPath;
  string style;
  int rarityLevel;
  unsigned int overlapSize;
  double polarity_threshold;
  map <string, string> adj_sem;
  map <string, string> noun_sem;
  map <string, string> verb_sem;
  map <string, double> pol_lex;
  map<string, cf_data> staph_word_freq_lex;
  map<string, cf_data> word_freq_lex;
  map<string, cf_data> lemma_freq_lex;
  map<string, top_val> top_freq_lex;
};

settingData settings;

bool fill( map<string,string>& m, istream& is ){
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
      m[parts[0]] = vals[0];
    }
    else if ( n == 0 ){
      cerr << "skip line: " << line << " (expected some values, got none."
	   << endl;
      continue;
    }
    else {
      map<string,int> stats;
      int max = 0;
      string topval;
      for ( size_t i=0; i< vals.size(); ++i ){
	if ( ++stats[vals[i]] > max ){
	  max = stats[vals[i]];
	  topval = vals[i];
	}
      }
      m[parts[0]] = topval;
    }
  }
  return true;
}

bool fill( map<string,string>& m, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill( map<string,double>& m, istream& is ){
  string line;
  while( getline( is, line ) ){
    vector<string> parts;
    size_t n = split_at( line, parts, "\t" ); // split at tab
    if ( n != 2 ){
      cerr << "skip line: " << line << " (expected 2 values, got " 
	   << n << ")" << endl;
      continue;
    }
    double value = TiCC::stringTo<double>( parts[1] );
    if ( abs(value) < settings.polarity_threshold ){
      value = 0;
    }
    // parts[0] is something like "sympathiek a"
    // is't not quite clear if there is exactly 1 space
    // also there is "dolce far niente n"
    // so we split again, and reconstruct using a colon before the pos.
    vector<string> vals;
    n = split_at( parts[0], vals, " " ); // split at space(s)
    if ( n < 2 ){
      cerr << "skip line: " << line << " (expected at least 2 values, got " 
	   << n << ")" << endl;
      continue;
    }
    string key;
    for ( size_t i=0; i < n-1; ++i )
      key += vals[i];
    key += ":" + vals[n-1];
    m[key] = value;
  }
  return true;
}

bool fill( map<string,double>& m, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill( map<string,cf_data>& m, istream& is ){
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
    data.freq = TiCC::stringTo<double>( parts[3] );
    m[parts[0]] = data;
  }
  return true;
}

bool fill( map<string,cf_data>& m, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

bool fill( map<string,top_val>& m, istream& is ){
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
    if ( n != 2 ){
      cerr << "skip line: " << line << " (expected 2 values, got " 
	   << n << ")" << endl;
      continue;
    }
    m[parts[0]] = val;
  }
  return true;
}

bool fill( map<string,top_val>& m, const string& filename ){
  ifstream is( filename.c_str() );
  if ( is ){
    return fill( m, is );
  }
  else {
    cerr << "couldn't open file: " << filename << endl;
  }
  return false;
}

void settingData::init( const Configuration& cf ){
  doAlpino = false;
  doAlpinoServer = false;
  doXfiles = true;
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
  doDecompound = false;
  val = cf.lookUp( "decompounderPath" );
  if( !val.empty() ){
    decompounderPath = val + "/";
    doDecompound = true;
  }
  doSurprisal = false;
  val = cf.lookUp( "surprisalPath" );
  if( !val.empty() ){
    surprisalPath = val + "/";
    doSurprisal = true;
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
  val = cf.lookUp( "adj_semtypes" );
  if ( !val.empty() ){
    if ( !fill( adj_sem, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "noun_semtypes" );
  if ( !val.empty() ){
    if ( !fill( noun_sem, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "verb_semtypes" );
  if ( !val.empty() ){
    if ( !fill( verb_sem, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "polarity_threshold" );
  if ( val.empty() ){
    polarity_threshold = 0.01;
  }
  else if ( !TiCC::stringTo( val, polarity_threshold ) ){ 
    cerr << "invalid value for 'polarity_threshold' in config file" << endl;
  }
  val = cf.lookUp( "polarity_lex" );
  if ( !val.empty() ){
    if ( !fill( pol_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "staph_word_freq_lex" );
  if ( !val.empty() ){
    if ( !fill( staph_word_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "word_freq_lex" );
  if ( !val.empty() ){
    if ( !fill( word_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "lemma_freq_lex" );
  if ( !val.empty() ){
    if ( !fill( lemma_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "top_freq_lex" );
  if ( !val.empty() ){
    if ( !fill( top_freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
}

inline void usage(){
  cerr << "usage:  tscan [options] -t <inputfile> " << endl;
  cerr << "options: " << endl;
  cerr << "\t-o <file> store XML in file " << endl;
  cerr << "\t--config=<file> read configuration from file " << endl;
  cerr << "\t-V or --version show version " << endl;
  cerr << "\t-D <value> set debug level " << endl;
  cerr << "\t--skip=[wadsc]    Skip Wopr (w), Alpino (a), Decompounder (d), Suprisal Parser (s), or CSV output (s) \n";
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
};

enum WordProp { ISNAME, ISPUNCT, 
		ISVD, ISOD, ISINF, ISPVTGW, ISPVVERL, ISSUBJ,
		ISPPRON1, ISPPRON2, ISPPRON3, ISAANW,
		JUSTAWORD };

string toString( const WordProp& w ){
  switch ( w ){
  case ISNAME:
    return "naam";
  case ISPUNCT:
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
  default:
    return "default";
  }
}

enum ConnType { NOCONN, TEMPOREEL, REEKS, CONTRASTIEF, COMPARATIEF, CAUSAAL }; 

string toString( const ConnType& c ){
  if ( c == NOCONN )
    return "Not_a_connector";
  else if ( c == TEMPOREEL )
    return "temporeel";
  else if ( c == REEKS )
    return "reeks";
  else if ( c == CONTRASTIEF )
    return "contrastief";
  else if ( c == COMPARATIEF )
    return "comparatief";
  else if ( c == CAUSAAL )
    return "causaal";
  else 
    throw "no translation for ConnType";
}

enum SemType { UNFOUND, CONCRETE_NOUN, CONCRETE_ADJ, CONCRETE_HUMAN, 
	       ABSTRACT_NOUN, ABSTRACT_ADJ, BROAD_NOUN, BROAD_ADJ, EMO_ADJ,
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
  case CONCRETE_HUMAN:
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

struct basicStats {
  basicStats( FoliaElement *el, const string& cat ): 
    folia_node( el ),
    category( cat ), 
    charCnt(0),charCntExNames(0),
    morphCnt(0), morphCntExNames(0)
  {};
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
  virtual ConnType getConnType() const { return NOCONN; };
  FoliaElement *folia_node;
  string category;
  int charCnt;
  int charCntExNames;
  int morphCnt;
  int morphCntExNames;
  vector<basicStats *> sv;
};

struct wordStats : public basicStats {
  wordStats( Word *, xmlDoc *, const set<size_t>& );
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
  ConnType getConnType() const { return connType; };
  void addMetrics( ) const;
  bool checkContent() const;
  ConnType checkConnective( ) const;
  bool checkNominal( Word *, xmlDoc * ) const;
  WordProp checkProps( const PosAnnotation* );
  double checkPolarity( ) const;
  SemType checkSemProps( ) const;
  bool checkPropNeg() const;
  bool checkMorphNeg() const;
  void staphFreqLookup();
  void topFreqLookup();
  void freqLookup();
  void getSentenceOverlap( const Sentence * );
  string word;
  string pos;
  CGN::Type tag;
  string lemma;
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
  ConnType connType;
  bool f50;
  bool f65;
  bool f77;
  bool f80;
  int compPartCnt;
  top_val top_freq;
  int word_freq;
  int lemma_freq;
  int argRepeatCnt;
  int wordOverlapCnt;
  int lemmaRepeatCnt;
  int lemmaOverlapCnt;
  double word_freq_log;
  double lemma_freq_log;
  double polarity;
  double surprisal;
  double logprob10;
  WordProp prop;
  SemType sem_type;
  vector<string> morphemes;
  multimap<DD_type,int> distances;
};

ConnType wordStats::checkConnective() const {
  static string temporalList[] = 
    { "aanstonds", "achtereenvolgens", "alvast", "alvorens", "anno", 
      "bijtijds", "binnenkort", "daarnet", "daarstraks", "daartussendoor",
      "dadelijk", "destijds", "eensklaps", "eer", "eerdaags", 
      "eergisteren", "eerlang", "eerst", "eertijds", "eindelijk", 
      "ertussendoor", "gisteren", "heden", "hedenavond", "hedenmiddag", 
      "hedenmorgen", "hedennacht", "hedenochtend", "hoelang", "indertijd", 
      "ineens", "ingaande", "inmiddels", "medio", "meer", "meestal", 
      "meteen", "morgen", "morgenavond", "morgenmiddag", "morgennacht", 
      "morgenochtend", "morgenvroeg", "na", "nadat", "naderhand", 
      "nadezen", "nadien", "net", "nooit", "olim", "omstreeks", 
      "onderwijl", "onlangs", "ooit", "opeens", "overdag", "overmorgen", 
      "pardoes", "pas", "plotsklaps", "recentelijk", "reeds", "sedert", 
      "sedert", "sedertdien", "sinds", "sinds", "sindsdien", "steeds", 
      "strakjes", "straks", "subiet", "tegelijk", "tegelijkertijd", 
      "terstond", "tevoren", "tezelfdertijd", "thans", "toen", "toenmaals", 
      "toentertijd", "totdat", "uiterlijk", "vanavond", "vandaag", 
      "vanmiddag", "vanmorgen", "vannacht", "vanochtend", "vervolgens",
      "vooraf", "vooraleer", "vooralsnog", "voordat", "voorheen", "weldra", 
      "weleer", "zodra", "zo-even", "zojuist", "zonet", "zopas" };
  static set<string> temporals( temporalList, 
				temporalList + sizeof(temporalList)/sizeof(string) );

  static string reeksList[] = 
    {"alsmede", "alsook", "annex", "bovendien", "buitendien", "daarenboven",
     "daarnaast", "en", "evenals", "eveneens", "evenmin", "hetzij", 
     "hierenboven", "noch", "of", "ofwel", "ook", "respectievelijk", 
     "tevens", "vooral", "waarnaast"};
  static set<string> reeks( reeksList, 
			    reeksList + sizeof(reeksList)/sizeof(string) );

  static string contrastList[] = 
    { "al", "alhoewel", "althans", "anderzijds", "behalve", "behoudens",
      "daarentegen", "desondanks", "doch", "echter", "evengoed", "evenwel", 
      "hoewel", "hoezeer", "integendeel", "maar", "niettegenstaande", 
      "niettemin", "nochtans", "ofschoon", "ondanks", "ondertussen", 
      "ongeacht", "tenzij", "uitgezonderd", "weliswaar", "enerzijds"  };
  static set<string> contrastief( contrastList, 
				  contrastList + sizeof(contrastList)/sizeof(string) );
  
  static string comparList[] = {
    "als", "alsof", "dan", "meer", "meest", "minder", "minst", "naargelang", "naarmate", "zoals" };
  static set<string> comparatief( comparList, 
				  comparList + sizeof(comparList)/sizeof(string) );

  static string causesList[] = 
    { "aangezien", "als", "bijgevolg", "daar", "daardoor", "daarmee", 
      "daarom", "daartoe", "daarvoor", "dankzij", "derhalve", "dientengevolge",
      "doordat", "dus", "dus", "ergo", "ermee", "erom", "ertoe", "getuige", 
      "gezien", "hierdoor", "hiermee", "hierom", "hiertoe", "hiervoor", 
      "immers", "indien", "ingeval", "ingevolge", "krachtens", "middels", 
      "mits", "namelijk", "nu", "om", "om", "omdat", "opdat", "teneinde", 
      "vanwege", "vermits", "waardoor", "waarmee", "waarom", "waartoe",
      "waartoe", "wanneer", "want", "wegens", "zodat", "zodoende", "zolang" };
  static set<string> causuals( causesList, 
			       causesList + sizeof(causesList)/sizeof(string) );
  if ( tag == CGN::VG || tag == CGN::BW ){
    string lword = lowercase( word );
    if ( temporals.find( lword ) != temporals.end() )
      return TEMPOREEL;
    else if ( reeks.find( lword ) != reeks.end() )
      return REEKS;
    else if ( contrastief.find( lword ) != contrastief.end() )
      return CONTRASTIEF;
    else if ( comparatief.find( lword ) != comparatief.end() )
      return COMPARATIEF;
    else if ( causuals.find( lword ) != causuals.end() )
      return CAUSAAL;
  }
  return NOCONN;
}

ConnType check2Connectives( const string& mword ){
  static string temporal2List[] = {"de dato", "na dato"};
  static set<string> temporals_2( temporal2List, 
				  temporal2List + sizeof(temporal2List)/sizeof(string) );

  static string reeks2List[] = 
    { "bovenal ", "daarbij komt", "dan wel", "evenmin als", "ten derde", 
      "ten eerste", "ten tweede", "ten vierde", 
      "verder ", "voornamelijk ", "voorts ", "zomin als", "zowel als" }; 
  static set<string> reeks_2( reeks2List, 
			      reeks2List + sizeof(reeks2List)/sizeof(string) );

  static string contrast2List[] = { "ook al", "zij het" };
  static set<string> contrastief_2( contrast2List, 
				    contrast2List + sizeof(contrast2List)/sizeof(string) );

  static string compar2List[] = { "net als" };
  static set<string> comparatief_2( compar2List, 
				    compar2List + sizeof(compar2List)/sizeof(string) );

  static string causes2List[] = 
    { "dan ook", "tengevolge van", "vandaar dat", "zo ja", 
      "zo nee", "zo niet" };
  static set<string> causuals_2( causes2List, 
				 causes2List + sizeof(causes2List)/sizeof(string) );
  if ( temporals_2.find( mword ) != temporals_2.end() )
    return TEMPOREEL;
  else if ( reeks_2.find( mword ) != reeks_2.end() )
    return REEKS;
  else if ( contrastief_2.find( mword ) != contrastief_2.end() )
    return CONTRASTIEF;
  else if ( comparatief_2.find( mword ) != comparatief_2.end() )
    return COMPARATIEF;
  else if ( causuals_2.find( mword ) != causuals_2.end() )
    return CAUSAAL;
  return NOCONN;
}

ConnType check3Connectives( const string& mword ){
  static string temporal3List[] = {"a la minute", "hic et nunc"};
  static set<string> temporals_3( temporal3List, 
				  temporal3List + sizeof(temporal3List)/sizeof(string) );

  static string reeks3List[] = { "om te beginnen" };
  static set<string> reeks_3( reeks3List, 
			      reeks3List + sizeof(reeks3List)/sizeof(string) );

  static string contrast3List[] = 
    { "in plaats daarvan", "in tegenstelling tot", "zij het dat" };
  static set<string> contrastief_3( contrast3List, 
				    contrast3List + sizeof(contrast3List)/sizeof(string) );
  
  static string compar3List[] = { "net zo als" };
  static set<string> comparatief_3( compar3List, 
				    compar3List + sizeof(compar3List)/sizeof(string) );

  static string causes3List[] = { "met behulp van" };
  static set<string> causuals_3( causes3List, 
				 causes3List + sizeof(causes3List)/sizeof(string) );
  if ( temporals_3.find( mword ) != temporals_3.end() )
    return TEMPOREEL;
  else if ( reeks_3.find( mword ) != reeks_3.end() )
    return REEKS;
  else if ( contrastief_3.find( mword ) != contrastief_3.end() )
    return CONTRASTIEF;
  else if ( comparatief_3.find( mword ) != comparatief_3.end() )
    return COMPARATIEF;
  else if ( causuals_3.find( mword ) != causuals_3.end() )
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
    return ( tag == CGN::N || tag == CGN::BW || tag == CGN::ADJ );
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

bool wordStats::checkNominal( Word *w, xmlDoc *alpDoc ) const {
  static string morphList[] = { "ing", "sel", "nis", "enis", "heid", "te", 
				"schap", "dom", "sie", "iek", "iteit", "age",
				"esse",	"name" };
  static set<string> morphs( morphList, 
			     morphList + sizeof(morphList)/sizeof(string) );
  //  cerr << "check Nominal. morphemes=" << morphemes << endl;
  if ( tag == CGN::N && morphemes.size() > 1 ){
    string last_morph = morphemes[morphemes.size()-1];
    if ( ( last_morph == "en" || last_morph == "s" ) 
	 && morphemes.size() > 2 )
      last_morph =  morphemes[morphemes.size()-2];
    if ( morphs.find( last_morph ) != morphs.end() ){
      // morphemes.size() > 1 check mijdt false hits voor "dom", "schap".
      //      cerr << "check Nominal, MATCHED morheme " << last_morph << endl;
      return true;
    }
  }
  bool matched = match_tail( word, "ose" ) ||
    match_tail( word, "ase" ) ||
    match_tail( word, "ese" ) ||
    match_tail( word, "isme" ) ||
    match_tail( word, "sie" ) ||
    match_tail( word, "tie" );
  if ( matched ){
    //    cerr << "check Nominal, MATCHED tail " <<  word << endl;    
    return true;
  }
  else {
    xmlNode *node = getAlpWord( alpDoc, w );
    if ( node ){
      KWargs args = getAttributes( node );
      if ( args["pos"] == "verb" ){
	// Alpino heeft de voor dit feature prettige eigenschap dat het nogal
	// eens nominalisaties wil taggen als werkwoord dat onder een 
	// NP knoop hangt 
	node = node->parent;
	KWargs args = getAttributes( node );
	if ( args["cat"] == "np" )
	  return true;
      }
    }
  }
  return false;
}

WordProp wordStats::checkProps( const PosAnnotation* pa ) {
  if ( tag == CGN::LET )
    prop = ISPUNCT;
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
    if ( lowercase( word ) != "men" ){
      string cas = pa->feat("case");
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
    string cas = pa->feat("case");
    archaic = ( cas == "gen" || cas == "dat" );
  }
  else if ( tag == CGN::VG ) {
    string cp = pa->feat("conjtype");
    isOnder = cp == "onder";
  }
  return prop;
}

double wordStats::checkPolarity( ) const {
  string key = lowercase(word)+":";
  if ( tag == CGN::N )
    key += "n";
  else if ( tag == CGN::ADJ )
    key += "a";
  else if ( tag == CGN::WW )
    key += "v";
  else
    return NA;
  map<string,double>::const_iterator it = settings.pol_lex.find( key );
  if ( it != settings.pol_lex.end() ){
    return it->second;
  }
  return NA;
}

SemType get_sem_type( const string& lemma, CGN::Type tag ){
  if ( tag == CGN::N ){
    map<string,string>::const_iterator it = settings.noun_sem.find( lemma );
    if ( it != settings.noun_sem.end() ){
      string type = it->second;
      if ( type == "human" )
	return CONCRETE_HUMAN;
      else if ( type == "concrother" || type == "substance" 
		|| type == "artefact" || type == "nonhuman" )
	return CONCRETE_NOUN;
      else if ( type == "dynamic" || type == "nondynamic" )
	return ABSTRACT_NOUN;
      else 
	return BROAD_NOUN;
    }
  }
  else if ( tag == CGN::ADJ ) {
    map<string,string>::const_iterator it = settings.adj_sem.find( lemma );
    if ( it != settings.adj_sem.end() ){
      string type = it->second;
      if ( type == "emomen" )
	return EMO_ADJ;
      else if ( type == "phyper" || type == "stuff" || type == "colour" )
	return CONCRETE_ADJ;
      else if ( type == "abstract" )
	return ABSTRACT_ADJ;
      else 
	return BROAD_ADJ;
    }
  }
  else if ( tag == CGN::WW ) {
    map<string,string>::const_iterator it = settings.verb_sem.find( lemma );
    if ( it != settings.verb_sem.end() ){
      string type = it->second;
      if ( type == "state" )
	return STATE;
      else if ( type == "action" )
	return ACTION;
      else if ( type == "process" )
	return PROCESS;
      else
	return WEIRD;
    }
  }
  return UNFOUND;
}
  
SemType wordStats::checkSemProps( ) const {
  SemType type = get_sem_type( lemma, tag );
  return type;
}

void wordStats::topFreqLookup(){
  map<string,top_val>::const_iterator it = settings.top_freq_lex.find( lowercase(word) );
  top_freq = notFound;
  if ( it != settings.top_freq_lex.end() ){
    top_freq = it->second;
  }
}

void wordStats::freqLookup(){
  map<string,cf_data>::const_iterator it = settings.word_freq_lex.find( lowercase(word) );
  if ( it != settings.word_freq_lex.end() ){
    word_freq = it->second.count;
    word_freq_log = log10(word_freq);
  }
  it = settings.lemma_freq_lex.find( lowercase(lemma) );
  if ( it != settings.lemma_freq_lex.end() ){
    lemma_freq = it->second.count;
    lemma_freq_log = log10(lemma_freq);
  }
}

void wordStats::staphFreqLookup(){
  map<string,cf_data>::const_iterator it = settings.staph_word_freq_lex.find( lowercase(word) );
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
  string lword = lowercase( word );
  if ( negatives.find( lword ) != negatives.end() ){
    return true;
  }
  else if ( tag == CGN::BW &&
	    ( lword == "moeilijk" || lword == "weg" ) ){
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
    string lword = lowercase( word );
    for ( size_t i=0; i < negminus.size(); ++i ){
      if ( word.find( negminus[i] ) != string::npos )
	return true;
    }
  }
  return false;
}

void argument_overlap( const string w_or_l, 
		       const vector<string>& buffer, 
		       int& arg_cnt, int& arg_overlap_cnt ){
  // calculate the overlap of the Word or Lemma with the buffer
  if ( buffer.empty() )
    return;
  // cerr << "test overlap, lemma/word= " << w_or_l << endl;
  // cerr << "buffer=" << buffer << endl;
  static string vnw_1sA[] = {"ik","mij","me","mijn"};
  static set<string> vnw_1s = set<string>( vnw_1sA, 
					   vnw_1sA + sizeof(vnw_1sA)/sizeof(string) );
  static string vnw_2sA[] = {"jij","je","jouw"};
  static set<string> vnw_2s = set<string>( vnw_2sA, 
					    vnw_2sA + sizeof(vnw_2sA)/sizeof(string) );
  static string vnw_3smA[] = {"hij", "hem", "zijn"};
  static set<string> vnw_3sm = set<string>( vnw_3smA, 
					    vnw_3smA + sizeof(vnw_3smA)/sizeof(string) );
  static string vnw_3sfA[] = {"zij","ze","haar"};
  static set<string> vnw_3sf = set<string>( vnw_3sfA, 
					    vnw_3sfA + sizeof(vnw_3sfA)/sizeof(string) );
  static string vnw_1pA[] = {"wij","we","ons","onze"};
  static set<string> vnw_1p = set<string>( vnw_1pA, 
					   vnw_1pA + sizeof(vnw_1pA)/sizeof(string) );
  static string vnw_2pA[] = {"jullie"};
  static set<string> vnw_2p = set<string>( vnw_2pA, 
					   vnw_2pA + sizeof(vnw_2pA)/sizeof(string) );
  static string vnw_3pA[] = {"zij","ze","hen","hun"};
  static set<string> vnw_3p = set<string>( vnw_3pA, 
					   vnw_3pA + sizeof(vnw_3pA)/sizeof(string) );

  ++arg_cnt; // we tellen ook het totaal aantal (mogelijke) argumenten om 
  // later op te kunnen delen 
  // (aantal overlappende argumenten op totaal aantal argumenten)
  for( size_t i=0; i < buffer.size(); ++i ){
    if ( w_or_l == buffer[i] )
      ++arg_overlap_cnt;
    else if ( vnw_1s.find( w_or_l ) != vnw_1s.end() &&
	      vnw_1s.find( buffer[i] ) != vnw_1s.end() )
      ++arg_overlap_cnt;
    else if ( vnw_2s.find( w_or_l ) != vnw_2s.end() &&
	      vnw_2s.find( buffer[i] ) != vnw_2s.end() )
      ++arg_overlap_cnt;	
    else if ( vnw_3sm.find( w_or_l ) != vnw_3sm.end() &&
	      vnw_3sm.find( buffer[i] ) != vnw_3sm.end() )
      ++arg_overlap_cnt;
    else if ( vnw_3sf.find( w_or_l ) != vnw_3sf.end() &&
	      vnw_3sf.find( buffer[i] ) != vnw_3sf.end() )
      ++arg_overlap_cnt;	
    else if ( vnw_1p.find( w_or_l ) != vnw_1p.end() &&
	      vnw_1p.find( buffer[i] ) != vnw_1p.end() )
      ++arg_overlap_cnt;
    else if ( vnw_2p.find( w_or_l ) != vnw_2p.end() &&
	      vnw_2p.find( buffer[i] ) != vnw_2p.end() )
      ++arg_overlap_cnt;	
    else if ( vnw_3p.find( w_or_l ) != vnw_3p.end() &&
	      vnw_3p.find( buffer[i] ) != vnw_3p.end() )
      ++arg_overlap_cnt;	
  }
}


wordStats::wordStats( Word *w, xmlDoc *alpDoc, const set<size_t>& puncts ):
  basicStats( w, "WORD" ), wwform(::NO_VERB),
  isPersRef(false), isPronRef(false),
  archaic(false), isContent(false), isNominal(false),isOnder(false), isImperative(false),
  isBetr(false), isPropNeg(false), isMorphNeg(false), connType(NOCONN),
  f50(false), f65(false), f77(false), f80(false),  compPartCnt(0), 
  top_freq(notFound), word_freq(0), lemma_freq(0),
  argRepeatCnt(0), wordOverlapCnt(0), lemmaRepeatCnt(0), lemmaOverlapCnt(0),
  word_freq_log(NA), lemma_freq_log(NA),
  polarity(NA), surprisal(NA), logprob10(NA), prop(JUSTAWORD), sem_type(UNFOUND)
{
  word = UnicodeToUTF8( w->text() );
  charCnt = w->text().length();

  vector<PosAnnotation*> posV = w->select<PosAnnotation>(frog_pos_set);
  if ( posV.size() != 1 )
    throw ValueError( "word doesn't have Frog POS tag info" );
  PosAnnotation *pa = posV[0];
  pos = pa->cls();
  tag = CGN::toCGN( pa->feat("head") );
  lemma = w->lemma( frog_lemma_set );
  prop = checkProps( pa );
  if ( alpDoc ){
    if ( tag == CGN::WW ){
      wwform = classifyVerb( w, alpDoc );
      if ( prop == ISPVTGW || prop == ISPVVERL ){
	isImperative = checkImp( w, alpDoc );
      }
    }
    distances = getDependencyDist( w, alpDoc, puncts);
  }
  isContent = checkContent();
  if ( prop != ISPUNCT ){
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
    connType = checkConnective();
    morphCnt = max;
    if ( prop != ISNAME ){
      charCntExNames = charCnt;
      morphCntExNames = max;
    }
    if (alpDoc) isNominal = checkNominal( w, alpDoc );
    polarity = checkPolarity();
    sem_type = checkSemProps();
    if ( sem_type == CONCRETE_HUMAN ||
	 prop == ISNAME ||
	 prop == ISPPRON1 || prop == ISPPRON2 || prop == ISPPRON3 ){
      isPersRef = true;
    }
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

void wordStats::getSentenceOverlap( const Sentence *prev ){
  if ( prev &&
       ( ( tag == CGN::VNW && prop != ISAANW ) ||
	 ( tag == CGN::N ) ||
	 ( pos == "SPEC(deeleigen)" ) ) ){
    vector<string> wordbuffer;
    vector<string> lemmabuffer;
    // get the words and lemmas' of the previous sentence
    vector<Word*> wv = prev->words();
    for ( size_t w=0; w < wv.size(); ++w ){
      wordbuffer.push_back( lowercase( UnicodeToUTF8( wv[w]->text() ) ) );
      lemmabuffer.push_back( lowercase(wv[w]->lemma( frog_lemma_set ) ) );
    }
    // cerr << "call word sentenceOverlap, word = " << lowercase(word) 
    // 	 << " prev=" << wordbuffer << endl;
    // int tmp1 = argRepeatCnt;
    // int tmp2 = wordOverlapCnt;
    argument_overlap( lowercase(word), wordbuffer, argRepeatCnt, wordOverlapCnt );
    // if ( tmp1 != argRepeatCnt ){
    //   cerr << "argument repeated " << argRepeatCnt - tmp1 << endl;
    // }
    // if ( tmp2 != wordOverlapCnt ){
    //   cerr << "word Overlapped " << wordOverlapCnt - tmp2 << endl;
    // }

    // cerr << "call lemma sentenceOverlap, lemma= " << lowercase(lemma)
    // 	 << " prev=" << lemmabuffer << endl;
    // tmp1 = lemmaRepeatCnt;
    // tmp2 = lemmaOverlapCnt;
    argument_overlap( lowercase(lemma), lemmabuffer, lemmaRepeatCnt, lemmaOverlapCnt );
    // if ( tmp1 != lemmaRepeatCnt ){
    //   cerr << "lemma argument repeated " << lemmaRepeatCnt - tmp1 << endl;
    // }
    // if ( tmp2 != lemmaOverlapCnt ){
    //   cerr << "lemma Overlapped " << lemmaOverlapCnt - tmp2 << endl;
    // }

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
		"argument_repeat_count", toString( argRepeatCnt ) );
  addOneMetric( doc, el, 
		"word_overlap_count", toString( wordOverlapCnt ) );
  addOneMetric( doc, el, 
		"lemma_argument_repeat_count", toString( lemmaRepeatCnt ) );
  addOneMetric( doc, el, 
		"lemma_overlap_count", toString( lemmaOverlapCnt ) );
  if ( polarity != NA  )
    addOneMetric( doc, el, "polarity", toString(polarity) );
  if ( surprisal != NA  )
    addOneMetric( doc, el, "surprisal", toString(surprisal) );
  if ( logprob10 != NA  )
    addOneMetric( doc, el, "lprob10", toString(logprob10) );
  if ( prop != JUSTAWORD )
    addOneMetric( doc, el, "property", toString(prop) );
  if ( sem_type != UNFOUND )
    addOneMetric( doc, el, "semtype", toString(sem_type) );
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
     << "freq1000,freq2000,freq3000,freq5000,freq10000,freq20000,";
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
}

void wordStats::coherenceHeader( ostream& os ) const {
  os << "temporeel,reeks,contrastief,comparatief,causaal,referential_pron,";
} 

void wordStats::coherenceToCSV( ostream& os ) const {
  os << (connType==TEMPOREEL?1:0) << ","
     << (connType==REEKS?1:0) << ","
     << (connType==CONTRASTIEF) << ","
     << (connType==COMPARATIEF) << ","
     << (connType==CAUSAAL) << ","
     << isPronRef << ",";
}

void wordStats::concreetHeader( ostream& os ) const {
  os << "noun_conc_strict,";
  os << "noun_conc_broad,";
  os << "adj_conc_strict,";
  os << "adj_conc_broad,";
}

void wordStats::concreetToCSV( ostream& os ) const {
  if ( sem_type == CONCRETE_HUMAN || sem_type == CONCRETE_NOUN ){
    os << "1,0,0,0,";
  }
  else if ( sem_type == BROAD_NOUN ){
    os << "0,1,0,0,";
  }
  else if ( sem_type  == CONCRETE_ADJ ){
    os << "0,0,1,0,";
  }
  else if ( sem_type == BROAD_ADJ ){
    os << "0,0,0,1,";
  }
  else {
    os << "0,0,0,0,";
  } 
}

void wordStats::persoonlijkheidHeader( ostream& os ) const {
  os << "pers_ref,pers_pron_1,pers_pron_2,pers_pron_3,pers_pron,"
     << "name,"
     << "action_verb,state_verb,"
     << "process_verb,human_noun,"
     << "emo_adj,imperative,"
     << "question,polarity,";
}

void wordStats::persoonlijkheidToCSV( ostream& os ) const {
  os << isPersRef << ","
     << (prop == ISPPRON1 ) << ","
     << (prop == ISPPRON2 ) << ","
     << (prop == ISPPRON3 ) << ","
     << (prop == ISPPRON1 || prop == ISPPRON2 || prop == ISPPRON3) << ","
     << (prop == ISNAME) << ","
     << (sem_type == ACTION ) << ","
     << (sem_type == STATE ) << ","
     << (sem_type == PROCESS ) << ","
     << (sem_type == CONCRETE_HUMAN ) << ","
     << (sem_type == EMO_ADJ ) << ","
     << isImperative << ","
     << "NA,";
  if ( polarity == NA )
    os << "NA,";
  else
    os << polarity << ",";
}

void wordStats::wordSortHeader( ostream& os ) const {
  os << "adj,vg,vnw,lid,vz,bijw,tw,noun,verb,interjections,";
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
     << (tag == CGN::TSW ) << ",";
}

void wordStats::miscHeader( ostream& os ) const {
  os << "present_verb,modal,time_verb,copula,archaic,"
     << "vol_deelw,onvol_deelw,infin,surprisal,wopr_logprob";
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
  if ( surprisal == NA )
    os << "NA,";
  else
    os << surprisal << ",";
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

struct structStats: public basicStats {
  structStats( FoliaElement *el, const string& cat ): 
    basicStats( el, cat ),
    wordCnt(0),
    sentCnt(0),
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
    onderCnt(0),
    betrCnt(0),
    reeksCnt(0),
    cnjCnt(0),
    crdCnt(0),
    tempConnCnt(0),
    reeksConnCnt(0),
    contConnCnt(0),
    compConnCnt(0),
    causeConnCnt(0),
    propNegCnt(0),
    morphNegCnt(0),
    multiNegCnt(0),
    argRepeatCnt(0),
    wordOverlapCnt(0),
    lemmaRepeatCnt(0),
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
    polarity(NA),
    surprisal(NA),
    avg_prob10(NA),
    entropy(NA),
    perplexity(NA),
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
    npModCnt(0),
    dLevel(-1),
    dLevel_gt4(0),
    impCnt(0),
    questCnt(0)
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
  string id;
  string text;
  int wordCnt;
  int sentCnt;
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
  int onderCnt;
  int betrCnt;
  int reeksCnt;
  int cnjCnt;
  int crdCnt;
  int tempConnCnt;
  int reeksConnCnt;
  int contConnCnt;
  int compConnCnt;
  int causeConnCnt;
  int propNegCnt;
  int morphNegCnt;
  int multiNegCnt;
  int argRepeatCnt;
  int wordOverlapCnt;
  int lemmaRepeatCnt;
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
  double polarity;
  double surprisal;
  double avg_prob10;
  double entropy;
  double perplexity;
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
  int npModCnt;
  int dLevel;
  int dLevel_gt4;
  int impCnt;
  int questCnt;
  map<CGN::Type,int> heads;
  map<string,int> unique_words;
  map<string,int> unique_lemmas;
  map<NerProp, int> ners;
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
  onderCnt += ss->onderCnt;
  betrCnt += ss->betrCnt;
  reeksCnt += ss->reeksCnt;
  cnjCnt += ss->cnjCnt;
  crdCnt += ss->crdCnt;
  tempConnCnt += ss->tempConnCnt;
  reeksConnCnt += ss->reeksConnCnt;
  contConnCnt += ss->contConnCnt;
  compConnCnt += ss->compConnCnt;
  causeConnCnt += ss->causeConnCnt;
  propNegCnt += ss->propNegCnt;
  morphNegCnt += ss->morphNegCnt;
  multiNegCnt += ss->multiNegCnt;
  argRepeatCnt += ss->argRepeatCnt;
  wordOverlapCnt += ss->wordOverlapCnt;
  lemmaRepeatCnt += ss->lemmaRepeatCnt;
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
  if ( ss->polarity != NA ){
    if ( polarity == NA )
      polarity = ss->polarity;
    else
      polarity += ss->polarity;
  }
  if ( ss->surprisal != NA ){
    if ( surprisal == NA )
      surprisal = ss->surprisal;
    else
      surprisal += ss->surprisal;
  }
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
      perplexity = ss->surprisal;
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
  aggregate( unique_words, ss->unique_words );
  aggregate( unique_lemmas, ss->unique_lemmas );
  aggregate( ners, ss->ners );
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
  double result = 0.0;
  for( multimap<DD_type, int>::const_iterator pos = mm.begin(); 
       pos != mm.end();
       ++pos ){
    if ( pos->second > result )
      result = pos->second;
  }
  return result;
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
  addOneMetric( doc, el, "subord_count", toString(onderCnt) );
  addOneMetric( doc, el, "rel_count", toString(betrCnt) );
  addOneMetric( doc, el, "reeks_count", toString(reeksCnt) );
  addOneMetric( doc, el, "cnj_count", toString(cnjCnt) );
  addOneMetric( doc, el, "crd_count", toString(crdCnt) );
  addOneMetric( doc, el, "temporals_count", toString(tempConnCnt) );
  addOneMetric( doc, el, "reeks_connector_count", toString(reeksConnCnt) );
  addOneMetric( doc, el, "contrast_count", toString(contConnCnt) );
  addOneMetric( doc, el, "comparatief_count", toString(compConnCnt) );
  addOneMetric( doc, el, "causaal_count", toString(causeConnCnt) );
  addOneMetric( doc, el, "prop_neg_count", toString(propNegCnt) );
  addOneMetric( doc, el, "morph_neg_count", toString(morphNegCnt) );
  addOneMetric( doc, el, "multiple_neg_count", toString(multiNegCnt) );
  addOneMetric( doc, el, 
		"argument_repeat_count", toString( argRepeatCnt ) );
  addOneMetric( doc, el, 
		"word_overlap_count", toString( wordOverlapCnt ) );
  addOneMetric( doc, el, 
		"lemma_argument_repeat_count", toString( lemmaRepeatCnt ) );
  addOneMetric( doc, el, 
		"lemma_overlap_count", toString( lemmaOverlapCnt ) );
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
  if ( word_freq_log != NA  )
    addOneMetric( doc, el, "log_lemma_freq", toString(lemma_freq_log) );
  if ( lemma_freq_log_n != NA  )
    addOneMetric( doc, el, "log_lemma_freq_no_names", toString(lemma_freq_log_n) );
  if ( polarity != NA )
    addOneMetric( doc, el, "polarity", toString(polarity) );
  if ( surprisal != NA )
    addOneMetric( doc, el, "surprisal", toString(surprisal) );
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
  addOneMetric( doc, el, "max_deplen", toString( getHighest( distances )/sentCnt ) );
  for ( size_t i=0; i < sv.size(); ++i ){
    sv[i]->addMetrics();
  }
}

void structStats::CSVheader( ostream& os, const string& intro ) const {
  os << intro << ",";
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
     << "freq1000,freq2000,freq3000,freq5000,freq10000,freq20000,";
}

void structStats::wordDifficultiesToCSV( ostream& os ) const {
  os << std::showpoint
     << charCnt/double(wordCnt) << "," 
     << wordCnt/double(charCnt) <<  ",";
  if ( wordCnt == nameCnt )
    os << "NA,NA,"
       << morphCnt/double(wordCnt) << "," 
       << wordCnt/double(morphCnt) << ","
       << "NA,NA,";
  else {
    os << charCntExNames/double(wordCnt-nameCnt) << ","
       << double(wordCnt - nameCnt)/charCntExNames <<  ","
       << morphCnt/double(wordCnt) << "," 
       << wordCnt/double(morphCnt) << ","
       << morphCntExNames/double(wordCnt-nameCnt) << "," 
       << double(wordCnt-nameCnt)/double(morphCntExNames) << ",";
  }
  os << compPartCnt/double(wordCnt) << ","
     << compCnt/double(wordCnt) *1000.0 << ",";

  os << f50Cnt/double(wordCnt) << ",";
  os << f65Cnt/double(wordCnt) << ",";
  os << f77Cnt/double(wordCnt) << ",";
  os << f80Cnt/double(wordCnt) << ",";
  if ( word_freq_log == NA )
    os << "NA,";
  else
    os << word_freq_log/double(wordCnt) << ",";
  if ( wordCnt == nameCnt || word_freq_log_n == NA )
    os << "NA" << ",";
  else
    os << word_freq_log_n/double(wordCnt-nameCnt) << ",";
  if ( lemma_freq_log == NA )
    os << "NA,";
  else
    os << lemma_freq_log/double(wordCnt) << ",";
  if ( wordCnt == nameCnt || lemma_freq_log_n == NA )
    os << "NA" << ",";
  else
    os << lemma_freq_log_n/double(wordCnt-nameCnt) << ",";
  os << top1000Cnt/double(wordCnt) << ",";
  os << top2000Cnt/double(wordCnt) << ",";
  os << top3000Cnt/double(wordCnt) << ",";
  os << top5000Cnt/double(wordCnt) << ",";
  os << top10000Cnt/double(wordCnt) << ",";
  os << top20000Cnt/double(wordCnt) << ",";
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
  os << wordCnt/double(sentCnt) << ","
     << double(sentCnt)/wordCnt * 100.0 << ",";
  if ( npCnt == 0 )
    os << "NA,";
  else
    os << wordCnt/double(npCnt) << ",";
  os << onderCnt/double(wordCnt) * 1000 << ","
     << betrCnt/double(wordCnt) * 1000 << ","
     << (pastCnt + presentCnt)/double(wordCnt) * 1000 << ","
     << (pastCnt + presentCnt)/double(sentCnt) << ","
     << dLevel/double(sentCnt) << "," 
     << dLevel_gt4/double(sentCnt) << ",";
  if ( sentCnt == dLevel_gt4 )
    os << "NA,";
  else
    os<< dLevel_gt4/double(sentCnt - dLevel_gt4) << ",";
  os << nominalCnt/double(wordCnt) * 1000 << ","
     << passiveCnt/double(wordCnt) * 1000 << ",";
  if ( (pastCnt+presentCnt) == 0 )
    os << "NA,";
  else
    os << passiveCnt/double(pastCnt+presentCnt) << ",";
  os << propNegCnt/double(wordCnt) * 1000 << ","
     << morphNegCnt/double(wordCnt) * 1000 << ","
     << (propNegCnt+morphNegCnt)/double(wordCnt) * 1000 << ","
     << multiNegCnt/double(sentCnt) * 1000 << ",";
  //  cerr << distances << endl;
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
  os << getHighest( distances )/sentCnt << ",";
}

void structStats::infoHeader( ostream& os ) const {
  os << "word_ttr,lemma_ttr,content_words_r,content_words_d,content_words_g,"
     << "rar_index,vc_mods_d,vc_mods_g,np_mods_d,np_mods_g,np_dens,conjuncts,";
}
 
void structStats::informationDensityToCSV( ostream& os ) const {
  os << unique_words.size()/double(wordCnt) << ",";
  os << unique_lemmas.size()/double(wordCnt) << ",";
  os << contentCnt/double(wordCnt - contentCnt) << ",";
  os << contentCnt/double(wordCnt) * 1000 << ",";
  if ( (pastCnt + presentCnt) == 0 )
    os << "NA,";
  else
    os << contentCnt/double(pastCnt + presentCnt) << ",";
  os << rarity( settings.rarityLevel ) << ",";
  os << vcModCnt/double(wordCnt) * 1000 << ",";
  if ( (pastCnt + presentCnt) == 0 )
    os << "NA,";
  else
    os << vcModCnt/double(pastCnt + presentCnt) << ",";
  os << npModCnt/double(wordCnt) * 1000 << ",";
  if ( (pastCnt + presentCnt) == 0 )
    os << "NA,";
  else
    os << npModCnt/double(pastCnt + presentCnt) << ",";
  os << npCnt/double(wordCnt) << ",";
  if ( crdCnt == 0 )
    os << "NA,";
  else
    os << (cnjCnt/double(crdCnt)) * 1000 << ",";
}

 
void structStats::coherenceHeader( ostream& os ) const {
  os << "temporals,reeks,contrast,comparatief,causal,referential_prons,"
     << "argument_overlap_d,argument_overlap_g,lem_argument_overlap_d,"
     << "lem_argument_overlap_g,wordbuffer_argument_overlap_d,"
     << "wordbuffer_argument_overlap_g,lemmabuffer_argument_overlap_d,"
     << "lemmabuffer_argument_overlap_g,indef_nps_p,indef_nps_r,indef_nps_g,";
}

void structStats::coherenceToCSV( ostream& os ) const {
  os << (tempConnCnt/double(wordCnt)) * 1000 << ","
     << (reeksConnCnt/double(wordCnt)) * 1000 << ","
     << (contConnCnt/double(wordCnt)) * 1000 << ","
     << (compConnCnt/double(wordCnt)) * 1000 << ","
     << (causeConnCnt/double(wordCnt)) * 1000 << ","
     << (pronRefCnt/double(wordCnt)) * 1000 << ",";
  if ( isSentence() ){
    os << double(argRepeatCnt) << ",NA,"
       << double(lemmaRepeatCnt) << ",NA,";
  }
  else {
    os << (argRepeatCnt/double(wordCnt)) * 1000 << ","
       << (argRepeatCnt/double(sentCnt)) << ",";
    os << (lemmaRepeatCnt/double(wordCnt)) * 1000 << ","
       << (lemmaRepeatCnt/double(sentCnt)) << ",";
  }
  if ( !isDocument() ){
    os << "NA,NA,NA,NA,";
  }
  else {
    os << (word_overlapCnt()/double(wordCnt)) * 1000 << ","
       << word_overlapCnt()/double(presentCnt+pastCnt) << ","
       << (lemma_overlapCnt()/double(wordCnt)) * 1000 << ","
       << lemma_overlapCnt()/double(presentCnt+pastCnt) << ",";
  }
  if ( npCnt == 0 )
    os << "NA,";
  else
    os << indefNpCnt/double(npCnt) << ",";
  if ( npCnt == indefNpCnt )
    os << "NA,";
  else
    os << indefNpCnt/double(npCnt - indefNpCnt) << ",";
  os << (indefNpCnt/double(wordCnt)) * 1000 << ",";
}

void structStats::concreetHeader( ostream& os ) const {
  os << "noun_conc_strict_p,noun_conc_strict_r,noun_conc_strict_d,";
  os << "noun_conc_broad_p,noun_conc_broad_r,noun_conc_broad_d,";
  os << "adj_conc_strict_p,adj_conc_strict_r,adj_conc_strict_d,";
  os << "adj_conc_broad_p,adj_conc_broad_r,adj_conc_broad_d,";
}

void structStats::concreetToCSV( ostream& os ) const {
  if ( nounCnt == 0 ){
    os << "NA,";
  }
  else {
    os << strictConcreteNounCnt/double(nounCnt) << ",";
  }
  if ( broadAbstractNounCnt == 0 ){
    os << "NA,";
  }
  else {
    os << strictConcreteNounCnt/double(broadAbstractNounCnt) << ",";
  }
  os << (strictConcreteNounCnt/double(wordCnt)) * 1000 << ",";
  if ( nounCnt == 0 ){
    os << "NA,";
  }
  else {
    os << broadConcreteNounCnt/double(nounCnt) << ",";
  }
  if ( broadAbstractNounCnt == 0 ){
    os << "NA,";
  }
  else {
    os << broadConcreteNounCnt/double(broadAbstractNounCnt) << ",";
  }
  os << (broadConcreteNounCnt/double(wordCnt)) * 1000 << ",";
  if ( adjCnt == 0 ){
    os << "NA,";
  }
  else {
    os << strictConcreteAdjCnt/double(adjCnt) << ",";
  }
  if ( broadAbstractAdjCnt == 0 ){
    os << "NA,";
  }
  else {
    os << strictConcreteAdjCnt/double(broadAbstractAdjCnt) << ",";
  }
  os << (strictConcreteAdjCnt/double(wordCnt)) * 1000 << ",";
  if ( adjCnt == 0 ){
    os << "NA,";
  }
  else {
    os << broadConcreteAdjCnt/double(adjCnt) << ",";
  }
  if ( broadAbstractAdjCnt == 0 ){
    os << "NA,";
  }
  else {
    os << broadConcreteAdjCnt/double(broadAbstractAdjCnt) << ",";
  }
  os << (broadConcreteAdjCnt/double(wordCnt)) * 1000 << ",";
}


void structStats::persoonlijkheidHeader( ostream& os ) const {
  os << "pers_ref_d,pers_pron_1,pers_pron_2,pers_pron3,pers_pron,"
     << "names_p,names_r,names_d,"
     << "action_verbs_p,action_verbs_d,state_verbs_p,state_verbs_d,"
     << "process_verbs_p,process_verbs_d,human_nouns_p,human_nouns_d,"
     << "emo_adjs_p,emo_adjs_d,imperatives_p,imperatives_d,"
     << "questions_p,questions_d,polarity,";
}

void structStats::persoonlijkheidToCSV( ostream& os ) const {
  os << (persRefCnt/double(wordCnt)) * 1000 << ",";
  os << (pron1Cnt/double(wordCnt)) * 1000 << ",";
  os << (pron2Cnt/double(wordCnt)) * 1000 << ",";
  os << (pron3Cnt/double(wordCnt)) * 1000 << ",";
  os << ((pron1Cnt+pron2Cnt+pron3Cnt)/double(wordCnt)) * 1000 << ",";
  if ( nounCnt == 0 )
    os << "NA,";
  else
    os << nameCnt/double(nounCnt) << ",";
  if ( nameCnt == wordCnt )
    os << "NA,";
  else
    os << nameCnt/double(wordCnt - nameCnt) << ",";
  os << (nameCnt/double(wordCnt)) * 1000 << ",";
  if ( verbCnt == 0 )
    os << "NA,";
  else
    os << actionCnt/double(verbCnt) << ",";
  os << (actionCnt/double(wordCnt)) * 1000 << ",";
  if ( verbCnt == 0 )
    os << "NA,";
  else
    os << stateCnt/double(verbCnt) << ",";
  os << (stateCnt/double(wordCnt)) * 1000 << ",";
  if ( verbCnt == 0 )
    os << "NA,";
  else
    os << processCnt/double(verbCnt) << ",";
  os << (processCnt/double(wordCnt)) * 1000 << ",";
  if ( nounCnt == 0 )
    os << "NA,";
  else
    os << humanCnt/double(nounCnt) << ",";
  os << (humanCnt/double(wordCnt)) * 1000 << ",";
  if ( adjCnt == 0 )
    os << "NA,";
  else
    os << emoCnt/double(adjCnt) << ",";
  os << (emoCnt/double(wordCnt)) * 1000 << ",";
  os << impCnt/double(sentCnt) << ",";
  os << (impCnt/double(wordCnt)) * 1000 << ",";
  os << questCnt/double(sentCnt) << ",";
  os << (questCnt/double(wordCnt)) * 1000 << ",";
  if ( polarity == NA )
    os << "NA,";
  else
    os << polarity << ",";
}

void structStats::wordSortHeader( ostream& os ) const {
  os << "adj,vg,vnw,lid,vz,bijw,tw,noun,verb,interjections,";
}

void structStats::wordSortToCSV( ostream& os ) const {
  os << (adjCnt/double(wordCnt)) * 1000 << ","
     << (vgCnt/double(wordCnt)) * 1000 << ","
     << (vnwCnt/double(wordCnt)) * 1000 << ","
     << (lidCnt/double(wordCnt)) * 1000 << ","
     << (vzCnt/double(wordCnt)) * 1000 << ","
     << (bwCnt/double(wordCnt)) * 1000 << ","
     << (twCnt/double(wordCnt)) * 1000 << ","
     << (nounCnt/double(wordCnt)) * 1000 << ","
     << (verbCnt/double(wordCnt)) * 1000 << ","
     << (tswCnt/double(wordCnt)) * 1000 << ",";
}

void structStats::miscHeader( ostream& os ) const {
  os << "present_verbs_r,present_verbs_d,modals_d_,modals_g,"
     << "time_verbs_d,time_verbs_g,copula_d,copula_g,"
     << "archaics,vol_deelw_d,vol_deelw_g,"
     << "onvol_deelw_d,onvol_deelw_g,infin_d,infin_g,surprisal,"
     << "wopr_logprob,wopr_entropy,wopr_perplexity,";
}

void structStats::miscToCSV( ostream& os ) const {
  if ( pastCnt == 0 ){
    os << "NA,";
  }
  else {
    os << presentCnt/double(pastCnt) << ",";
  }
  os  << (presentCnt/double(wordCnt)) * 1000 << ","
      << (modalCnt/double(wordCnt)) * 1000 << ",";
  if ( pastCnt + presentCnt == 0 ){
    os << "NA,";
  }
  else {
    os << modalCnt/double(presentCnt + pastCnt) << ",";
  }
  os << (timeCnt/double(wordCnt)) * 1000 << ",";
  if ( pastCnt + presentCnt == 0 ){
    os << "NA,";
  }
  else {
    os << timeCnt/double(presentCnt + pastCnt) << ",";
  }
  os << (koppelCnt/double(wordCnt)) * 1000 << ",";
  if ( pastCnt + presentCnt == 0 ){
    os << "NA,";
  }
  else {
    os << koppelCnt/double(presentCnt + pastCnt) << ",";
  }
  os << (archaicsCnt/double(wordCnt)) * 1000 << ","
     << (vdCnt/double(wordCnt)) * 1000 << ",";
  if ( pastCnt + presentCnt == 0 ){
    os << "NA,";
  }
  else {
    os << vdCnt/double(pastCnt + presentCnt) << ",";
  }
  os << (odCnt/double(wordCnt)) * 1000 << ",";
  if ( pastCnt + presentCnt == 0 ){
    os << "NA,";
  }
  else {
    os << odCnt/double(pastCnt + presentCnt) << ",";
  }
  os << (infCnt/double(wordCnt)) * 1000 << ",";
  if ( pastCnt + presentCnt == 0 ){
    os << "NA,";
  }
  else {
    os << infCnt/double(pastCnt + presentCnt) << ",";
  }
  if ( surprisal == NA )
    os << "NA,";
  else
    os << surprisal/double(sentCnt) << ",";
  if ( avg_prob10 == NA )
    os << "NA,";
  else
    os << avg_prob10/double(sentCnt) << ",";
  if ( entropy == NA )
    os << "NA,";
  else
    os << entropy/double(sentCnt) << ",";
  if ( perplexity == NA )
    os << "NA,";
  else
    os << perplexity/double(sentCnt) << ",";
}

struct sentStats : public structStats {
  sentStats( Sentence *, Sentence * );
  bool isSentence() const { return true; };
  void resolveConnectives();
  void addMetrics( ) const;
};

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

void sentStats::resolveConnectives(){
  if ( sv.size() > 1 ){
    for ( size_t i=0; i < sv.size()-2; ++i ){
      string multiword2 = lowercase( sv[i]->text() )
	+ " " + lowercase( sv[i+1]->text() );
      //      cerr << "zoek op " << multiword2 << endl;
      if ( sv[i+1]->getConnType() == NOCONN ){
	// no result yet
	ConnType conn = check2Connectives( multiword2 );
	switch( conn ){
	case TEMPOREEL:
	  tempConnCnt++;
	  break;
	case REEKS:
	  reeksConnCnt++;
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
      if ( negatives_long.find( multiword2 ) != negatives_long.end() ){
	propNegCnt++;
      }
      string multiword3 = multiword2 + " "
	+ lowercase( sv[i+2]->text() );
      //      cerr << "zoek op " << multiword3 << endl;
      if ( sv[sv.size()-1]->getConnType() == NOCONN ){
	// no result yet
	ConnType conn = check3Connectives( multiword3 );
	switch( conn ){
	case TEMPOREEL:
	  tempConnCnt++;
	  break;
	case REEKS:
	  reeksConnCnt++;
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
      if ( negatives_long.find( multiword3 ) != negatives_long.end() )
	propNegCnt++;
    }
    string multiword2 = lowercase( sv[sv.size()-2]->text() )
    + " " + lowercase( sv[sv.size()-1]->text() );
    //    cerr << "zoek op " << multiword2 << endl;
    if ( sv[sv.size()-1]->getConnType() == NOCONN ){
      // no result yet
      ConnType conn = check2Connectives( multiword2 );
      switch( conn ){
      case TEMPOREEL:
	tempConnCnt++;
	break;
      case REEKS:
	reeksConnCnt++;
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
    if ( negatives_long.find( multiword2 ) != negatives_long.end() ){
      propNegCnt++;
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
	LOG << "OESP" << endl;
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
	LOG << "OESP 2" << endl;
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

sentStats::sentStats( Sentence *s, Sentence *prev ): 
  structStats( s, "ZIN" ){
  sentCnt = 1;
  id = s->id();
  text = UnicodeToUTF8( s->toktext() );
  vector<Word*> w = s->words();
  vector<double> surprisalV(w.size(),NA);
  vector<double> woprProbsV(w.size(),NA);
  double sentProb = NA;
  double sentEntropy = NA;
  double sentPerplexity = NA;
  xmlDoc *alpDoc = 0;
  set<size_t> puncts;
  if ( settings.doSurprisal 
       && w.size() != 1 ){ // the surpisal pasrer chokes on 1 word sentences
    surprisalV = runSurprisal( s, workdir_name, settings.surprisalPath );
    if ( surprisalV.size() != w.size() ){
      cerr << "MISMATCH! " << surprisalV.size() << " != " <<  w.size()<< endl;
      surprisalV.clear();
    }
  }
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
	  for( size_t i=0; i < w.size(); ++i ){
	    vector<PosAnnotation*> posV = w[i]->select<PosAnnotation>(frog_pos_set);
	    if ( posV.size() != 1 )
	      throw ValueError( "word doesn't have Frog POS tag info" );
	    PosAnnotation *pa = posV[0];
	    string posHead = pa->feat("head");
	    if ( posHead == "LET" )
	      puncts.insert( i );
	  }
	  dLevel = get_d_level( s, alpDoc );
	  if ( dLevel > 4 )
	    dLevel_gt4 = 1;
	  countCrdCnj( alpDoc, crdCnt, cnjCnt, reeksCnt );
	  int np2Cnt;
	  mod_stats( alpDoc, vcModCnt, np2Cnt, npModCnt );
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

  avg_prob10 = sentProb;
  entropy = sentEntropy;
  perplexity = sentPerplexity;
  //  cerr << "PUNCTS " << puncts << endl;
  bool question = false;
  for ( size_t i=0; i < w.size(); ++i ){
    wordStats *ws = new wordStats( w[i], alpDoc, puncts );
    ws->surprisal = surprisalV[i];
    ws->logprob10 = woprProbsV[i];
    if ( prev ){
      ws->getSentenceOverlap( prev );
    }
    if ( ws->lemma == "?" ){
      question = true;
    }
    if ( ws->prop == ISPUNCT ){
      sv.push_back( ws );
      continue;
    }
    else {
      if ( settings.doSurprisal ){
	if ( surprisal == NA )
	  surprisal = ws->surprisal;
	else
	  surprisal += ws->surprisal;
      }
      NerProp ner = lookupNer( w[i], s );
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
      
      argRepeatCnt += ws->argRepeatCnt;
      wordOverlapCnt += ws->wordOverlapCnt;
      lemmaRepeatCnt += ws->lemmaRepeatCnt;
      lemmaOverlapCnt += ws->lemmaOverlapCnt;
      charCnt += ws->charCnt;
      charCntExNames += ws->charCntExNames;
      morphCnt += ws->morphCnt;
      morphCntExNames += ws->morphCntExNames;
      unique_words[lowercase(ws->word)] += 1;
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
      if ( ws->isContent )
	contentCnt++;
      if ( ws->isNominal )
	nominalCnt++;
      if ( ws->tag == CGN::N )
	nounCnt++;
      else if ( ws->tag == CGN::ADJ )
	adjCnt++;
      else if ( ws->tag == CGN::WW )
	verbCnt++;
      else if ( ws->tag == CGN::VG )
	vgCnt++;
      else if ( ws->tag == CGN::TSW )
	tswCnt++;
      else if ( ws->tag == CGN::BW )
	bwCnt++;
      else if ( ws->tag == CGN::VNW )
	vnwCnt++;
      else if ( ws->tag == CGN::LID )
	lidCnt++;
      else if ( ws->tag == CGN::TW )
	twCnt++;
      else if ( ws->tag == CGN::VZ )
	vzCnt++;
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
      switch( ws->connType ){
      case TEMPOREEL:
	tempConnCnt++;
	break;
      case REEKS:
	reeksConnCnt++;
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
      if ( ws->polarity != NA ){
	if ( polarity == NA )
	  polarity = ws->polarity;
	else
	  polarity += ws->polarity;
      }
      switch ( ws->sem_type ){
      case CONCRETE_HUMAN:
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
  if ( question )
    questCnt = 1;
  if ( (morphNegCnt + propNegCnt) > 1 )
    multiNegCnt = 1;
  resolveConnectives();
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
  parStats( Paragraph * );
  void addMetrics( ) const;
};

parStats::parStats( Paragraph *p ): 
  structStats( p, "PARAGRAAF" )
{
  sentCnt = 0;
  id = p->id();
  vector<Sentence*> sents = p->sentences();
  for ( size_t i=0; i < sents.size(); ++i ){
    sentStats *ss;
    if ( i == 0 )
      ss = new sentStats( sents[i], 0 );
    else
      ss = new sentStats( sents[i], sents[i-1] );
    merge( ss );
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

struct docStats : public structStats {
  docStats( Document * );
  bool isDocument() const { return true; };
  void toCSV( const string&, csvKind ) const;
  string rarity( int level ) const;
  void addMetrics( ) const;
  int word_overlapCnt() const { return doc_word_overlapCnt; };
  int lemma_overlapCnt() const { return doc_lemma_overlapCnt; };
  int doc_word_argCnt;
  int doc_word_overlapCnt;
  int doc_lemma_argCnt;
  int doc_lemma_overlapCnt;
};

docStats::docStats( Document *doc ):
  structStats( 0, "DOCUMENT" ),
  doc_word_argCnt(0), doc_word_overlapCnt(0),
  doc_lemma_argCnt(0), doc_lemma_overlapCnt(0) 
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
  vector<Paragraph*> pars = doc->paragraphs();
  if ( pars.size() > 0 )
    folia_node = pars[0]->parent();
  for ( size_t i=0; i != pars.size(); ++i ){
    parStats *ps = new parStats( pars[i] );
    merge( ps );
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
  vector<Word*> wv = doc->words();
  vector<string> wordbuffer(settings.overlapSize);
  vector<string> lemmabuffer(settings.overlapSize);
  for ( size_t w=0; w < wv.size() && w < settings.overlapSize; ++w ){
    wordbuffer[w] = lowercase( UnicodeToUTF8( wv[w]->text() ) );
    lemmabuffer[w] = lowercase( wv[w]->lemma( frog_lemma_set ) );
  }
  for ( size_t i=settings.overlapSize; i < wv.size(); ++i ){
    vector<PosAnnotation*> posV = wv[i]->select<PosAnnotation>(frog_pos_set);
    if ( posV.size() != 1 )
      throw ValueError( "word doesn't have Frog POS tag info" );
    string head = posV[0]->feat("head");
    if ( ( head == "VNW" && posV[0]->feat( "vwtype" ) != "aanw" ) ||
	 ( head == "N" ) ||
	 ( wv[i]->pos( frog_pos_set ) == "SPEC(deeleigen)" ) ){
      string word = UnicodeToUTF8(wv[i]->text()); 
      argument_overlap( lowercase(word), wordbuffer, 
			doc_word_argCnt, doc_word_overlapCnt );
      wordbuffer.erase(wordbuffer.begin());
      wordbuffer.push_back( word );
      string lemma = wv[i]->lemma( frog_lemma_set );
      argument_overlap( lowercase(lemma), lemmabuffer, 
			doc_lemma_argCnt, doc_lemma_overlapCnt );
      lemmabuffer.erase(lemmabuffer.begin());
      lemmabuffer.push_back( lemma );
    }
  }
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
  addOneMetric( el->doc(), el, 
		"rar_index", rarity( settings.rarityLevel ) );
  addOneMetric( el->doc(), el, 
		"document_word_argument_count", toString( doc_word_argCnt ) );
  addOneMetric( el->doc(), el, 
		"document_word_argument_overlap_count", toString( doc_word_overlapCnt ) );
  addOneMetric( el->doc(), el, 
		"document_lemma_argument_count", toString( doc_lemma_argCnt ) );
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
      cout << "stored document statistics in " << fname << endl;
    }
    else {
      cout << "storing document statistics in " << fname << " FAILED!" << endl;
    }
  }
  else if ( what == PAR_CSV ){
    string fname = name + ".paragraphs.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      for ( size_t par=0; par < sv.size(); ++par ){
	if ( par == 0 )
	  sv[0]->CSVheader( out, "File,FoLiA-ID" );
	out << name << "," << sv[par]->folia_node->id() << ",";
	sv[par]->toCSV( out );
      }
      cout << "stored paragraph statistics in " << fname << endl;
    }
    else {
      cout << "storing paragraph statistics in " << fname << " FAILED!" << endl;
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
	  out << name << "," << sv[par]->sv[sent]->folia_node->id() << ",";
	  sv[par]->sv[sent]->toCSV( out );
	}
      }
      cout << "stored sentence statistics in " << fname << endl;
    }
    else {
      cout << "storing sentence statistics in " << fname << " FAILED!" << endl;
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
	    out << name << "," << sv[par]->sv[sent]->sv[word]->folia_node->id() << ",";
	    sv[par]->sv[sent]->sv[word]->toCSV( out );
	  }
	}
      }
      cout << "stored word statistics in " << fname << endl;
    }
    else {
      cout << "storing word statistics in " << fname << " FAILED!" << endl;
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
    if ( skip.find_first_of("aA") != string::npos ){
      settings.doAlpino = false;
      settings.doAlpinoServer = false;
    }
    if ( skip.find_first_of("sS") != string::npos ){
      settings.doSurprisal = false;
    }
    if ( skip.find_first_of("dD") != string::npos ){
      settings.doDecompound = false;
    }
    if ( skip.find_first_of("cC") != string::npos ){
      settings.doXfiles = false;
    }
    opts.Delete("skip");
  };

  string inName;
  string outName;
  if ( opts.Find( 't', val, mood ) ){
    inName = val;
  }
  else {
    cerr << "missing input file (-t option) " << endl;
    exit(EXIT_FAILURE);
  }
  if ( opts.Find( 'o', val, mood ) ){
    outName = val;
  }
  else {
    outName = inName + ".tscan.xml";
  }
  cerr << "TScan " << VERSION << endl;
  cerr << "working dir " << workdir_name << endl;
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
      // cerr << analyse << endl;
    }
  }
  
  exit(EXIT_SUCCESS);
}

