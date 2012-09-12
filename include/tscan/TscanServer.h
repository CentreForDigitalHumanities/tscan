#ifndef TSCAN_SERVER_H
#define TSCAN_SERVER_H

#include "ticcutils/LogStream.h"
#include "tscan/Configuration.h"

class TscanServerClass {
 public:
  TscanServerClass( Timbl::TimblOpts& opts );
  ~TscanServerClass();
  void Start( Timbl::TimblOpts& Opts );
  void exec( const std::string&, std::ostream& );
  TiCC::LogStream cur_log;
 private:
  bool RunServer();
  bool RunOnce( const std::string& );
  bool getConfig( const std::string& );
  folia::Document *getFrogResult( std::istream& );
  std::string configFile;
  std::string pidFile;
  std::string logFile;
  Configuration config;
  bool doDaemon;
  int tcp_socket;
  LogLevel dbLevel;
};

#endif
