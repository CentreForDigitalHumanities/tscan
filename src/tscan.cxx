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
  
};

void settingData::init( const Configuration& cf ){
  string val = cf.lookUp( "useAlpino" );
  doAlpino = false;
  if( !Timbl::stringTo( val, doAlpino ) ){
    cerr << "invalid value for 'useAlpino' in config file" << endl;
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
		JUSTAWORD };

struct wordStats {
  wordStats(): wordLen(0),wordLenExNames(0),
	       morphLen(0),morphLenExNames(0),
	       prop(JUSTAWORD) {};
  string word;
  string pos;
  string posHead;
  string lemma;
  int wordLen;
  int wordLenExNames;
  int morphLen;
  int morphLenExNames;
  WordProp prop;
};

ostream& operator<<( ostream& os, const wordStats& ws ){
  os << "word: [" << ws.word << "], lengte=" << ws.wordLen 
     << ", aantal morphemen=" << ws.morphLen <<  ", HEAD: " << ws.posHead;
  switch ( ws.prop ){
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
  }
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

struct sentStats {
  sentStats(): wordCnt(0),aggWordLen(0),aggWordLenExNames(0),
	       aggMorphLen(0),aggMorphLenExNames(0),
	       vdCnt(0),odCnt(0),infCnt(0), presentCnt(0), pastCnt(0),
	       nameCnt(0) {};
  string id;
  string text;
  int wordCnt;
  int aggWordLen;
  int aggWordLenExNames;
  int aggMorphLen;
  int aggMorphLenExNames;
  int vdCnt;
  int odCnt;
  int infCnt;
  int presentCnt;
  int pastCnt;
  int nameCnt;
  vector<wordStats> wsv;
  map<string,int> heads;
  map<string,int> unique_words;
  map<string,int> unique_lemmas;
  map<NerProp, int> ners;
};

ostream& operator<<( ostream& os, const sentStats& s ){
  os << "Zin " << s.id << " [" << s.text << "]" << endl;
  os << "Zin POS tags: ";
  for ( map<string,int>::const_iterator it = s.heads.begin();
	it != s.heads.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "#Voltooid deelwoorden " << s.vdCnt 
     << " gemiddeld: " << s.vdCnt/double(s.wordCnt) << endl;
  os << "#Onvoltooid deelwoorden " << s.odCnt 
     << " gemiddeld: " << s.odCnt/double(s.wordCnt) << endl;
  os << "#Infinitieven " << s.infCnt 
     << " gemiddeld: " << s.infCnt/double(s.wordCnt) << endl;
  os << "#Persoonsvorm (TW) " << s.presentCnt
     << " gemiddeld: " << s.presentCnt/double(s.wordCnt) << endl;
  os << "#Persoonsvorm (VERL) " << s.pastCnt
     << " gemiddeld: " << s.pastCnt/double(s.wordCnt) << endl;
  os << "Zin gemiddelde woordlengte " << s.aggWordLen/double(s.wordCnt) << endl;
  os << "Zin gemiddelde woordlengte zonder Namen " << s.aggWordLenExNames/double(s.wordCnt - s.nameCnt) << endl;
  os << "Zin gemiddelde aantal morphemen " << s.aggMorphLen/double(s.wordCnt) << endl;
  os << "Zin gemiddelde aantal morphemen zonder Namen " << s.aggMorphLenExNames/double(s.wordCnt-s.nameCnt) << endl;
  os << "Zin aantal Namen " << s.nameCnt << endl;
  os << "Zin Named Entities ";
  for ( map<NerProp,int>::const_iterator it = s.ners.begin();
	it != s.ners.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "Zin lengte " << s.wordCnt << endl;
  for ( size_t i=0;  i < s.wsv.size(); ++ i ){
    os << s.wsv[i] << endl;
  }
  return os;
}

struct parStats {
  parStats(): aggWordCnt(0), aggWordLen(0), aggWordLenExNames(0),
	      aggMorphLen(0), aggMorphLenExNames(0), aggNameCnt(0),
	      sentCnt(0), 
	      aggVdCnt(0), aggOdCnt(0), aggInfCnt(0), aggPresentCnt(0),
	      aggPastCnt(0) {};
  string id;
  int aggWordCnt;
  int aggWordLen;
  int aggWordLenExNames;
  int aggMorphLen;
  int aggMorphLenExNames;
  int aggNameCnt;
  int sentCnt;
  int aggVdCnt;
  int aggOdCnt;
  int aggInfCnt;
  int aggPresentCnt;
  int aggPastCnt;
  vector<sentStats> ssv;
  map<string,int> heads;
  map<string,int> unique_words;
  map<string,int> unique_lemmas;
  map<NerProp, int> ners;
};

ostream& operator<<( ostream& os, const parStats& p ){
  os << "Paragraaf " << p.id << endl;
  os << "Paragraaf POS tags: ";
  for ( map<string,int>::const_iterator it = p.heads.begin();
	it != p.heads.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "#Voltooid deelwoorden " << p.aggVdCnt
     << " gemiddeld: " << p.aggVdCnt/double(p.aggWordCnt) << endl;
  os << "#Onvoltooid deelwoorden " << p.aggOdCnt
     << " gemiddeld: " << p.aggOdCnt/double(p.aggWordCnt) << endl;
  os << "#Infinitieven " << p.aggInfCnt
     << " gemiddeld: " << p.aggInfCnt/double(p.aggWordCnt) << endl;
  os << "#Persoonsvorm (TW) " << p.aggPresentCnt
     << " gemiddeld: " << p.aggPresentCnt/double(p.aggWordCnt) << endl;
  os << "#Persoonsvorm (VERL) " << p.aggPastCnt
     << " gemiddeld: " << p.aggPastCnt/double(p.aggWordCnt) << endl;
  os << "Paragraaf aantal zinnen " << p.sentCnt << endl;
  os << "Paragraaf gemiddelde woordlengte " << p.aggWordLen/double(p.aggWordCnt) << endl;
  os << "Paragraaf gemiddelde woordlengte zonder Namen " << p.aggWordLenExNames/double(p.aggWordCnt-p.aggNameCnt) << endl;
  os << "Paragraaf gemiddelde aantal morphemen " << p.aggMorphLen/double(p.aggWordCnt) << endl;
  os << "Paragraaf gemiddelde aantal morphemen zonder Namen " << p.aggMorphLenExNames/double(p.aggWordCnt-p.aggNameCnt) << endl;
  os << "Paragraaf gemiddelde zinslengte " << p.aggWordCnt/double(p.sentCnt) << endl;
  os << "Paragraaf aantal namen " << p.aggNameCnt << endl << endl;
  os << "Paragraaf Named Entities ";
  for ( map<NerProp,int>::const_iterator it = p.ners.begin();
	it != p.ners.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;

  for ( size_t i=0; i != p.ssv.size(); ++i ){
    os << p.ssv[i] << endl;
  }
  os << endl;
  return os;
}

struct docStats {
  docStats(): aggWordCnt(0),aggWordLen(0),aggWordLenExNames(0),
	      aggMorphLen(0),aggMorphLenExNames(), 
	      aggSentCnt(0),aggNameCnt(0),
	      aggVdCnt(0), aggOdCnt(0), aggInfCnt(0), 
	      aggPresentCnt(0), aggPastCnt(0) {};
  string id;
  int aggWordCnt;
  int aggWordLen;
  int aggWordLenExNames;
  int aggMorphLen;
  int aggMorphLenExNames;
  int aggSentCnt;
  int aggNameCnt;
  int aggVdCnt;
  int aggOdCnt;
  int aggInfCnt;
  int aggPresentCnt;
  int aggPastCnt;
  vector<parStats> psv;
  map<string,int> heads;
  map<string,int> unique_words;
  map<string,int> unique_lemmas;
  map<NerProp, int> ners;
};

ostream& operator<<( ostream& os, const docStats& d ){
  os << "Document: " << d.id << endl;
  os << "#paragraphs " << d.psv.size() << endl;
  os << "#sentences " << d.aggSentCnt << endl;
  os << "#words " << d.aggWordCnt << endl;
  os << "#namen " << d.aggNameCnt << endl;
  os << "#Voltooid deelwoorden " << d.aggVdCnt
     << " gemiddeld: " << d.aggVdCnt/double(d.aggWordCnt) << endl;
  os << "#Onvoltooid deelwoorden " << d.aggOdCnt
     << " gemiddeld: " << d.aggOdCnt/double(d.aggWordCnt) << endl;
  os << "#Infinitieven " << d.aggInfCnt
     << " gemiddeld: " << d.aggInfCnt/double(d.aggWordCnt) << endl;
  os << "#Persoonsvorm (TW) " << d.aggPresentCnt
     << " gemiddeld: " << d.aggPresentCnt/double(d.aggWordCnt) << endl;
  os << "#Persoonsvorm (VERL) " << d.aggPastCnt
     << " gemiddeld: " << d.aggPastCnt/double(d.aggWordCnt) << endl;
  os << "TTW = " << d.unique_words.size()/double(d.aggWordCnt) << endl;
  os << "TTL = " << d.unique_lemmas.size()/double(d.aggWordCnt) << endl;
  os << "Document POS tags: ";
  for ( map<string,int>::const_iterator it = d.heads.begin();
	it != d.heads.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "Document Named Entities ";
  for ( map<NerProp,int>::const_iterator it = d.ners.begin();
	it != d.ners.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "Document gemiddelde woordlengte " << d.aggWordLen/double(d.aggWordCnt) << endl;
  os << "Document gemiddelde woordlengte zonder Namen " << d.aggWordLenExNames/double(d.aggWordCnt-d.aggNameCnt) << endl;
  os << "Document gemiddelde aantal morphemen " << d.aggMorphLen/double(d.aggWordCnt) << endl;
  os << "Document gemiddelde aantal morphemen zonder Namen " << d.aggMorphLenExNames/double(d.aggWordCnt-d.aggNameCnt) << endl;
  os << "Document gemiddelde zinslengte " << d.aggWordCnt/double(d.aggSentCnt) << endl << endl;
  for ( size_t i=0; i < d.psv.size(); ++i ){
    os << d.psv[i] << endl;
  }
  os << endl;
  return os;
}

NerProp lookupNer( const Word *w, const Sentence * s ){
  NerProp result = NONER;
  vector<Entity*> v = s->select<Entity>("http://ilk.uvt.nl/folia/sets/frog-ner-nl");
  for ( size_t i=0; i < v.size(); ++i ){
    FoliaElement *e = v[i];
    for ( size_t j=0; j < e->size(); ++j ){
      if ( e->index(j) == w ){
	cerr << "hit " << e->index(j) << " in " << v[i] << endl;
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

wordStats analyseWord( folia::Word *w ){
  wordStats ws;
  ws.word = UnicodeToUTF8( w->text() );
  ws.wordLen = w->text().length();
  vector<folia::PosAnnotation*> pos = w->select<folia::PosAnnotation>();
  if ( pos.size() != 1 )
    throw ValueError( "word doesn't have POS tag info" );
  ws.pos = pos[0]->cls();
  ws.posHead = pos[0]->feat("head");
  ws.lemma= w->lemma();
  if ( ws.posHead == "LET" )
    ws.prop = ISPUNCT;
  else if ( ws.posHead == "SPEC" && ws.pos.find("eigen") != string::npos )
    ws.prop = ISNAME;
  if ( ws.posHead == "WW" ){
    string wvorm = pos[0]->feat("wvorm");
    if ( wvorm == "inf" )
      ws.prop = ISINF;
    else if ( wvorm == "vd" )
      ws.prop = ISVD;
    else if ( wvorm == "od" )
      ws.prop = ISOD;
    else if ( wvorm == "pv" ){
      string tijd = pos[0]->feat("pvtijd");
      if ( tijd == "tgw" )
	ws.prop = ISPVTGW;
      else if ( tijd == "verl" )
	ws.prop = ISPVVERL;
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
  if ( ws.prop != ISPUNCT ){
    int max = 0;
    vector<folia::MorphologyLayer*> ml = w->annotations<folia::MorphologyLayer>();
    for ( size_t q=0; q < ml.size(); ++q ){
      vector<folia::Morpheme*> m = ml[q]->select<folia::Morpheme>();
      if ( m.size() > max )
	max = m.size();
    }
    ws.morphLen = max;
    if ( ws.prop != ISNAME ){
      ws.wordLenExNames = ws.wordLen;
      ws.morphLenExNames = max;
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

sentStats analyseSent( folia::Sentence *s ){
  sentStats ss;
  ss.id = s->id();
  ss.text = UnicodeToUTF8( s->toktext() );
  vector<folia::Word*> w = s->words();
  for ( size_t i=0; i < w.size(); ++i ){
    wordStats ws = analyseWord( w[i] );
    if ( ws.prop == ISPUNCT )
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
	ss.ners[ner]++;
	break;
      default:
	;// skip
      }
      ss.wordCnt++;
      ss.heads[ws.posHead]++;
      // if ( ws.posHead == "WW" ){
      // 	string vt = classifyVerb( w[i], s );
      // }
      ss.aggWordLen += ws.wordLen;
      ss.aggWordLenExNames += ws.wordLenExNames;
      ss.aggMorphLen += ws.morphLen;
      ss.aggMorphLenExNames += ws.morphLenExNames;
      ss.unique_words[lowercase(ws.word)] += 1;
      ss.unique_lemmas[ws.lemma] += 1;
      ss.wsv.push_back( ws );
      switch ( ws.prop ){
      case ISNAME:
	ss.nameCnt++;
	break;
      case ISVD:
	ss.vdCnt++;
	break;
      case ISINF:
	ss.infCnt++;
	break;
      case ISOD:
	ss.odCnt++;
	break;
      case ISPVVERL:
	ss.pastCnt++;
	break;
      case ISPVTGW:
	ss.presentCnt++;
	break;
      default:
	;// ignore
      }
    }
  }
  return ss;
}

parStats analysePar( folia::Paragraph *p ){
  parStats ps;
  ps.id = p->id();
  vector<folia::Sentence*> sents = p->sentences();
  for ( size_t i=0; i < sents.size(); ++i ){
    if ( settings.doAlpino ){
      cerr << "calling Alpino parser" << endl;
      if ( !AlpinoParse( sents[i] ) ){
	cerr << "alpino parser failed!" << endl;
      }
    }
    sentStats ss = analyseSent( sents[i] );
    ps.aggWordCnt += ss.wordCnt;
    ps.aggWordLen += ss.aggWordLen;
    ps.aggWordLenExNames += ss.aggWordLenExNames;
    ps.aggMorphLen += ss.aggMorphLen;
    ps.aggMorphLenExNames += ss.aggMorphLenExNames;
    ps.sentCnt++;
    ps.aggNameCnt += ss.nameCnt;
    ps.aggVdCnt += ss.vdCnt;
    ps.aggOdCnt += ss.odCnt;
    ps.aggInfCnt += ss.infCnt;
    ps.aggPresentCnt += ss.presentCnt;
    ps.aggPastCnt += ss.pastCnt;
    ps.ssv.push_back( ss );
    aggregate( ps.heads, ss.heads );
    aggregate( ps.unique_words, ss.unique_words );
    aggregate( ps.unique_lemmas, ss.unique_lemmas );
    aggregate( ps.ners, ss.ners );
  }
  return ps;
}

docStats analyseDoc( folia::Document *doc ){
  docStats result;
  vector<folia::Paragraph*> pars = doc->paragraphs();
  for ( size_t i=0; i != pars.size(); ++i ){
    parStats ps = analysePar( pars[i] );
    result.aggWordCnt += ps.aggWordCnt;
    result.aggWordLen += ps.aggWordLen;
    result.aggWordLenExNames += ps.aggWordLenExNames;
    result.aggMorphLen += ps.aggMorphLen;
    result.aggMorphLenExNames += ps.aggMorphLenExNames;
    result.aggNameCnt += ps.aggNameCnt;
    result.aggSentCnt += ps.sentCnt;
    result.aggVdCnt += ps.aggVdCnt;
    result.aggOdCnt += ps.aggOdCnt;
    result.aggInfCnt += ps.aggInfCnt;
    result.aggPresentCnt += ps.aggPresentCnt;
    result.aggPastCnt += ps.aggPastCnt;
    result.psv.push_back( ps );
    aggregate( result.heads, ps.heads );
    aggregate( result.unique_words, ps.unique_words );
    aggregate( result.unique_lemmas, ps.unique_lemmas );
    aggregate( result.ners, ps.ners );
  }
  return result;
}

folia::Document *TscanServerClass::getFrogResult( istream& is ){
  string host = config.lookUp( "host", "frog" );
  string port = config.lookUp( "port", "frog" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    SLOG << "failed to open Frog connection: "<< host << ":" << port << endl;
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
    doc = new folia::Document();
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
    folia::Document *doc = getFrogResult( is );
    if ( !doc ){
      os << "big trouble: no FoLiA document created " << endl;
    }
    else {
      //      cerr << *doc << endl;
      docStats result = analyseDoc( doc );
      cerr << *doc << endl;
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

