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
  cerr << "TScan server " << VERSION << endl;
  cerr << "based on " << TimblServer::VersionName() << endl;
  doDaemon = true;
  dbLevel = LogNormal;
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
  
  if ( !configFile.empty() &&
       config.fill( configFile ) ){
    settings.init( config );
    if ( opts.Find( "t", val, mood ) ){
      RunOnce( val );
    }
    else {
      RunServer();
    }
  }
  else {
    cerr << "invalid configuration" << endl;
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

struct wordStats {
  string word;
  string pos;
  string posHead;
  string lemma;
  string morph;
  int wordLen;
};

ostream& operator<<( ostream& os, const wordStats& ws ){
  os << "word: " << ws.word << ", HEAD: " << ws.posHead;
  return os;
}

struct sentStats {
  string id;
  string text;
  int wordLen;
  int wordCnt;
  vector<wordStats> wsv;
  map<string,int> heads;
  set<string> unique_lemmas;
  set<string> unique_words;
};

ostream& operator<<( ostream& os, const sentStats& s ){
  os << "sentence " << s.id << " [" << s.text << "]" << endl;
  os << "sentence POS tags: ";
  for ( map<string,int>::const_iterator it = s.heads.begin();
	it != s.heads.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "gemiddelde woordlengte " << s.wordLen/double(s.wordCnt) << endl;
  os << "zinslengte " << s.wordCnt << endl;
  for ( size_t i=0;  i < s.wsv.size(); ++ i ){
    os << s.wsv[i] << endl;
  }
  return os;
}

struct parStats {
  string id;
  size_t wordCnt;
  int wordLen;
  int sentCnt;
  vector<sentStats> ssv;
  map<string,int> heads;
  set<string> unique_lemmas;
  set<string> unique_words;
};

ostream& operator<<( ostream& os, const parStats& p ){
  os << "paragraph: " << p.id << endl;
  os << "paragraph POS tags: ";
  for ( map<string,int>::const_iterator it = p.heads.begin();
	it != p.heads.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "aantal zinnen " << p.sentCnt << endl;
  os << "gemiddelde woordlengte " << p.wordLen/double(p.wordCnt) << endl << endl;
  os << "gemiddelde zinslengte " << p.wordCnt/double(p.sentCnt) << endl << endl;
  for ( size_t i=0; i != p.ssv.size(); ++i ){
    os << p.ssv[i] << endl;
  }
  os << endl;
  return os;
}

struct docStats {
  string id;
  size_t wordCnt;
  int wordLen;
  int sentCnt;
  vector<parStats> psv;
  map<string,int> heads;
  set<string> unique_lemmas;
  set<string> unique_words;
};

ostream& operator<<( ostream& os, const docStats& d ){
  os << "Document: " << d.id << endl;
  os << "TTW = " << d.unique_words.size()/double(d.wordCnt) << endl;
  os << "TTL = " << d.unique_lemmas.size()/double(d.wordCnt) << endl;
  os << "Document POS tags: ";
  for ( map<string,int>::const_iterator it = d.heads.begin();
	it != d.heads.end(); ++it ){
    os << it->first << "[" << it->second << "] ";
  }
  os << endl;
  os << "gemiddelde woordlengte " << d.wordLen/double(d.wordCnt) << endl;
  os << "gemiddelde zinslengte " << d.wordCnt/double(d.sentCnt) << endl << endl;
  for ( size_t i=0; i < d.psv.size(); ++i ){
    os << d.psv[i] << endl;
  }
  os << endl;
  return os;
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
  string morph;
  vector<folia::MorphologyLayer*> ml = w->annotations<folia::MorphologyLayer>();
  for ( size_t q=0; q < ml.size(); ++q ){
    vector<folia::Morpheme*> m = ml[q]->select<folia::Morpheme>();
    for ( size_t t=0; t < m.size(); ++t ){
      morph += "[" + folia::UnicodeToUTF8( m[t]->text() ) + "]";
    }
    if ( q < ml.size()-1 )
      morph += "/";
  }
  ws.morph = morph;
  return ws;
}

sentStats analyseSent( folia::Sentence *s ){
  sentStats ss;
  ss.id = s->id();
  ss.text = UnicodeToUTF8( s->toktext() );
  vector<folia::Word*> w = s->words();
  ss.wordCnt = 0;
  ss.wordLen = 0;
  for ( size_t i=0; i < w.size(); ++i ){
    wordStats ws = analyseWord( w[i] );
    if ( ws.posHead != "LET" )
      ss.heads[ws.posHead]++;
    ss.wordCnt += 1;
    ss.wordLen += ws.wordLen;
    ss.unique_words.insert(lowercase(ws.word));
    ss.unique_lemmas.insert(ws.lemma);
    ss.wsv.push_back( ws );
  }
  return ss;
}

parStats analysePar( folia::Paragraph *p ){
  parStats ps;
  ps.id = p->id();
  ps.wordCnt = 0;
  ps.wordLen = 0;
  ps.sentCnt = 0;
  vector<folia::Sentence*> sents = p->sentences();
  for ( size_t i=0; i < sents.size(); ++i ){
    if ( settings.doAlpino ){
      cerr << "calling Alpino parser" << endl;
      if ( !AlpinoParse( sents[i] ) ){
	cerr << "alpino parser failed!" << endl;
      }
    }
    sentStats ss = analyseSent( sents[i] );
    ps.wordCnt += ss.wordCnt;
    ps.wordLen += ss.wordLen;
    ps.sentCnt += 1;
    ps.ssv.push_back( ss );
    ps.heads.insert( ss.heads.begin(), ss.heads.end() );
    ps.unique_words.insert( ss.unique_words.begin(), ss.unique_words.end() );
    ps.unique_lemmas.insert( ss.unique_lemmas.begin(), ss.unique_lemmas.end() );
  }
  return ps;
}

void analyseDoc( folia::Document *doc, ostream& os ){
  os << "document: " << doc->id() << endl;
  os << "paragraphs " << doc->paragraphs().size() << endl;
  os << "sentences " << doc->sentences().size() << endl;
  os << "words " << doc->words().size() << endl;
  docStats result;
  result.wordCnt = 0;
  result.wordLen = 0;
  result.sentCnt = 0;
  vector<folia::Paragraph*> pars = doc->paragraphs();
  for ( size_t i=0; i != pars.size(); ++i ){
    parStats ps = analysePar( pars[i] );
    result.wordCnt += ps.wordCnt;
    result.wordLen += ps.wordLen;
    result.sentCnt += ps.sentCnt;
    result.psv.push_back( ps );
    result.heads.insert( ps.heads.begin(),
			 ps.heads.end() );
    result.unique_words.insert( ps.unique_words.begin(),
				ps.unique_words.end() );
    result.unique_lemmas.insert( ps.unique_lemmas.begin(),
				 ps.unique_lemmas.end() );
  }
  os << result << endl;
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
  folia::Document *doc;
  if ( !result.empty() && result.size() > 10 ){
    SDBG << "start FoLiA parsing" << endl;
    doc = new folia::Document();
    try {
      doc->readFromString( result );
      SDBG << "finished" << endl;
    }
    catch ( std::exception& e ){
      doc = 0;
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
      analyseDoc( doc, os );
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
      LOG << "Started logging " << endl;	
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
      LOG << "Started logging " << endl;	
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

