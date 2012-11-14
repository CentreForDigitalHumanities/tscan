/*
  $Id$
  $URL$

  Copyright (c) 1998 - 2012
 
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

#include <csignal>
#include <cerrno>
#include <string>
#include <algorithm>
#include <cstdio> // for remove()
#include "config.h"
#include "timblserver/TimblServerAPI.h"
#include "libfolia/folia.h"
#include "libfolia/document.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/LogStream.h"
#include "tscan/Configuration.h"
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

string configFile = "tscan.cfg";
Configuration config;

struct cf_data {
  long int count;
  double freq;
};

struct settingData {
  void init( const Configuration& );
  bool doAlpino;
  bool doDecompound;
  bool doSurprisal;
  string decompounderPath;
  string surprisalPath;
  string style;
  int rarityLevel;
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
  string val = cf.lookUp( "useAlpino" );
  doAlpino = false;
  if( !Timbl::stringTo( val, doAlpino ) ){
    cerr << "invalid value for 'useAlpino' in config file" << endl;
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
    rarityLevel = 5;
  }
  else if ( !Timbl::stringTo( val, rarityLevel ) ){ 
    cerr << "invalid value for 'rarityLevel' in config file" << endl;
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

enum WordProp { ISNAME, ISPUNCT, 
		ISVD, ISOD, ISINF, ISPVTGW, ISPVVERL,
		ISPPRON1, ISPPRON2, ISPPRON3,
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

enum SemType { UNFOUND, CONCRETE, CONCRETE_HUMAN, ABSTRACT, BROAD, 
	       STATE, ACTION, PROCESS, WEIRD };

struct basicStats {
  basicStats( const string& cat ): category( cat ), 
				   wordLen(0),wordLenExNames(0),
				   morphLen(0), morphLenExNames(0) {};
  virtual ~basicStats(){};
  virtual void print( ostream& ) const = 0;
  virtual void addMetrics( FoliaElement *el ) const = 0;
  virtual string text() const { return ""; };
  virtual ConnType getConnType() const { return NOCONN; };
  string category;
  int wordLen;
  int wordLenExNames;
  int morphLen;
  int morphLenExNames;
};

ostream& operator<<( ostream& os, const basicStats& b ){
  b.print(os);
  return os;
}

void basicStats::print( ostream& os ) const {
  os << "BASICSTATS PRINT" << endl;
  os << category << endl;
  os << " lengte=" << wordLen << ", aantal morphemen=" << morphLen << endl;
  os << " lengte zonder namem =" << wordLenExNames << ", aantal morphemen zonder namen=" << morphLenExNames << endl;
}

struct wordStats : public basicStats {
  wordStats( Word *, xmlDoc *, double, 
	     const vector<string>&,  const vector<string>& );
  void print( ostream& ) const;
  string text() const { return word; };
  ConnType getConnType() const { return connType; };
  void addMetrics( FoliaElement * ) const;
  bool checkContent() const;
  ConnType checkConnective( ) const;
  bool checkNominal( Word *, xmlDoc * ) const;
  WordProp checkProps( const PosAnnotation* );
  double checkPolarity( ) const;
  SemType checkSemProps( ) const;
  bool checkPropNeg() const;
  bool checkMorphNeg() const;
  void freqLookup();
  string word;
  string pos;
  string posHead;
  string lemma;
  string wwform;
  bool isPassive;
  bool isPronRef;
  bool archaic;
  bool isContent;
  bool isNominal;
  bool isOnder;
  bool isBetr;
  bool isPropNeg;
  bool isMorphNeg;
  ConnType connType;
  bool f50;
  bool f65;
  bool f77;
  bool f80;
  int compLen;
  int wfreq;
  int wordRepeatCnt;
  int wordOverlapCnt;
  int lemmaRepeatCnt;
  int lemmaOverlapCnt;
  double lwfreq;
  double polarity;
  double surprisal;
  WordProp prop;
  SemType sem_type;
  vector<string> morphemes;
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
  if ( tail.size() > word.size() )
    return false;
  string::const_reverse_iterator wir = word.rbegin();
  string::const_reverse_iterator tir = tail.rbegin();
  while ( tir != tail.rend() ){
    if ( *tir != *wir )
      return false;
    ++tir;
    ++wir;
  }
  return false;
}

bool wordStats::checkNominal( Word *w, xmlDoc *alpDoc ) const {
  static string morphList[] = { "ing", "sel", "nis", "enis", "heid", "te", 
				"schap", "dom", "sie", "iek", "iteit", "age",
				"esse",	"name" };
  static set<string> morphs( morphList, 
			     morphList + sizeof(morphList)/sizeof(string) );
  if ( posHead == "N" && morphemes.size() > 1 
       && morphs.find( morphemes[morphemes.size()-1] ) != morphs.end() ){
    // morphemes.size() > 1 check mijdt false hits voor "dom", "schap".
    return true;
  }
  bool matched = match_tail( word, "ose" ) ||
    match_tail( word, "ase" ) ||
    match_tail( word, "ese" ) ||
    match_tail( word, "isme" ) ||
    match_tail( word, "sie" ) ||
    match_tail( word, "tie" );
  if ( matched )
    return true;
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
      else if ( vwtype == "aanw" )
	isPronRef = true;
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
	return CONCRETE;
      else if ( type == "dynamic" || type == "nondynamic" )
	return ABSTRACT;
      else 
	return BROAD;
    }
  }
  else if ( pos == "ADJ" ) {
    map<string,string>::const_iterator it = settings.adj_sem.find( lemma );
    if ( it != settings.adj_sem.end() ){
      string type = it->second;
      if ( type == "phyper" || type == "stuff" || type == "colour" )
	return CONCRETE;
      else if ( type == "abstract" )
	return ABSTRACT;
      else 
	return BROAD;
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
    lwfreq = log(wfreq);
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


wordStats::wordStats( Word *w, xmlDoc *alpDoc, double surp,
 		      const vector<string>& wordbuffer,
		      const vector<string>& lemmabuffer ):
  basicStats( "WORD" ), 
  isPassive(false), isPronRef(false),
  archaic(false), isContent(false), isNominal(false), isOnder(false), 
  isBetr(false), isPropNeg(false), isMorphNeg(false), connType(NOCONN),
  f50(false), f65(false), f77(false), f80(false),  compLen(0), wfreq(0), lwfreq(0), wordRepeatCnt(0), wordOverlapCnt(0), lemmaRepeatCnt(0), lemmaOverlapCnt(0),
  polarity(NA), surprisal(surp), prop(JUSTAWORD), sem_type(UNFOUND)
{
  word = UnicodeToUTF8( w->text() );
  wordLen = w->text().length();

  vector<PosAnnotation*> posV = w->select<PosAnnotation>(frog_pos_set);
  if ( posV.size() != 1 )
    throw ValueError( "word doesn't have Frog POS tag info" );
  PosAnnotation *pa = posV[0];
  pos = pa->cls();
  posHead = pa->feat("head");
  lemma = w->lemma( frog_lemma_set );
  prop = checkProps( pa );
  if ( ( posHead == "VNW" && prop != isPronRef ) ||
       ( posHead == "N" ) ||
       ( pos == "SPEC(deeleigen)" ) ){
    argument_overlap( lowercase(word), wordbuffer, wordRepeatCnt, wordOverlapCnt );
    argument_overlap( lemma, lemmabuffer, lemmaRepeatCnt, lemmaOverlapCnt );
  }
  if ( posHead == "WW" ){
    if ( alpDoc ){
      wwform = classifyVerb( w, alpDoc );
      isPassive = ( wwform == "passiefww" );
    }
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
    morphLen = max;
    if ( prop != ISNAME ){
      wordLenExNames = wordLen;
      morphLenExNames = max;
    }
    if (alpDoc) isNominal = checkNominal( w, alpDoc );
    polarity = checkPolarity();
    sem_type = checkSemProps();
    freqLookup();
    if ( settings.doDecompound )
      compLen = runDecompoundWord(word, settings.decompounderPath );
  }
  addMetrics( w );
}

void addOneMetric( Document *doc, FoliaElement *parent,
		   const string& cls, const string& val ){
  MetricAnnotation *m = new MetricAnnotation( doc,
					      "class='" + cls + "', value='" 
					      + val + "'" );
  parent->append( m );
}

void wordStats::addMetrics( FoliaElement *el ) const {
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
  if ( compLen > 0 )
    addOneMetric( doc, el, "compound_len", toString(compLen) );
  addOneMetric( doc, el, 
		"word_repeat_count", toString( wordRepeatCnt ) );
  addOneMetric( doc, el, 
		"word_overlap_count", toString( wordOverlapCnt ) );
  addOneMetric( doc, el, 
		"lemma_repeat_count", toString( lemmaRepeatCnt ) );
  addOneMetric( doc, el, 
		"lemma_overlap_count", toString( lemmaOverlapCnt ) );

  addOneMetric( doc, el, "word_freq", toString(lwfreq) );
  if ( !wwform.empty() ){
    KWargs args;
    args["set"] = "tscan-set";
    args["class"] = "wwform(" + wwform + ")";
    el->addPosAnnotation( args );
  }
}

void wordStats::print( ostream& os ) const {
  os << "word: [" <<word << "], lengte=" << wordLen 
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
  structStats( const string& cat ): basicStats( cat ),
				    wordCnt(0),
				    vdCnt(0),odCnt(0),
				    infCnt(0), presentCnt(0), pastCnt(0),
				    nameCnt(0),
				    pron1Cnt(0), pron2Cnt(0), pron3Cnt(0), 
				    passiveCnt(0),
				    pronRefCnt(0), archaicsCnt(0),
				    contentCnt(0),
				    nominalCnt(0),
				    onderCnt(0),
				    betrCnt(0),
				    tempConnCnt(0),
				    reeksConnCnt(0),
				    contConnCnt(0),
				    compConnCnt(0),
				    causeConnCnt(0),
				    propNegCnt(0),
				    morphNegCnt(0),
				    wordRepeatCnt(0),
				    wordOverlapCnt(0),
				    lemmaRepeatCnt(0),
				    lemmaOverlapCnt(0),
				    f50Cnt(0),
				    f65Cnt(0),
				    f77Cnt(0),
				    f80Cnt(0),
				    compCnt(0),
				    compLen(0),
				    wfreq(0),
				    wfreq_n(0),
				    lwfreq(0),
				    lwfreq_n(0),
				    polarity(NA),
				    surprisal(NA),
				    broadConcreteCnt(0),
				    strictConcreteCnt(0),
				    broadAbstractCnt(0),
				    strictAbstractCnt(0),
				    stateCnt(0),
				    actionCnt(0),
				    processCnt(0),
				    weirdCnt(0),
				    humanCnt(0),
				    npCnt(0),
				    indefNpCnt(0),
				    npSize(0),
				    dLevel(-1)
 {};
  ~structStats();
  virtual void print(  ostream& ) const;
  void addMetrics( FoliaElement *el ) const;
  void merge( structStats * );
  string id;
  string text;
  int wordCnt;
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
  int pronRefCnt;
  int archaicsCnt;
  int contentCnt;
  int nominalCnt;
  int onderCnt;
  int betrCnt;
  int tempConnCnt;
  int reeksConnCnt;
  int contConnCnt;
  int compConnCnt;
  int causeConnCnt;
  int propNegCnt;
  int morphNegCnt;
  int wordRepeatCnt;
  int wordOverlapCnt;
  int lemmaRepeatCnt;
  int lemmaOverlapCnt;
  int f50Cnt;
  int f65Cnt;
  int f77Cnt;
  int f80Cnt;
  int compCnt;
  int compLen;
  int wfreq; 
  int wfreq_n; 
  double lwfreq; 
  double lwfreq_n;
  double polarity;
  double surprisal;
  int broadConcreteCnt;
  int strictConcreteCnt;
  int broadAbstractCnt;
  int strictAbstractCnt;
  int stateCnt;
  int actionCnt;
  int processCnt;
  int weirdCnt;
  int humanCnt;
  int npCnt;
  int indefNpCnt;
  int npSize;
  int dLevel;
  vector<basicStats *> sv;
  map<string,int> heads;
  map<string,int> unique_words;
  map<string,int> unique_lemmas;
  map<NerProp, int> ners;
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
  wordLen += ss->wordLen;
  wordLenExNames += ss->wordLenExNames;
  morphLen += ss->morphLen;
  morphLenExNames += ss->morphLenExNames;
  nameCnt += ss->nameCnt;
  vdCnt += ss->vdCnt;
  odCnt += ss->odCnt;
  infCnt += ss->infCnt;
  passiveCnt += ss->passiveCnt;
  archaicsCnt += ss->archaicsCnt;
  contentCnt += ss->contentCnt;
  nominalCnt += ss->nominalCnt;
  onderCnt += ss->onderCnt;
  betrCnt += ss->betrCnt;
  tempConnCnt += ss->tempConnCnt;
  reeksConnCnt += ss->reeksConnCnt;
  contConnCnt += ss->contConnCnt;
  compConnCnt += ss->compConnCnt;
  causeConnCnt += ss->causeConnCnt;
  propNegCnt += ss->propNegCnt;
  morphNegCnt += ss->morphNegCnt;
  wordRepeatCnt += ss->wordRepeatCnt;
  wordOverlapCnt += ss->wordOverlapCnt;
  lemmaRepeatCnt += ss->lemmaRepeatCnt;
  lemmaOverlapCnt += ss->lemmaOverlapCnt;
  f50Cnt += ss->f50Cnt;
  f65Cnt += ss->f65Cnt;
  f77Cnt += ss->f77Cnt;
  f80Cnt += ss->f80Cnt;
  compCnt += ss->compCnt;
  compLen += ss->compLen;
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
  pronRefCnt += ss->pronRefCnt;
  strictAbstractCnt += ss->strictAbstractCnt;
  broadAbstractCnt += ss->broadAbstractCnt;
  strictConcreteCnt += ss->strictConcreteCnt;
  broadConcreteCnt += ss->broadConcreteCnt;
  stateCnt += ss->stateCnt;
  actionCnt += ss->actionCnt;
  processCnt += ss->processCnt;
  weirdCnt += ss->weirdCnt;
  humanCnt += ss->humanCnt;
  npCnt += ss->npCnt;
  indefNpCnt += ss->indefNpCnt;
  npSize += ss->npSize;
  if ( ss->dLevel > 0 ){
    if ( dLevel < 0 )
      dLevel = ss->dLevel;
    else
      dLevel += ss->dLevel;
  }
  sv.push_back( ss );
  aggregate( heads, ss->heads );
  aggregate( unique_words, ss->unique_words );
  aggregate( unique_lemmas, ss->unique_lemmas );
  aggregate( ners, ss->ners );
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
  os << category << " gemiddelde woordlengte " << wordLen/double(wordCnt) << endl;
  os << category << " gemiddelde woordlengte zonder Namen " << wordLenExNames/double(wordCnt - nameCnt) << endl;
  os << category << " gemiddelde aantal morphemen " << morphLen/double(wordCnt) << endl;
  os << category << " gemiddelde aantal morphemen zonder Namen " << morphLenExNames/double(wordCnt-nameCnt) << endl;
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

void structStats::addMetrics( FoliaElement *el ) const {
  Document *doc = el->doc();
  addOneMetric( doc, el, "word_count", toString(wordCnt) );
  addOneMetric( doc, el, "name_count", toString(nameCnt) );
  addOneMetric( doc, el, "vd_count", toString(vdCnt) );
  addOneMetric( doc, el, "od_count", toString(odCnt) );
  addOneMetric( doc, el, "inf_count", toString(infCnt) );
  addOneMetric( doc, el, "archaic_count", toString(archaicsCnt) );
  addOneMetric( doc, el, "content_count", toString(contentCnt) );
  addOneMetric( doc, el, "nominal_count", toString(nominalCnt) );
  addOneMetric( doc, el, "subordinate_cnt", toString(onderCnt) );
  addOneMetric( doc, el, "temporeel_connective_cnt", toString(tempConnCnt) );
  addOneMetric( doc, el, "reeks_connective_cnt", toString(reeksConnCnt) );
  addOneMetric( doc, el, "contrastief_connective_cnt", toString(contConnCnt) );
  addOneMetric( doc, el, "comparatief_connective_cnt", toString(compConnCnt) );
  addOneMetric( doc, el, "causaal_connective_cnt", toString(causeConnCnt) );
  addOneMetric( doc, el, "relative_cnt", toString(betrCnt) );
  if ( polarity != NA )
    addOneMetric( doc, el, "polarity", toString(polarity) );
  if ( surprisal != NA )
    addOneMetric( doc, el, "surprisal", toString(surprisal) );
  addOneMetric( doc, el, "proper_negative_count", toString(propNegCnt) );
  addOneMetric( doc, el, "morph_negative_count", toString(morphNegCnt) );
  addOneMetric( doc, el, "compound_count", toString(compCnt) );
  addOneMetric( doc, el, "compound_len", toString(compLen) );
  addOneMetric( doc, el, 
		"word_repeat_count", toString( wordRepeatCnt ) );
  addOneMetric( doc, el, 
		"word_overlap_count", toString( wordOverlapCnt ) );
  addOneMetric( doc, el, 
		"lemma_repeat_count", toString( lemmaRepeatCnt ) );
  addOneMetric( doc, el, 
		"lemma_overlap_count", toString( lemmaOverlapCnt ) );
  addOneMetric( doc, el, "word_freq", toString(lwfreq) );
  addOneMetric( doc, el, "word_freq_nonames", toString(lwfreq_n) );
  addOneMetric( doc, el, "freq50", toString(f50Cnt) );
  addOneMetric( doc, el, "freq65", toString(f65Cnt) );
  addOneMetric( doc, el, "freq77", toString(f77Cnt) );
  addOneMetric( doc, el, "freq80", toString(f80Cnt) );
  addOneMetric( doc, el, "pronoun_tw_count", toString(presentCnt) );
  addOneMetric( doc, el, "pronoun_verl_count", toString(pastCnt) );
  addOneMetric( doc, el, "pronoun_ref_count", toString(pronRefCnt) );
  addOneMetric( doc, el, "pronoun_1_count", toString(pron1Cnt) );
  addOneMetric( doc, el, "pronoun_2_count", toString(pron2Cnt) );
  addOneMetric( doc, el, "pronoun_3_count", toString(pron3Cnt) );
  addOneMetric( doc, el, "character_sum", toString(wordLen) );
  addOneMetric( doc, el, "character_sum_no_names", toString(wordLenExNames) );
  addOneMetric( doc, el, "morph_count", toString(morphLen) );
  addOneMetric( doc, el, "morph_count_no_names", toString(morphLenExNames) );
  addOneMetric( doc, el, "concrete_strict", toString(strictConcreteCnt) );
  addOneMetric( doc, el, "concrete_broad", toString(broadConcreteCnt) );
  addOneMetric( doc, el, "abstract_strict", toString(strictAbstractCnt) );
  addOneMetric( doc, el, "abstract_broad", toString(broadAbstractCnt) );
  addOneMetric( doc, el, "state_count", toString(stateCnt) );
  addOneMetric( doc, el, "action_count", toString(actionCnt) );
  addOneMetric( doc, el, "process_count", toString(processCnt) );
  addOneMetric( doc, el, "weird_count", toString(weirdCnt) );
  addOneMetric( doc, el, "human_count", toString(humanCnt) );
  addOneMetric( doc, el, "indef_np_count", toString(indefNpCnt) );
  addOneMetric( doc, el, "np_count", toString(npCnt) );
  addOneMetric( doc, el, "np_size", toString(npSize) );
  if ( dLevel >= 0 )
    addOneMetric( doc, el, "d_level", toString(dLevel) );

  /*
  os << category << " Named Entities ";
  for ( map<NerProp,int>::const_iterator it = ners.begin();
	it != ners.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  */

}

struct sentStats : public structStats {
  sentStats( Sentence *, const vector<string>&, 
	     const vector<string>&, xmlDoc * );
  virtual void print( ostream& ) const;
  void resolveConnectives();
  void addMetrics( FoliaElement *el ) const;
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

sentStats::sentStats( Sentence *s, 
		      const vector<string>& wordbuffer,
		      const vector<string>& lemmabuffer,
		      xmlDoc *alpDoc ): structStats("ZIN" ){
  id = s->id();
  text = UnicodeToUTF8( s->toktext() );
  vector<Word*> w = s->words();
  vector<double> surprisalV(w.size(),NA);
  if ( settings.doSurprisal ){
    surprisalV = runSurprisal( s, settings.surprisalPath );
    if ( surprisalV.size() != w.size() ){
      cerr << "MISMATCH! " << surprisalV.size() << " != " <<  w.size()<< endl;
      exit(-9);
    }
  }
  for ( size_t i=0; i < w.size(); ++i ){
    wordStats *ws = new wordStats( w[i], alpDoc, surprisalV[i], wordbuffer, lemmabuffer );
    if ( ws->prop == ISPUNCT ){
      delete ws;
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
      
      wordRepeatCnt += ws->wordRepeatCnt;
      wordOverlapCnt += ws->wordOverlapCnt;
      lemmaRepeatCnt += ws->lemmaRepeatCnt;
      lemmaOverlapCnt += ws->lemmaOverlapCnt;
      wordLen += ws->wordLen;
      wordLenExNames += ws->wordLenExNames;
      morphLen += ws->morphLen;
      morphLenExNames += ws->morphLenExNames;
      unique_words[lowercase(ws->word)] += 1;
      unique_lemmas[ws->lemma] += 1;
      if ( ws->compLen > 0 )
	++compCnt;
      compLen += ws->compLen;
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
	;// ignore
      }
      if ( ws->isPassive )
	passiveCnt++;
      if ( ws->isPronRef )
	pronRefCnt++;
      if ( ws->archaic )
	archaicsCnt++;
      if ( ws->isContent )
	contentCnt++;
      if ( ws->isNominal )
	nominalCnt++;
      if ( ws->isOnder )
	onderCnt++;
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
      case CONCRETE:
	strictConcreteCnt++;
	broadConcreteCnt++;
	break;
      case ABSTRACT:
	strictAbstractCnt++;
	broadAbstractCnt++;
	break;
      case BROAD:
	broadConcreteCnt++;
	broadAbstractCnt++;
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
  resolveConnectives();
  lwfreq = log( wfreq / w.size() );
  lwfreq_n = log( wfreq_n / (wordCnt-nameCnt) );
  np_length( s, npCnt, indefNpCnt, npSize );
  if ( alpDoc ){
    dLevel = get_d_level( s, alpDoc );
  }
  addMetrics( s );
}

void sentStats::print( ostream& os ) const {
  os << category << "[" << text << "]" << endl;
  os << category << " er zijn " << propNegCnt << " negatieve woorden en " 
     << morphNegCnt << " negatieve morphemen gevonden";
  if ( propNegCnt + morphNegCnt > 1 )
    os << " er is dus een dubbele ontkenning! ";
  os << endl;
  structStats::print( os );
}

void sentStats::addMetrics( FoliaElement *el ) const {
  structStats::addMetrics( el );
}

struct parStats: public structStats {
  parStats( Paragraph * );
  void print( ostream& ) const;
  void addMetrics( FoliaElement *el ) const;
  int sentCnt;
};

parStats::parStats( Paragraph *p ): 
  structStats( "PARAGRAAF" ), 
  sentCnt(0) 
{
  id = p->id();
  vector<Sentence*> sents = p->sentences();
  for ( size_t i=0; i < sents.size(); ++i ){
    xmlDoc *alpDoc = 0;
    if ( settings.doAlpino ){
      cerr << "calling Alpino parser" << endl;
      alpDoc = AlpinoParse( sents[i] );
      if ( !alpDoc ){
	cerr << "alpino parser failed!" << endl;
      }
    }
    vector<string> wordbuffer;
    vector<string> lemmabuffer;
    if ( i > 0 ){
      // store the words and lemmas' of the previous sentence
      vector<Word*> wv = sents[i-1]->words();
      for ( size_t w=0; w < wv.size(); ++w ){
	wordbuffer.push_back( lowercase( UnicodeToUTF8( wv[w]->text() ) ) );
	lemmabuffer.push_back( wv[w]->lemma( frog_lemma_set ) );
      }
    }
    sentStats *ss = new sentStats( sents[i], wordbuffer, lemmabuffer, alpDoc );
    xmlFreeDoc( alpDoc );
    merge( ss );
    sentCnt++;
  }
  lwfreq = log( wfreq / sents.size() );
  lwfreq_n = log( wfreq_n / (wordCnt-nameCnt) );
  addMetrics( p );
}

void parStats::print( ostream& os ) const {
  os << category << " with " << sentCnt << " Sentences" << endl;
  structStats::print( os );
}

void parStats::addMetrics( FoliaElement *el ) const {
  structStats::addMetrics( el );
  addOneMetric( el->doc(), el, 
		"sentence_count", toString(sentCnt) );
}

struct docStats : public structStats {
  docStats( Document * );
  void print( ostream& ) const;
  void addMetrics( FoliaElement *el ) const;
  int sentCnt;
  int doc_word_argCnt;
  int doc_word_overlapCnt;
  int doc_lemma_argCnt;
  int doc_lemma_overlapCnt;
};

docStats::docStats( Document *doc ):
  structStats( "DOCUMENT" ), sentCnt(0), 
  doc_word_argCnt(0), doc_word_overlapCnt(0),
  doc_lemma_argCnt(0), doc_lemma_overlapCnt(0) 
{
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
  for ( size_t i=0; i != pars.size(); ++i ){
    parStats *ps = new parStats( pars[i] );
    merge( ps );
    sentCnt += ps->sentCnt;
  }
  lwfreq = log( wfreq / pars.size() );
  lwfreq_n = log( wfreq_n / (wordCnt-nameCnt) );
  vector<Word*> wv = doc->words();
  vector<string> wordbuffer(50);
  vector<string> lemmabuffer(50);
  for ( size_t w=0; w < wv.size() && w < 50; ++w ){
    wordbuffer[w] = lowercase(UnicodeToUTF8( wv[w]->text() ));
    lemmabuffer[w] = wv[w]->lemma( frog_lemma_set );
  }
  for ( size_t i=50; i < wv.size(); ++i ){
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
      argument_overlap( lemma, lemmabuffer, 
			doc_lemma_argCnt, doc_lemma_overlapCnt );
      lemmabuffer.erase(lemmabuffer.begin());
      lemmabuffer.push_back( lemma );
    }
  }
  addMetrics( pars[0]->parent() );
}

double rarity( const docStats *d, double level ){
  map<string,int>::const_iterator it = d->unique_lemmas.begin();
  int rare = 0;
  while ( it != d->unique_lemmas.end() ){
    if ( it->second <= level )
      ++rare;
    ++it;
  }
  return rare/double( d->unique_lemmas.size() );
}

void docStats::addMetrics( FoliaElement *el ) const {
  structStats::addMetrics( el );
  addOneMetric( el->doc(), el, 
		"sentence_count", toString( sentCnt ) );
  addOneMetric( el->doc(), el, 
		"paragraph_count", toString( sv.size() ) );
  addOneMetric( el->doc(), el, 
		"TTW", toString( unique_words.size()/double(wordCnt) ) );
  addOneMetric( el->doc(), el, 
		"TTL", toString( unique_lemmas.size()/double(wordCnt) ) );
  addOneMetric( el->doc(), el, 
		"rarity", toString( rarity( this, settings.rarityLevel ) ) );
  addOneMetric( el->doc(), el, 
		"word_argument_count", toString( doc_word_argCnt ) );
  addOneMetric( el->doc(), el, 
		"word_argument_overlap_count", toString( doc_word_overlapCnt ) );
  addOneMetric( el->doc(), el, 
		"lemma_argument_count", toString( doc_lemma_argCnt ) );
  addOneMetric( el->doc(), el, 
		"lemma_argument_overlap_count", toString( doc_lemma_overlapCnt ) );
}

void docStats::print( ostream& os ) const {
  os << category << " with "  << sv.size() << " paragraphs and "
     << sentCnt << " Sentences" << endl;
  os << "TTW = " << unique_words.size()/double(wordCnt) << endl;
  os << "TTL = " << unique_lemmas.size()/double(wordCnt) << endl;
  os << "rarity(" << settings.rarityLevel << ")=" 
     << rarity( this, settings.rarityLevel ) << endl;
  structStats::print( os );
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
      //      doc->save( "/tmp/folia.1.xml" );
      docStats analyse( doc );
      doc->save( outName );
      delete doc;
      LOG << "saved output in " << outName << endl;
      // cerr << analyse << endl;
    }
  }
  
  exit(EXIT_SUCCESS);
}

