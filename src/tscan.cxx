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
#include "config.h"
#include "timblserver/TimblServerAPI.h"
#include "libfolia/folia.h"
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
const string frog_ner_set = "http://ilk.uvt.nl/folia/sets/frog-ner-nl";

enum csvKind { DOC_CSV, PAR_CSV, SENT_CSV, WORD_CSV };

string configFile = "tscan.cfg";
Configuration config;

struct cf_data {
  long int count;
  double freq;
};

struct settingData {
  void init( const Configuration& );
  bool doAlpino;
  bool doAlpinoServer;
  bool doDecompound;
  bool doSurprisal;
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
  map<string, cf_data> freq_lex;
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
    double value = stringTo<double>( parts[1] );
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
    data.count = stringTo<long int>( parts[1] );
    data.freq = stringTo<double>( parts[3] );
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

void settingData::init( const Configuration& cf ){
  string val = cf.lookUp( "useAlpinoServer" );
  doAlpinoServer = false;
  if ( !val.empty() ){
    if ( !Timbl::stringTo( val, doAlpinoServer ) ){
      cerr << "invalid value for 'useAlpinoServer' in config file" << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( !doAlpinoServer ){
    val = cf.lookUp( "useAlpino" );
    if( !Timbl::stringTo( val, doAlpino ) ){
      cerr << "invalid value for 'useAlpino' in config file" << endl;
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
  else if ( !Timbl::stringTo( val, rarityLevel ) ){ 
    cerr << "invalid value for 'rarityLevel' in config file" << endl;
  }
  val = cf.lookUp( "overlapSize" );
  if ( val.empty() ){
    overlapSize = 50;
  }
  else if ( !Timbl::stringTo( val, overlapSize ) ){ 
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
  else if ( !Timbl::stringTo( val, polarity_threshold ) ){ 
    cerr << "invalid value for 'polarity_threshold' in config file" << endl;
  }
  val = cf.lookUp( "polarity_lex" );
  if ( !val.empty() ){
    if ( !fill( pol_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
  val = cf.lookUp( "freq_lex" );
  if ( !val.empty() ){
    if ( !fill( freq_lex, cf.configDir() + "/" + val ) )
      exit( EXIT_FAILURE );
  }
}

inline void usage(){
  cerr << "usage:  tscan -t <inputfile> [-o <outputfile>]" << endl;
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

enum WordProp { ISNAME, ISPUNCT, 
		ISVD, ISOD, ISINF, ISPVTGW, ISPVVERL,
		ISPPRON1, ISPPRON2, ISPPRON3, ISAANW,
		JUSTAWORD };

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
	       ABSTRACT_NOUN, ABSTRACT_ADJ, BROAD_NOUN, BROAD_ADJ, 
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
  virtual void print( ostream& ) const = 0;
  virtual void wordDifficultiesToCSV( ostream& ) const = 0;
  virtual void sentDifficultiesToCSV( ostream& ) const = 0;
  virtual void informationDensityToCSV( ostream& ) const = 0;
  virtual void coherenceToCSV( ostream& ) const = 0;
  virtual void concreetToCSV( ostream& ) const = 0;
  virtual void persoonlijkheidToCSV( ostream& ) const = 0;;
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

ostream& operator<<( ostream& os, const basicStats& b ){
  b.print(os);
  return os;
}

void basicStats::print( ostream& os ) const {
  os << "BASICSTATS PRINT" << endl;
  os << category << endl;
  os << " lengte=" << charCnt << ", aantal morphemen=" << morphCnt << endl;
  os << " lengte zonder namem =" << charCntExNames << ", aantal morphemen zonder namen=" << morphCntExNames << endl;
}

struct wordStats : public basicStats {
  wordStats( Word *, xmlDoc *, const set<size_t>& );
  void print( ostream& ) const;
  static void CSVheader( ostream& os );
  void wordDifficultiesToCSV( ostream& ) const;
  void sentDifficultiesToCSV( ostream& ) const {};
  void informationDensityToCSV( ostream& ) const;
  void coherenceToCSV( ostream& ) const;
  void concreetToCSV( ostream& ) const;
  void persoonlijkheidToCSV( ostream& ) const;
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
  void freqLookup();
  void getSentenceOverlap( const Sentence * );
  string word;
  string pos;
  string posHead;
  string lemma;
  string wwform;
  bool isPassive;
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
  int wfreq;
  int argRepeatCnt;
  int wordOverlapCnt;
  int lemmaRepeatCnt;
  int lemmaOverlapCnt;
  double lwfreq;
  double polarity;
  double surprisal;
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
  if ( posHead == "VG" ){
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
  if ( posHead == "WW" ){
    if ( wwform == "hoofdww" ){
      return true;
    }
  }
  else {
    return ( posHead == "N" || posHead == "BW" || posHead == "ADJ" );
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
  if ( posHead == "N" && morphemes.size() > 1 ){
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
  if ( posHead == "LET" )
    prop = ISPUNCT;
  else if ( posHead == "SPEC" && pos.find("eigen") != string::npos )
    prop = ISNAME;
  else if ( posHead == "WW" ){
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
  else if ( posHead == "VNW" ){
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
  else if ( posHead == "LID" ) {
    string cas = pa->feat("case");
    archaic = ( cas == "gen" || cas == "dat" );
  }
  else if ( posHead == "VG" ) {
    string cp = pa->feat("conjtype");
    isOnder = cp == "onder";
  }
  return prop;
}

double wordStats::checkPolarity( ) const {
  string key = lowercase(word)+":";
  if ( posHead == "N" )
    key += "n";
  else if ( posHead == "ADJ" )
    key += "a";
  else if ( posHead == "WW" )
    key += "v";
  else
    return NA;
  map<string,double>::const_iterator it = settings.pol_lex.find( key );
  if ( it != settings.pol_lex.end() ){
    return it->second;
  }
  return NA;
}

SemType get_sem_type( const string& lemma, const string& pos ){
  if ( pos == "N" ){
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
  else if ( pos == "ADJ" ) {
    map<string,string>::const_iterator it = settings.adj_sem.find( lemma );
    if ( it != settings.adj_sem.end() ){
      string type = it->second;
      if ( type == "phyper" || type == "stuff" || type == "colour" )
	return CONCRETE_ADJ;
      else if ( type == "abstract" )
	return ABSTRACT_ADJ;
      else 
	return BROAD_ADJ;
    }
  }
  else if ( pos == "WW" ) {
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
  SemType type = get_sem_type( lemma, posHead );
  return type;
}

void wordStats::freqLookup(){
  map<string,cf_data>::const_iterator it = settings.freq_lex.find( lowercase(word) );
  if ( it != settings.freq_lex.end() ){
    wfreq = it->second.count;
    lwfreq = log10(wfreq);
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
  else if ( posHead == "BW" &&
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
  basicStats( w, "WORD" ), 
  isPassive(false), isPersRef(false), isPronRef(false),
  archaic(false), isContent(false), isNominal(false),isOnder(false), isImperative(false),
  isBetr(false), isPropNeg(false), isMorphNeg(false), connType(NOCONN),
  f50(false), f65(false), f77(false), f80(false),  compPartCnt(0), wfreq(0),
  argRepeatCnt(0), wordOverlapCnt(0), lemmaRepeatCnt(0), lemmaOverlapCnt(0),
  lwfreq(0),
  polarity(NA), surprisal(NA), prop(JUSTAWORD), sem_type(UNFOUND)
{
  word = UnicodeToUTF8( w->text() );
  charCnt = w->text().length();

  vector<PosAnnotation*> posV = w->select<PosAnnotation>(frog_pos_set);
  if ( posV.size() != 1 )
    throw ValueError( "word doesn't have Frog POS tag info" );
  PosAnnotation *pa = posV[0];
  pos = pa->cls();
  posHead = pa->feat("head");
  lemma = w->lemma( frog_lemma_set );
  prop = checkProps( pa );
  if ( alpDoc ){
    if ( posHead == "WW" ){
      wwform = classifyVerb( w, alpDoc );
      isPassive = ( wwform == "passiefww" );
      if ( prop == ISPVTGW || prop == ISPVVERL ){
	//	cerr << "check IMP voor " << pos << " en lemma " << lemma << endl;
	isImperative = checkImp( w, alpDoc );
	//	cerr << "isImperative =" << isImperative << endl;
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
      vector<Morpheme*> m = ml[q]->select<Morpheme>();
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
    freqLookup();
    if ( settings.doDecompound )
      compPartCnt = runDecompoundWord(word, settings.decompounderPath );
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
       ( ( posHead == "VNW" && prop != ISAANW ) ||
	 ( posHead == "N" ) ||
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
  if ( isContent )
    addOneMetric( doc, el, "content_word", "true" );
  if ( archaic )
    addOneMetric( doc, el, "archaic", "true" );
  if ( isNominal )
    addOneMetric( doc, el, "nominalization", "true" );
  if ( isOnder )
    addOneMetric( doc, el, "subordinate", "true" );
  if ( isBetr )
    addOneMetric( doc, el, "betrekkelijk", "true" );
  if ( isPropNeg )
    addOneMetric( doc, el, "proper_negative", "true" );
  if ( isMorphNeg )
    addOneMetric( doc, el, "morph_negative", "true" );
  if ( connType != NOCONN )
    addOneMetric( doc, el, "connective", toString(connType) );
  if ( polarity != NA  )
    addOneMetric( doc, el, "polarity", toString(polarity) );
  if ( surprisal != NA  )
    addOneMetric( doc, el, "surprisal", toString(surprisal) );
  if ( sem_type != UNFOUND )
    addOneMetric( doc, el, "semtype", toString(sem_type) );
  if ( compPartCnt > 0 )
    addOneMetric( doc, el, "compound_length", toString(compPartCnt) );
  addOneMetric( doc, el, 
		"argument_repeat_count", toString( argRepeatCnt ) );
  addOneMetric( doc, el, 
		"word_overlap_count", toString( wordOverlapCnt ) );
  addOneMetric( doc, el, 
		"lemma_argument_repeat_count", toString( lemmaRepeatCnt ) );
  addOneMetric( doc, el, 
		"lemma_overlap_count", toString( lemmaOverlapCnt ) );

  addOneMetric( doc, el, "average_log_wfreq", toString(lwfreq) );
  if ( !wwform.empty() ){
    KWargs args;
    args["set"] = "tscan-set";
    args["class"] = "wwform(" + wwform + ")";
    el->addPosAnnotation( args );
  }
}

void wordStats::print( ostream& os ) const {
  os << "word: [" <<word << "], lengte=" << charCnt 
     << ", morphemen: " << morphemes << ", HEAD: " << posHead;
  switch ( prop ){
  case ISINF:
    os << " (Infinitief)";
    break;
  case ISVD:
    os << " (Voltooid deelwoord)";
    break;
  case ISOD:
    os << " (Onvoltooid Deelwoord)";
    break;
  case ISPVTGW:
    os << " (Tegenwoordige tijd)";
    break;
  case ISPVVERL:
    os << " (Verleden tijd)";
    break;
  case ISPPRON1:
    os << " (1-ste persoon)";
    break;
  case ISPPRON2:
    os << " (2-de persoon)";
    break;
  case ISPPRON3:
    os << " (3-de persoon)";
    break;
  case ISAANW:
    os << " (aanwijzend)";
    break;
  case ISNAME:
    os << " (Name)";
    break;
  case ISPUNCT:
  case JUSTAWORD:
    break;
  }
  if ( !wwform.empty() )
    os << " (" << wwform << ")";  
  if ( isNominal )
    os << " nominalisatie";  
  if ( isOnder )
    os << " ondergeschikt";  
  if ( isBetr )
    os << " betrekkelijk";  
  if ( isPropNeg || isMorphNeg )
    os << " negatief";  
  if ( polarity != NA ){
    os << ", Polariteit=" << polarity << endl;
  }
}
 
void wordHeader( ostream& os ){
  os << "lpw,wpl,lpwzn,wplzn,mpw,wpm,mpwzn,wpmzn,"
     << "sdpw,sdens,freq50,freq65,freq77,freq80,"
     << "wfreq_log,wfreq_log_zn,"
     << "lemfreq_log,lemfreq_log_zn,";
}

void sentHeader( ostream& os ){
  os << "wpz,pzw,wnp,subord_clause,rel_clause,clauses_d,clauses_g,"
     << "dlevel,dlevel_gt4_prop,dlevel_gt4_r,"
     << "nom,lv_d,lv_g,prop_negs,morph_negs,total_negs,multiple_negs,"
     << "deplen_subverb,deplen_dirobverb,deplen_indirobverb,deplen_verbpp,"
     << "deplen_noundet,deplen_prepobj,deplen_verbvc,"
     << "deplen_compbody,deplen_crdcnj,deplen_verbcp,deplen_noun_vc,"
     << "deplen,deplen_max,";
}

void infoHeader( ostream& os ){
  os << "word_ttr,lemma_ttr,content_words_r,content_words_d,content_words_g,"
     << "rar_index,vc_mods_d,vc_mods_g,np_mods_d,np_mods_g,np_dens,conjuncts,";
}
 
void coherenceHeader( ostream& os ){
  os << "temporals,reeks,contrast,comparatief,causal,referential_prons,"
     << "argument_overlap_d,argument_overlap_g,lem_argument_overlap_d,"
     << "lem_argument_overlap_g,wordbuffer_argument_overlap_g,"
     << "wordbuffer_argument_overlap_d,lemmabuffer_argument_overlap_d,"
     << "lemmabuffer_argument_overlap_g,indef_nps_p,indef_nps_r,indef_nps_g,";
}
 
void concreetHeader( ostream& os ){
  os << "noun_conc_strict_p,noun_conc_strict_r,noun_conc_strict_d,";
  os << "noun_conc_broad_p,noun_conc_broad_r,noun_conc_broad_d,";
  os << "adj_conc_strict_p,adj_conc_strict_r,adj_conc_strict_d,";
  os << "adj_conc_broad_p,adj_conc_broad_r,adj_conc_broad_d,";
}

void persoonlijkheidHeader( ostream& os ){
  os << "pers_ref,pers_pron_1,pers_pron_2,pers_pron3,pers_pron";
}

void wordStats::CSVheader( ostream& os ){
  os << "file,foliaID,woord,";
  wordHeader( os );
  coherenceHeader( os );
  concreetHeader( os );
  persoonlijkheidHeader( os );
  os << endl;
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
  os << lwfreq << ",";
  if ( prop == ISNAME )
    os << "NA" << ",";
  else
    os << lwfreq << ",";
  os << "not implemented,not implemented,";
}

void wordStats::informationDensityToCSV( ostream& ) const {
}

void wordStats::coherenceToCSV( ostream& os ) const {
  os << (connType==TEMPOREEL?1:0) << ","
     << (connType==REEKS?1:0) << ","
     << (connType==CONTRASTIEF) << ","
     << (connType==COMPARATIEF) << ","
     << (connType==CAUSAAL) << ","
     << isPronRef << ","
     << "NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,";
}

void wordStats::concreetToCSV( ostream& os ) const {
  if ( sem_type == CONCRETE_HUMAN || sem_type == CONCRETE_NOUN ){
    os << "1,1,1,0,0,0,0,0,0,0,0,0,";
  }
  else if ( sem_type == BROAD_NOUN ){
    os << "0,0,0,1,1,1,0,0,0,0,0,0,";
  }
  else if ( sem_type  == CONCRETE_ADJ ){
    os << "0,0,0,0,0,0,1,1,1,0,0,0,";
  }
  else if ( sem_type == BROAD_ADJ ){
    os << "0,0,0,0,0,0,0,0,0,1,1,1,";
  }
  else {
    os << "0,0,0,0,0,0,0,0,0,0,0,0,";
  } 
}

void wordStats::persoonlijkheidToCSV( ostream& os ) const {
  os << isPersRef << ","
     << (prop == ISPPRON1 ) << ","
     << (prop == ISPPRON2 ) << ","
     << (prop == ISPPRON3 ) << ","
     << (prop == ISPPRON1 || prop == ISPPRON2 || prop == ISPPRON3) << ",";

}

void wordStats::toCSV( ostream& os ) const {
  if ( word == "," )
    os << "&komma;";
  else
    os << word;
  os << ",";
  wordDifficultiesToCSV( os );
  coherenceToCSV( os );
  concreetToCSV( os );
  persoonlijkheidToCSV( os );
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
    infCnt(0), presentCnt(0), pastCnt(0),
    nameCnt(0),
    pron1Cnt(0), pron2Cnt(0), pron3Cnt(0), 
    passiveCnt(0),
    persRefCnt(0), pronRefCnt(0), 
    archaicsCnt(0),
    contentCnt(0),
    nominalCnt(0),
    nounCnt(0),
    adjCnt(0),
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
    wfreq(0),
    wfreq_n(0),
    lwfreq(NA),
    lwfreq_n(NA),
    polarity(NA),
    surprisal(NA),
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
  virtual void print(  ostream& ) const;
  void addMetrics( ) const;
  void wordDifficultiesToCSV( ostream& ) const;
  void sentDifficultiesToCSV( ostream& ) const;
  void informationDensityToCSV( ostream& ) const;
  void coherenceToCSV( ostream& ) const;
  void concreetToCSV( ostream& ) const;
  void persoonlijkheidToCSV( ostream& ) const;
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
  int nameCnt;
  int pron1Cnt;
  int pron2Cnt;
  int pron3Cnt;
  int passiveCnt;
  int persRefCnt;
  int pronRefCnt;
  int archaicsCnt;
  int contentCnt;
  int nominalCnt;
  int nounCnt;
  int adjCnt;
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
  int wfreq; 
  int wfreq_n; 
  double lwfreq; 
  double lwfreq_n;
  double polarity;
  double surprisal;
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
  map<string,int> heads;
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
  archaicsCnt += ss->archaicsCnt;
  contentCnt += ss->contentCnt;
  nominalCnt += ss->nominalCnt;
  nounCnt += ss->nounCnt;
  adjCnt += ss->adjCnt;
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
  wfreq += ss->wfreq;
  wfreq_n += ss->wfreq_n;
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
  presentCnt += ss->presentCnt;
  pastCnt += ss->pastCnt;
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

void structStats::print( ostream& os ) const {
  os << category << " " << id << endl;
  os << category << " POS tags: ";
  for ( map<string,int>::const_iterator it = heads.begin();
	it != heads.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "#Voltooid deelwoorden " << vdCnt 
     << " gemiddeld: " << vdCnt/double(wordCnt) << endl;
  os << "#Onvoltooid deelwoorden " << odCnt 
     << " gemiddeld: " << odCnt/double(wordCnt) << endl;
  os << "#Infinitieven " << infCnt 
     << " gemiddeld: " << infCnt/double(wordCnt) << endl;
  os << "#Archaische constructies " << archaicsCnt
     << " gemiddeld: " << archaicsCnt/double(wordCnt) << endl;
  os << "#Content woorden " << contentCnt
     << " gemiddeld: " << contentCnt/double(wordCnt) << endl;
  os << "#NP's " << npCnt << " met in totaal " << npSize
     << " woorden, gemiddeld: " << npSize/npCnt << endl;
  os << "D LEVEL=" << dLevel << endl;
  if ( polarity != NA ){
    os << "#Polariteit:" << polarity
       << " gemiddeld: " << polarity/double(wordCnt) << endl;
  }
  else {
    os << "#Polariteit: NA" << endl;
  }
  os << "#Nominalizaties " << nominalCnt
     << " gemiddeld: " << nominalCnt/double(wordCnt) << endl;
  os << "#Persoonsvorm (TW) " << presentCnt
     << " gemiddeld: " << presentCnt/double(wordCnt) << endl;
  os << "#Persoonsvorm (VERL) " << pastCnt
     << " gemiddeld: " << pastCnt/double(wordCnt) << endl;
  os << "#Clauses " << pastCnt+presentCnt
     << " gemiddeld: " << (pastCnt+presentCnt)/double(wordCnt) << endl;  
  os << "#passief constructies " << passiveCnt 
     << " gemiddeld: " << passiveCnt/double(wordCnt) << endl;
  os << "#PersoonLijke Voornaamwoorden terugverwijzend " << pronRefCnt
     << " gemiddeld: " << pronRefCnt/double(wordCnt) << endl;
  os << "#PersoonLijke Voornaamwoorden 1-ste persoon " << pron1Cnt
     << " gemiddeld: " << pron1Cnt/double(wordCnt) << endl;
  os << "#PersoonLijke Voornaamwoorden 2-de persoon " << pron2Cnt
     << " gemiddeld: " << pron2Cnt/double(wordCnt) << endl;
  os << "#PersoonLijke Voornaamwoorden 3-de persoon " << pron3Cnt
     << " gemiddeld: " << pron3Cnt/double(wordCnt) << endl;
  os << "#PersoonLijke Voornaamwoorden totaal " 
     << pron1Cnt + pron2Cnt + pron3Cnt
     << " gemiddeld: " << (pron1Cnt + pron2Cnt + pron3Cnt)/double(wordCnt) << endl;
  os << category << " gemiddelde woordlengte " << charCnt/double(wordCnt) << endl;
  os << category << " gemiddelde woordlengte zonder Namen " << charCntExNames/double(wordCnt - nameCnt) << endl;
  os << category << " gemiddelde aantal morphemen " << morphCnt/double(wordCnt) << endl;
  os << category << " gemiddelde aantal morphemen zonder Namen " << morphCntExNames/double(wordCnt-nameCnt) << endl;
  os << category << " aantal ondergeschikte bijzinnen " << onderCnt
     << " dichtheid " << onderCnt/double(wordCnt) << endl;
  os << category << " aantal betrekkelijke bijzinnen " << betrCnt
     << " dichtheid " << betrCnt/double(wordCnt) << endl;
  os << category << " aantal Namen " << nameCnt << endl;
  os << category << " Named Entities ";
  for ( map<NerProp,int>::const_iterator it = ners.begin();
	it != ners.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << category << " aantal woorden " << wordCnt << endl;
  if ( sv.size() > 0 ){
    os << category << " bevat " << sv.size() << " " 
       << sv[0]->category << " onderdelen." << endl << endl;
    for ( size_t i=0;  i < sv.size(); ++ i ){
      os << *sv[i] << endl;
    }
  }
}

void structStats::addMetrics( ) const {
  FoliaElement *el = folia_node;
  Document *doc = el->doc();
  addOneMetric( doc, el, "word_count", toString(wordCnt) );
  addOneMetric( doc, el, "name_count", toString(nameCnt) );
  addOneMetric( doc, el, "vd_count", toString(vdCnt) );
  addOneMetric( doc, el, "od_count", toString(odCnt) );
  addOneMetric( doc, el, "inf_count", toString(infCnt) );
  addOneMetric( doc, el, "archaic_count", toString(archaicsCnt) );
  addOneMetric( doc, el, "content_count", toString(contentCnt) );
  addOneMetric( doc, el, "nominal_count", toString(nominalCnt) );
  addOneMetric( doc, el, "noun_count", toString(nounCnt) );
  addOneMetric( doc, el, "adj_count", toString(adjCnt) );
  addOneMetric( doc, el, "subord_count", toString(onderCnt) );
  addOneMetric( doc, el, "rel_count", toString(betrCnt) );
  addOneMetric( doc, el, "cnj_count", toString(cnjCnt) );
  addOneMetric( doc, el, "crd_count", toString(crdCnt) );
  addOneMetric( doc, el, "passive_count", toString(passiveCnt) );
  addOneMetric( doc, el, "temporals_count", toString(tempConnCnt) );
  addOneMetric( doc, el, "reeks_count", toString(reeksCnt) );
  addOneMetric( doc, el, "reeks_connector_count", toString(reeksConnCnt) );
  addOneMetric( doc, el, "contrast_count", toString(contConnCnt) );
  addOneMetric( doc, el, "comparatief_count", toString(compConnCnt) );
  addOneMetric( doc, el, "causaal_count", toString(causeConnCnt) );
  if ( polarity != NA )
    addOneMetric( doc, el, "polarity", toString(polarity) );
  if ( surprisal != NA )
    addOneMetric( doc, el, "surprisal", toString(surprisal) );
  addOneMetric( doc, el, "prop_neg_count", toString(propNegCnt) );
  addOneMetric( doc, el, "morph_neg_count", toString(morphNegCnt) );
  addOneMetric( doc, el, "compound_count", toString(compCnt) );
  addOneMetric( doc, el, "compound_length", toString(compPartCnt) );
  addOneMetric( doc, el, 
		"argument_repeat_count", toString( argRepeatCnt ) );
  addOneMetric( doc, el, 
		"word_overlap_count", toString( wordOverlapCnt ) );
  addOneMetric( doc, el, 
		"lemma_argument_repeat_count", toString( lemmaRepeatCnt ) );
  addOneMetric( doc, el, 
		"lemma_overlap_count", toString( lemmaOverlapCnt ) );
  if ( lwfreq != NA  )
    addOneMetric( doc, el, "average_log_wfreq", toString(lwfreq) );
  if ( lwfreq_n != NA  )
    addOneMetric( doc, el, "average_log_wfreq_min_names", toString(lwfreq_n) );
  addOneMetric( doc, el, "freq50", toString(f50Cnt) );
  addOneMetric( doc, el, "freq65", toString(f65Cnt) );
  addOneMetric( doc, el, "freq77", toString(f77Cnt) );
  addOneMetric( doc, el, "freq80", toString(f80Cnt) );
  addOneMetric( doc, el, "present_verb_count", toString(presentCnt) );
  addOneMetric( doc, el, "past_verb_count", toString(pastCnt) );
  addOneMetric( doc, el, "referential_pron_count", toString(pronRefCnt) );
  addOneMetric( doc, el, "pers_pron_1_count", toString(pron1Cnt) );
  addOneMetric( doc, el, "pers_pron_2_count", toString(pron2Cnt) );
  addOneMetric( doc, el, "pres_pron_3_count", toString(pron3Cnt) );
  addOneMetric( doc, el, "character_count", toString(charCnt) );
  addOneMetric( doc, el, "character_count_min_names", toString(charCntExNames) );
  addOneMetric( doc, el, "morpheme_count", toString(morphCnt) );
  addOneMetric( doc, el, "morpheme_count_min_names", toString(morphCntExNames) );
  addOneMetric( doc, el, "concrete_strict_noun", toString(strictConcreteNounCnt) );
  addOneMetric( doc, el, "concrete_broad_noun", toString(broadConcreteNounCnt) );
  addOneMetric( doc, el, "abstract_strict_noun", toString(strictAbstractNounCnt) );
  addOneMetric( doc, el, "abstract_broad_noun", toString(broadAbstractNounCnt) );
  addOneMetric( doc, el, "concrete_strict_adj", toString(strictConcreteAdjCnt) );
  addOneMetric( doc, el, "concrete_broad_adj", toString(broadConcreteAdjCnt) );
  addOneMetric( doc, el, "abstract_strict_adj", toString(strictAbstractAdjCnt) );
  addOneMetric( doc, el, "abstract_broad_adj", toString(broadAbstractAdjCnt) );
  addOneMetric( doc, el, "state_count", toString(stateCnt) );
  addOneMetric( doc, el, "action_count", toString(actionCnt) );
  addOneMetric( doc, el, "process_count", toString(processCnt) );
  addOneMetric( doc, el, "weird_count", toString(weirdCnt) );
  addOneMetric( doc, el, "human_nouns_count", toString(humanCnt) );
  addOneMetric( doc, el, "indef_np_count", toString(indefNpCnt) );
  addOneMetric( doc, el, "np_count", toString(npCnt) );
  addOneMetric( doc, el, "np_size", toString(npSize) );
  if ( dLevel >= 0 )
    addOneMetric( doc, el, "d_level", toString(dLevel) );
  else
    addOneMetric( doc, el, "d_level", "missing" );
  if ( questCnt > 0 )
    addOneMetric( doc, el, "question_count", toString(questCnt) );
  if ( impCnt > 0 )
    addOneMetric( doc, el, "imperative_count", toString(impCnt) );
  /*
  os << category << " Named Entities ";
  for ( map<NerProp,int>::const_iterator it = ners.begin();
	it != ners.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  */
  for ( size_t i=0; i < sv.size(); ++i ){
    sv[i]->addMetrics();
  }
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
  if ( lwfreq == NA )
    os << "NA,";
  else
    os << lwfreq/double(wordCnt) << ",";
  if ( wordCnt == nameCnt || lwfreq == NA )
    os << "NA" << ",";
  else
    os << lwfreq_n/double(wordCnt-nameCnt) << ",";
  os << "not implemented,not implemented,";
}

ostream& displayMM( ostream&os, const multimap<DD_type, int>& mm, DD_type t ){
  size_t len = mm.count(t);
  if ( len > 0 ){
    int result = 0;
    for( multimap<DD_type, int>::const_iterator pos = mm.lower_bound(t); 
	 pos != mm.upper_bound(t); 
	 ++pos ){
      result += pos->second;
    }
    os << result/double(len);
  }
  else
    os << "NA";
  return os;
}

ostream& displayTotalMM( ostream&os, const multimap<DD_type, int>& mm ){
  size_t len = mm.size();
  if ( len > 0 ){
    int result = 0;
    for( multimap<DD_type, int>::const_iterator pos = mm.begin(); 
	 pos != mm.end(); 
	 ++pos ){
      result += pos->second;
    }
    os << result/double(len);
  }
  else
    os << "NA";
  return os;
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

void structStats::sentDifficultiesToCSV( ostream& os ) const {
  os << wordCnt/double(sentCnt) << ","
     << double(sentCnt)/wordCnt * 100.0 << ","
     << wordCnt/double(npCnt) << ","
     << onderCnt/double(wordCnt) * 1000 << ","
     << betrCnt/double(wordCnt) * 1000 << ","
     << (pastCnt + presentCnt)/double(wordCnt) * 1000 << ","
     << (pastCnt + presentCnt)/double(sentCnt) << ","
     << dLevel/double(sentCnt) << "," 
     << dLevel_gt4/double(sentCnt) << ","
     << dLevel_gt4/double(sentCnt - dLevel_gt4) << ","
     << nominalCnt/double(wordCnt) * 1000 << ","
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
  displayMM( os, distances, SUB_VERB ) << ",";
  displayMM( os, distances, OBJ1_VERB ) << ",";
  displayMM( os, distances, OBJ2_VERB ) << ",";
  displayMM( os, distances, VERB_PP ) << ",";
  displayMM( os, distances, NOUN_DET ) << ",";
  displayMM( os, distances, PREP_OBJ1 ) << ",";
  displayMM( os, distances, VERB_VC ) << ",";
  displayMM( os, distances, COMP_BODY ) << ",";
  displayMM( os, distances, CRD_CNJ ) << ",";
  displayMM( os, distances, VERB_COMP ) << ",";
  displayMM( os, distances, NOUN_VC ) << ",";
  displayTotalMM( os, distances ) << ",";
  os << getHighest( distances )/sentCnt << ",";
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
  os << indefNpCnt/double(npCnt) << ","
     << indefNpCnt/double(npCnt - indefNpCnt) << ","
     << (indefNpCnt/double(wordCnt)) * 1000 << ",";
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

void structStats::persoonlijkheidToCSV( ostream& os ) const {
  os << (persRefCnt/double(wordCnt)) * 1000 << ",";
  os << (pron1Cnt/double(wordCnt)) * 1000 << ",";
  os << (pron2Cnt/double(wordCnt)) * 1000 << ",";
  os << (pron3Cnt/double(wordCnt)) * 1000 << ",";
  os << (pronRefCnt/double(wordCnt)) * 1000 << ",";
}

struct sentStats : public structStats {
  sentStats( Sentence *, Sentence *, xmlDoc * );
  void print( ostream& ) const;
  bool isSentence() const { return true; };
  static void CSVheader( ostream& os );
  void toCSV( ostream& ) const;
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

sentStats::sentStats( Sentence *s, Sentence *prev, xmlDoc *alpDoc ): 
  structStats( s, "ZIN" ){
  sentCnt = 1;
  id = s->id();
  text = UnicodeToUTF8( s->toktext() );
  vector<Word*> w = s->words();
  vector<double> surprisalV(w.size(),NA);
  if ( settings.doSurprisal ){
    surprisalV = runSurprisal( s, settings.surprisalPath );
    if ( surprisalV.size() != w.size() ){
      cerr << "MISMATCH! " << surprisalV.size() << " != " <<  w.size()<< endl;
      surprisalV.clear();
    }
  }
  set<size_t> puncts;
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
  }
  //  cerr << "PUNCTS " << puncts << endl;
  bool question = false;
  for ( size_t i=0; i < w.size(); ++i ){
    wordStats *ws = new wordStats( w[i], alpDoc, puncts );
    ws->surprisal = surprisalV[i];
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
      heads[ws->posHead]++;
      
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
      wfreq += ws->wfreq;
      if ( ws->prop != ISNAME )
	wfreq_n += ws->wfreq;
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
      case ISPPRON1:
	pron1Cnt++;
      case ISPPRON2:
	pron2Cnt++;
      case ISPPRON3:
	pron3Cnt++;
      default:
	;// ignore JUSTAWORD and ISAANW
      }
      if ( ws->isPassive )
	passiveCnt++;
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
      if ( ws->posHead == "N" )
	nounCnt++;
      else if ( ws->posHead == "ADJ" )
	adjCnt++;
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
      default:
	;
      }
      sv.push_back( ws );
    }
  }
  if ( question )
    questCnt = 1;
  if ( (morphNegCnt + propNegCnt) > 1 )
    multiNegCnt = 1;
  resolveConnectives();
  if ( wfreq == 0 )
    lwfreq = NA;
  else
    lwfreq = log10( wfreq / w.size() );
  if ( wordCnt == nameCnt || wfreq_n == 0 )
    lwfreq_n = NA;
  else
    lwfreq_n = log10( wfreq_n / (wordCnt-nameCnt) );
  np_length( s, npCnt, indefNpCnt, npSize );
  if ( alpDoc ){
    dLevel = get_d_level( s, alpDoc );
    if ( dLevel > 4 )
      dLevel_gt4 = 1;
    countCrdCnj( alpDoc, crdCnt, cnjCnt, reeksCnt );
    int np2Cnt;
    mod_stats( alpDoc, vcModCnt, np2Cnt, npModCnt );
  }
}

void sentStats::print( ostream& os ) const {
  os << category << "[" << text << "]" << endl;
  os << category << " er zijn " << propNegCnt << " negatieve woorden en " 
     << morphNegCnt << " negatieve morphemen gevonden";
  if ( multiNegCnt > 1 )
    os << " er is dus een dubbele ontkenning! ";
  os << endl;
  structStats::print( os );
}

void sentStats::CSVheader( ostream& os ){
  os << "file,foliaID,";
  wordHeader( os );
  sentHeader( os );
  infoHeader( os );
  coherenceHeader( os );
  concreetHeader( os );
  persoonlijkheidHeader( os );
  os << endl;
}

void sentStats::toCSV( ostream& os ) const {
  wordDifficultiesToCSV( os );
  sentDifficultiesToCSV( os );
  informationDensityToCSV( os );
  coherenceToCSV( os );
  concreetToCSV( os );
  persoonlijkheidToCSV( os );
  os << endl;
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
  void print( ostream& ) const;
  static void CSVheader( ostream& os );
  void toCSV( ostream& ) const;
  void addMetrics( ) const;
};

xmlDoc *AlpinoServerParse( Sentence *);

parStats::parStats( Paragraph *p ): 
  structStats( p, "PARAGRAAF" )
{
  sentCnt = 0;
  id = p->id();
  vector<Sentence*> sents = p->sentences();
  for ( size_t i=0; i < sents.size(); ++i ){
    xmlDoc *alpDoc = 0;
    if ( settings.doAlpinoServer ){
      cerr << "calling Alpino Server" << endl;
      alpDoc = AlpinoServerParse( sents[i] );
      if ( !alpDoc ){
	cerr << "alpino parser failed!" << endl;
      }
    }
    else if ( settings.doAlpino ){
      cerr << "calling Alpino parser" << endl;
      alpDoc = AlpinoParse( sents[i] );
      if ( !alpDoc ){
	cerr << "alpino parser failed!" << endl;
      }
    }
    sentStats *ss;
    if ( i == 0 )
      ss = new sentStats( sents[i], 0, alpDoc );
    else
      ss = new sentStats( sents[i], sents[i-1], alpDoc );
    xmlFreeDoc( alpDoc );
    merge( ss );
    sentCnt++;
  }
  if ( wfreq == 0 )
    lwfreq = NA;
  else
    lwfreq = log10( wfreq / sents.size() );
  if ( wordCnt == nameCnt || wfreq_n == 0 )
    lwfreq_n = NA;
  else
    lwfreq_n = log10( wfreq_n / (wordCnt-nameCnt) );
}

void parStats::print( ostream& os ) const {
  os << category << " with " << sentCnt << " Sentences" << endl;
  structStats::print( os );
}

void parStats::CSVheader( ostream& os ){
  os << "file,foliaID,";
  wordHeader( os );
  sentHeader( os );
  infoHeader( os );
  coherenceHeader( os );
  concreetHeader( os );
  persoonlijkheidHeader( os );
  os << endl;
}

void parStats::toCSV( ostream& os ) const {
  wordDifficultiesToCSV( os );
  sentDifficultiesToCSV( os );
  informationDensityToCSV( os );
  coherenceToCSV( os );
  concreetToCSV( os );
  persoonlijkheidToCSV( os );
  os << endl;
}

void parStats::addMetrics( ) const {
  FoliaElement *el = folia_node;
  structStats::addMetrics( );
  addOneMetric( el->doc(), el, 
		"sentence_count", toString(sentCnt) );
}

struct docStats : public structStats {
  docStats( Document * );
  void print( ostream& ) const;
  bool isDocument() const { return true; };
  void toCSV( const string&, csvKind ) const;
  static void CSVheader( ostream& os );
  void toCSV( ostream& ) const;
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
    sentCnt += ps->sentCnt;
  }
  if ( wfreq == 0 )
    lwfreq = NA;
  else
    lwfreq = log10( wfreq / pars.size() );
  if ( wordCnt == nameCnt || wfreq_n == 0 )
    lwfreq_n = NA;
  else
    lwfreq_n = log10( wfreq_n / (wordCnt-nameCnt) );
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

void docStats::print( ostream& os ) const {
  os << category << " with "  << sv.size() << " paragraphs and "
     << sentCnt << " Sentences" << endl;
  os << "TTW = " << unique_words.size()/double(wordCnt) << endl;
  os << "TTL = " << unique_lemmas.size()/double(wordCnt) << endl;
  os << "rarity(" << settings.rarityLevel << ")=" 
     << rarity( settings.rarityLevel ) << endl;
  structStats::print( os );
}

void docStats::toCSV( const string& name, 
		      csvKind what ) const {
  if ( what == DOC_CSV ){
    string fname = name + ".document.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      docStats::CSVheader( out );
      out << name << ",";
      toCSV( out );
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
      parStats::CSVheader( out );
      for ( size_t par=0; par < sv.size(); ++par ){
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
      sentStats::CSVheader( out );
      for ( size_t par=0; par < sv.size(); ++par ){
	for ( size_t sent=0; sent < sv[par]->sv.size(); ++sent ){
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
      wordStats::CSVheader( out );
      for ( size_t par=0; par < sv.size(); ++par ){
	for ( size_t sent=0; sent < sv[par]->sv.size(); ++sent ){
	  for ( size_t word=0; word < sv[par]->sv[sent]->sv.size(); ++word ){
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

void docStats::CSVheader( ostream& os ){
  os << "file,";
  wordHeader(os);
  sentHeader(os);
  infoHeader( os );
  coherenceHeader( os );
  concreetHeader( os );
  persoonlijkheidHeader( os );
  os << endl;
}

void docStats::toCSV( ostream& os ) const {
  wordDifficultiesToCSV( os );
  sentDifficultiesToCSV( os );
  informationDensityToCSV( os );
  coherenceToCSV( os );
  concreetToCSV( os );
  persoonlijkheidToCSV( os );
  os << endl;
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
    if ( s == "READY" )
      break;
    result += s + "\n";
  }
  DBG << "received data [" << result << "]" << endl;
  xmlDoc *doc = xmlReadMemory( result.c_str(), result.length(), 
			       0, 0, XML_PARSE_NOBLANKS );
  return doc;
}

int main(int argc, char *argv[]) {
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
      analyse.toCSV( inName, DOC_CSV );
      analyse.toCSV( inName, PAR_CSV );
      analyse.toCSV( inName, SENT_CSV );
      analyse.toCSV( inName, WORD_CSV );
      delete doc;
      LOG << "saved output in " << outName << endl;
      // cerr << analyse << endl;
    }
  }
  
  exit(EXIT_SUCCESS);
}

