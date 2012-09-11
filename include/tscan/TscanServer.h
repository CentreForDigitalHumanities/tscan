#ifndef TSCAN_SERVER_H
#define TSCAN_SERVER_H

#include "ticcutils/LogStream.h"

class TscanServerClass {
 public:
  TscanServerClass( Timbl::TimblOpts& opts );
  ~TscanServerClass();
  void Start( Timbl::TimblOpts& Opts );
  void exec( const std::string&, std::ostream& );
  TiCC::LogStream cur_log;
 private:
  void RunServer();
  bool getConfig( const std::string& );
  int maxConn;
  int serverPort;
  std::string configFile;
  std::string pidFile;
  std::string logFile;
  bool doDaemon;
  int tcp_socket;
  LogLevel dbLevel;
};

#endif
