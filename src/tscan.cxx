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
#include "timblserver/FdStream.h"
#include "libfolia/folia.h"
#include "libfolia/document.h"
#include "ticcutils/StringOps.h"
#include "tscan/TscanServer.h"
#include "tscan/Alpino.h"

using namespace std;
using namespace TiCC;
using namespace folia;

#define SLOG (*Log(cur_log))
#define SDBG (*Dbg(cur_log))

#define LOG cerr

struct settingData {
  void init( const Configuration& );
  bool doAlpino;
  int rarityLevel;
};

void settingData::init( const Configuration& cf ){
  string val = cf.lookUp( "useAlpino" );
  doAlpino = false;
  if( !Timbl::stringTo( val, doAlpino ) ){
    cerr << "invalid value for 'useAlpino' in config file" << endl;
  }
  val = cf.lookUp( "rarityLevel" );
  if ( val.empty() ){
    rarityLevel = 5;
  }
  else if ( !Timbl::stringTo( val, rarityLevel ) ){ 
    cerr << "invalid value for 'rarityLevel' in config file" << endl;
  }
}

settingData settings;

inline void usage(){
  cerr << "usage:  tscan" << endl;
}

TscanServerClass::TscanServerClass( Timbl::TimblOpts& opts ):
  cur_log("T-Scan", StampMessage ){
  string val;
  bool mood;
  if ( opts.Find( "config", val, mood ) ){
    configFile = val;
    opts.Delete( "config" );
  }
  else {
    cerr << "missing --config=<file> option" << endl;
    usage();
    exit( EXIT_FAILURE );
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
    if ( val == "LogNormal" )
      cur_log.setlevel( LogNormal );
    else if ( val == "LogDebug" )
      cur_log.setlevel( LogDebug );
    else if ( val == "LogHeavy" )
      cur_log.setlevel( LogHeavy );
    else if ( val == "LogExtreme" )
      cur_log.setlevel( LogExtreme );
    else {
      cerr << "Unknown Debug mode! (-D " << val << ")" << endl;
    }
    opts.Delete( 'D' );
  }
  string fileName;
  if ( !opts.Find( "t", val, mood ) ){
    cerr << "TScan server " << VERSION << endl;
    cerr << "based on " << TimblServer::VersionName() << endl;
    doDaemon = true;
    dbLevel = LogNormal;
    if ( opts.Find( "pidfile", val ) ) {
      pidFile = val;
      opts.Delete( "pidfile" );
    }
    if ( opts.Find( "logfile", val ) ) {
      logFile = val;
      opts.Delete( "logfile" );
    }
    if ( opts.Find( "daemonize", val ) ) {
      doDaemon = ( val != "no" && val != "NO" && val != "false" && val != "FALSE" );
      opts.Delete( "daemonize" );
    }
    RunServer();
  }
  else {
    fileName = val;
    cerr << "TScan " << VERSION << endl;
    RunOnce( fileName );
  }
}

TscanServerClass::~TscanServerClass(){
}

struct childArgs{
  TscanServerClass *Mother;
  Sockets::ServerSocket *socket;
  int maxC;
};

inline void Split( const string& line, string& com, string& rest ){
  string::size_type b_it = line.find( '=' );
  if ( b_it != string::npos ){
    com = trim( line.substr( 0, b_it ) );
    rest = trim( line.substr( b_it+1 ) );
  }
  else {
    rest.clear();
    com = line;
  }
}  

void StopServerFun( int Signal ){
  if ( Signal == SIGINT ){
    exit(EXIT_FAILURE);
  }
  signal( SIGINT, StopServerFun );
}  

void BrokenPipeChildFun( int Signal ){
  if ( Signal == SIGPIPE ){
    signal( SIGPIPE, BrokenPipeChildFun );
  }
}

void AfterDaemonFun( int Signal ){
  if ( Signal == SIGCHLD ){
    exit(1);
  }
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

struct basicStats {
  basicStats( const string& cat ): category( cat ), 
				   wordLen(0),wordLenExNames(0),
				   morphLen(0), morphLenExNames(0) {};
  virtual ~basicStats(){};
  virtual void print( ostream& ) const;
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
  os << "BASICTATS PRINT" << endl;
  os << category << endl;
  os << " lengte=" << wordLen << ", aantal morphemen=" << morphLen << endl;
  os << " lengte zonder namem =" << wordLenExNames << ", aantal morphemen zonder namen=" << morphLenExNames << endl;
}

struct wordStats : public basicStats {
  wordStats(): basicStats( "WORD" ),
	       isPronRef(false), archaic(false),
	       prop(JUSTAWORD) {};
  void print( ostream& ) const;
  string word;
  string pos;
  string posHead;
  string lemma;
  bool isPronRef;
  bool archaic;
  WordProp prop;
};

void wordStats::print( ostream& os ) const {
  os << "word: [" <<word << "], lengte=" << wordLen 
     << ", aantal morphemen=" << morphLen <<  ", HEAD: " << posHead;
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
				    pron1Cnt(0), pron2Cnt(0), pron3Cnt(0), 
				    pronRefCnt(0),
				    nameCnt(0), archaicsCnt(0) {};
  virtual void print(  ostream& ) const;
  virtual void addMetrics( FoliaElement *el ) const;
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
  int pronRefCnt;
  int archaicsCnt;
  vector<basicStats *> sv;
  map<string,int> heads;
  map<string,int> unique_words;
  map<string,int> unique_lemmas;
  map<NerProp, int> ners;
};

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
  presentCnt += ss->presentCnt;
  pastCnt += ss->pastCnt;
  pron1Cnt += ss->pron1Cnt;
  pron2Cnt += ss->pron2Cnt;
  pron3Cnt += ss->pron3Cnt;
  pronRefCnt += ss->pronRefCnt;
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
  os << "#Persoonsvorm (TW) " << presentCnt
     << " gemiddeld: " << presentCnt/double(wordCnt) << endl;
  os << "#Persoonsvorm (VERL) " << pastCnt
     << " gemiddeld: " << pastCnt/double(wordCnt) << endl;
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

struct sentStats : public structStats {
  sentStats(): structStats("ZIN" ){};
  virtual void print( ostream& ) const;
};

void sentStats::print( ostream& os ) const {
  os << category << "[" << text << "]" << endl;
  os << structStats(*this) << endl;
}

struct parStats: public structStats {
  parStats(): structStats( "PARAGRAAF" ),
	      sentCnt(0) {};
  void print( ostream& ) const;
  void addMetrics( FoliaElement *el ) const;
  int sentCnt;
};

void parStats::print( ostream& os ) const {
  os << category << " with " << sentCnt << " Sentences" << endl;
  structStats::print( os );
}

struct docStats : public structStats {
  docStats(): structStats( "DOCUMENT" ), sentCnt(0) {};
  void print( ostream& ) const;
  void addMetrics( FoliaElement *el ) const;
  int sentCnt;
};

void addOneMetric( Document *doc, FoliaElement *parent,
		   const string& cls, const string& val ){
  MetricAnnotation *m = new MetricAnnotation( doc,
					      "class='" + cls + "', value='" 
					      + val + "'" );
  parent->append( m );
}

void structStats::addMetrics( FoliaElement *el ) const {
  Document *doc = el->doc();
  doc->declare( AnnotationType::METRIC, 
		"metricset", 
		"annotator='tscan'" );
  addOneMetric( doc, el, "word_count", toString(wordCnt) );
  addOneMetric( doc, el, "name_count", toString(nameCnt) );
  addOneMetric( doc, el, "vd_count", toString(vdCnt) );
  addOneMetric( doc, el, "od_count", toString(odCnt) );
  addOneMetric( doc, el, "inf_count", toString(infCnt) );
  addOneMetric( doc, el, "archaic_count", toString(infCnt) );
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

  /*
  os << category << " Named Entities ";
  for ( map<NerProp,int>::const_iterator it = ners.begin();
	it != ners.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  */

}

void parStats::addMetrics( FoliaElement *el ) const {
  structStats::addMetrics( el );
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
		"TTW", toString( unique_words.size()/double(wordCnt) ) );
  addOneMetric( el->doc(), el, 
		"TTL", toString( unique_lemmas.size()/double(wordCnt) ) );
  addOneMetric( el->doc(), el, 
		"rarity", toString( rarity( this, settings.rarityLevel ) ) );
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

NerProp lookupNer( const Word *w, const Sentence * s ){
  NerProp result = NONER;
  vector<Entity*> v = s->select<Entity>("http://ilk.uvt.nl/folia/sets/frog-ner-nl");
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

wordStats *analyseWord( Word *w ){
  wordStats *ws = new wordStats();
  ws->word = UnicodeToUTF8( w->text() );
  ws->wordLen = w->text().length();
  vector<PosAnnotation*> pos = w->select<PosAnnotation>();
  if ( pos.size() != 1 )
    throw ValueError( "word doesn't have POS tag info" );
  ws->pos = pos[0]->cls();
  ws->posHead = pos[0]->feat("head");
  ws->lemma= w->lemma();
  if ( ws->posHead == "LET" )
    ws->prop = ISPUNCT;
  else if ( ws->posHead == "SPEC" && ws->pos.find("eigen") != string::npos )
    ws->prop = ISNAME;
  else if ( ws->posHead == "WW" ){
    string wvorm = pos[0]->feat("wvorm");
    if ( wvorm == "inf" )
      ws->prop = ISINF;
    else if ( wvorm == "vd" )
      ws->prop = ISVD;
    else if ( wvorm == "od" )
      ws->prop = ISOD;
    else if ( wvorm == "pv" ){
      string tijd = pos[0]->feat("pvtijd");
      if ( tijd == "tgw" )
	ws->prop = ISPVTGW;
      else if ( tijd == "verl" )
	ws->prop = ISPVVERL;
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
  else if ( ws->posHead == "VNW" && lowercase( ws->word ) != "men" ){
    string cas = pos[0]->feat("case");
    ws->archaic = ( cas == "gen" || cas == "dat" );
    string vwtype = pos[0]->feat("vwtype");
    if ( vwtype == "pers" || vwtype == "refl" 
	 || vwtype == "pr" || vwtype == "bez" ) {
      string persoon = pos[0]->feat("persoon");
      if ( !persoon.empty() ){
	if ( persoon[0] == '1' )
	  ws->prop = ISPPRON1;
	else if ( persoon[0] == '2' )
	  ws->prop = ISPPRON2;
	else if ( persoon[0] == '3' ){
	  ws->prop = ISPPRON3;
	  ws->isPronRef = ( vwtype == "pers" || vwtype == "bez" );
	}
	else {
	  cerr << "PANIEK: een onverwachte PRONOUN persoon : " << persoon 
	       << " in word " << w << endl;
	  exit(3);
	}
      }
    }
    if ( vwtype == "aanw" )
      ws->isPronRef = true;
  }
  else if ( ws->posHead == "LID" ) {
    string cas = pos[0]->feat("case");
    ws->archaic = ( cas == "gen" || cas == "dat" );
  }
  if ( ws->prop != ISPUNCT ){
    int max = 0;
    vector<MorphologyLayer*> ml = w->annotations<MorphologyLayer>();
    for ( size_t q=0; q < ml.size(); ++q ){
      vector<Morpheme*> m = ml[q]->select<Morpheme>();
      if ( m.size() > max )
	max = m.size();
    }
    ws->morphLen = max;
    if ( ws->prop != ISNAME ){
      ws->wordLenExNames = ws->wordLen;
      ws->morphLenExNames = max;
    }
  }
  return ws;
}

FoliaElement *search( Word *w, FoliaElement *l ){
  for ( size_t i=0; i < l->size(); ++i ){
    if ( l->index(i) == w ){
      cerr << "FOUND " << w << endl;
      return l;
    }
    else {
      FoliaElement *tmp = search( w, l->index(i) );
      if ( tmp )
	return tmp;
    }
  }
  return 0;
}

vector<FoliaElement*> siblings( Word *w, Sentence *s ){
  vector<FoliaElement*> result;
  vector<SyntaxLayer*> l = s->select<SyntaxLayer>();
  if ( l.size() > 0 ){
    FoliaElement *parent = search( w, l[0] );
    if ( parent ){
      cerr << "FOUND " << parent << endl;
      for ( size_t i =0; i < parent->size(); ++ i ){
	if ( parent->index(i) != w ){
	  result.push_back( parent->index(i) );
	}
      }
    }
  }
  cerr << "found siblings! " << result << endl;
  return result;
}

string classifyVerb( Word *w, Sentence *s ){
  vector<FoliaElement*> v = siblings( w, s );
  return "";
}

sentStats *analyseSent( folia::Sentence *s ){
  sentStats *ss = new sentStats();
  ss->id = s->id();
  ss->text = UnicodeToUTF8( s->toktext() );
  vector<folia::Word*> w = s->words();
  for ( size_t i=0; i < w.size(); ++i ){
    wordStats *ws = analyseWord( w[i] );
    if ( ws->prop == ISPUNCT )
      continue;
    else {
      NerProp ner = lookupNer( w[i], s );
      switch( ner ){
      case LOC_B:
      case EVE_B:
      case MISC_B:
      case ORG_B:
      case PER_B:
      case PRO_B:
	ss->ners[ner]++;
	break;
      default:
	;
      }
      ss->wordCnt++;
      ss->heads[ws->posHead]++;
      // if ( ws.posHead == "WW" ){
      // 	string vt = classifyVerb( w[i], s );
      // }
      ss->wordLen += ws->wordLen;
      ss->wordLenExNames += ws->wordLenExNames;
      ss->morphLen += ws->morphLen;
      ss->morphLenExNames += ws->morphLenExNames;
      ss->unique_words[lowercase(ws->word)] += 1;
      ss->unique_lemmas[ws->lemma] += 1;
      ss->sv.push_back( ws );
      switch ( ws->prop ){
      case ISNAME:
	ss->nameCnt++;
	break;
      case ISVD:
	ss->vdCnt++;
	break;
      case ISINF:
	ss->infCnt++;
	break;
      case ISOD:
	ss->odCnt++;
	break;
      case ISPVVERL:
	ss->pastCnt++;
	break;
      case ISPVTGW:
	ss->presentCnt++;
	break;
      case ISPPRON1:
	ss->pron1Cnt++;
      case ISPPRON2:
	ss->pron2Cnt++;
      case ISPPRON3:
	ss->pron3Cnt++;
      default:
	;// ignore
      }
      if ( ws->isPronRef )
	ss->pronRefCnt++;
      if ( ws->archaic )
	ss->archaicsCnt++;
    }
  }

  return ss;
}

parStats *analysePar( folia::Paragraph *p ){
  parStats *ps = new parStats();
  ps->id = p->id();
  vector<folia::Sentence*> sents = p->sentences();
  for ( size_t i=0; i < sents.size(); ++i ){
    if ( settings.doAlpino ){
      cerr << "calling Alpino parser" << endl;
      if ( !AlpinoParse( sents[i] ) ){
	cerr << "alpino parser failed!" << endl;
      }
    }
    sentStats *ss = analyseSent( sents[i] );
    ps->merge( ss );
    ps->sentCnt++;
  }
  ps->addMetrics( p );
  return ps;
}

docStats analyseDoc( folia::Document *doc ){
  docStats result;
  vector<folia::Paragraph*> pars = doc->paragraphs();
  for ( size_t i=0; i != pars.size(); ++i ){
    parStats *ps = analysePar( pars[i] );
    result.merge( ps );
    result.sentCnt += ps->sentCnt;
  }
  result.addMetrics( pars[0]->parent() );
  return result;
}

Document *TscanServerClass::getFrogResult( istream& is ){
  string host = config.lookUp( "host", "frog" );
  string port = config.lookUp( "port", "frog" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    SLOG << "failed to open Frog connection: "<< host << ":" << port << endl;
    SLOG << "Reason: " << client.getMessage() << endl;
    return 0;
  }
  SDBG << "start input loop" << endl;
  string line;
  while ( getline( is, line ) ){
    SDBG << "read: " << line << endl;
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
  SDBG << "received data [" << result << "]" << endl;
  folia::Document *doc = 0;
  if ( !result.empty() && result.size() > 10 ){
    SDBG << "start FoLiA parsing" << endl;
    doc = new Document();
    try {
      doc->readFromString( result );
      SDBG << "finished" << endl;
    }
    catch ( std::exception& e ){
      SLOG << "FoLiaParsing failed:" << endl
	   << e.what() << endl;	  
    }
  }
  return doc;
}

void TscanServerClass::exec( const string& file, ostream& os ){
  SLOG << "try file ='" << file << "'" << endl;
  ifstream is( file.c_str() );
  if ( !is ){
    os << "failed to open file '" << file << "'" << endl;
  }
  else {
    os << "opened file " << file << endl;
    Document *doc = getFrogResult( is );
    if ( !doc ){
      os << "big trouble: no FoLiA document created " << endl;
    }
    else {
      doc->save( "/tmp/folia.1.xml" );
      docStats result = analyseDoc( doc );
      doc->save( "/tmp/folia.2.xml" );
      delete doc;
      os << result << endl;
    }
  }
}

// ***** This is the routine that is executed from a new thread **********
void *runChild( void *arg ){
  childArgs *args = (childArgs *)arg;
  TscanServerClass *theServer = args->Mother;
  LogStream *cur_log = &theServer->cur_log;
  Sockets::Socket *Sock = args->socket;
  static int service_count = 0;
  static pthread_mutex_t my_lock = PTHREAD_MUTEX_INITIALIZER;
  //
  // use a mutex to update the global service counter
  //
  pthread_mutex_lock( &my_lock );
  service_count++;
  int nw = 0;
  if ( service_count > args->maxC ){
    Sock->write( "Maximum connections exceeded\n" );
    Sock->write( "try again later...\n" );
    pthread_mutex_unlock( &my_lock );
    SLOG << "Thread " << pthread_self() << " refused " << endl;
  }
  else {
    // Greeting message for the client
    //
    pthread_mutex_unlock( &my_lock );
    time_t timebefore, timeafter;
    time( &timebefore );
    // report connection to the server terminal
    //
    SLOG << "Thread " << pthread_self() << ", Socket number = "
	 << Sock->getSockId() << ", started at: " 
	 << asctime( localtime( &timebefore) );
    signal( SIGPIPE, BrokenPipeChildFun );
    fdistream is( Sock->getSockId() );
    fdostream os( Sock->getSockId() );
    os << "Welcome to the T-scan server." << endl;
    string line;
    if ( getline( is, line ) ){
      line = TiCC::trim( line );
      SDBG << "FirstLine='" << line << "'" << endl;
      theServer->exec( line, os );
    }
    time( &timeafter );
    SLOG << "Thread " << pthread_self() << ", terminated at: " 
	 << asctime( localtime( &timeafter ) );
    SLOG << "Total time used in this thread: " << timeafter - timebefore 
	 << " sec, " << nw << " words processed " << endl;
  }
  // exit this thread
  //
  pthread_mutex_lock( &my_lock );
  service_count--;
  SLOG << "Socket total = " << service_count << endl;
  pthread_mutex_unlock( &my_lock );
  delete Sock;
  return 0;
}

bool TscanServerClass::RunOnce( const string& file ){
  if ( !logFile.empty() ){
    ostream *tmp = new ofstream( logFile.c_str() );
    if ( tmp && tmp->good() ){
      cerr << "switching logging to file " << logFile << endl;
      cur_log.associate( *tmp );
      cur_log.message( "T-Scan:" );
      SLOG << "Started logging " << endl;	
    }
    else {
      cerr << "unable to create logfile: " << logFile << endl;
      exit(EXIT_FAILURE);
    }
  }
  exec( file, cout );
  return true;
}
  
bool TscanServerClass::RunServer(){
  int maxConn = 25;
  int serverPort = -1;
  string value;
  value = config.lookUp( "maxconn" );
  if ( !value.empty() && !Timbl::stringTo( value, maxConn ) ){
    cerr << "invalid value for maxconn" << endl;
    return false;
  }
  value = config.lookUp( "port" );
  if ( !value.empty() && !Timbl::stringTo( value, serverPort ) ){
    cerr << "invalid value for port" << endl;
    return false;
  }

  cerr << "trying to start a Server on port: " << serverPort << endl
       << "maximum # of simultaneous connections: " << maxConn
       << endl;
  if ( !logFile.empty() ){
    ostream *tmp = new ofstream( logFile.c_str() );
    if ( tmp && tmp->good() ){
      cerr << "switching logging to file " << logFile << endl;
      cur_log.associate( *tmp );
      cur_log.message( "T-Scan:" );
      SLOG << "Started logging " << endl;	
    }
    else {
      cerr << "unable to create logfile: " << logFile << endl;
      cerr << "not started" << endl;
      exit(EXIT_FAILURE);
    }
  }
  int start = 0;
  if ( doDaemon ){
    signal( SIGCHLD, AfterDaemonFun );
    if ( logFile.empty() )
      start = TimblServer::daemonize( 1, 1 );
    else
      start = TimblServer::daemonize( 0, 0 );
  }
  if ( start < 0 ){
    LOG << "Failed to daemonize error= " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  };
  if ( !pidFile.empty() ){
    // we have a liftoff!
    // signal it to the world
    remove( pidFile.c_str() ) ;
    ofstream pid_file( pidFile.c_str() ) ;
    if ( !pid_file ){
      LOG << "Unable to create pidfile:"<< pidFile << endl;
      LOG << "TimblServer NOT Started" << endl;
      exit(EXIT_FAILURE);
    }
    else {
      pid_t pid = getpid();
      pid_file << pid << endl;
    }
  }
  
  // set the attributes
  pthread_attr_t attr;
  if ( pthread_attr_init(&attr) ||
       pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED ) ){
    LOG << "Threads: couldn't set attributes" << endl;
    exit(EXIT_FAILURE);
  }
  //
  // setup Signal handling to abort the server.
  signal( SIGINT, StopServerFun );
  
  pthread_t chld_thr;
  
  // start up server
  // 
  LOG << "Started Server on port: " << serverPort << endl
      << "Maximum # of simultaneous connections: " << maxConn
      << endl;
  
  Sockets::ServerSocket server;
  string portString = Timbl::toString<int>(serverPort);
  if ( !server.connect( portString ) ){
    LOG << "Failed to start Server: " << server.getMessage() << endl;
    exit(EXIT_FAILURE);
  }
  if ( !server.listen( maxConn ) < 0 ){
    LOG << "Server: listen failed " << strerror( errno ) << endl;
    exit(EXIT_FAILURE);
  };
  
  while(true){ // waiting for connections loop
    Sockets::ServerSocket *newSock = new Sockets::ServerSocket();
    if ( !server.accept( *newSock ) ){
      if( errno == EINTR )
	continue;
      else {
	LOG << "Server: Accept Error: " << server.getMessage() << endl;
	exit(EXIT_FAILURE);
      }
    }
    LOG << "Accepting Connection " << newSock->getSockId()
	<< " from remote host: " << newSock->getClientName() << endl;
    
    // create a new thread to process the incoming request 
    // (The thread will terminate itself when done processing
    // and release its socket handle)
    //
    childArgs *args = new childArgs();
    args->Mother = this;
    args->maxC = maxConn;
    args->socket = newSock;
    pthread_create( &chld_thr, &attr, runChild, (void *)args );
    // the server is now free to accept another socket request 
  }
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
  TscanServerClass server( opts );
  exit(EXIT_SUCCESS);
}

