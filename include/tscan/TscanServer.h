#ifndef TSCAN_SERVER_H
#define TSCAN_SERVER_H
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

#include "ticcutils/LogStream.h"
#include "tscan/Configuration.h"

class TscanClass {
 public:
  TscanClass( Timbl::TimblOpts& opts );
  ~TscanClass();
  void init( Timbl::TimblOpts& Opts );
  void exec( const std::string&, std::ostream& );
  TiCC::LogStream cur_log;
 private:
  bool getConfig( const std::string& );
  folia::Document *getFrogResult( std::istream& );
  std::string configFile;
  std::string logFile;
  Configuration config;
  LogLevel dbLevel;
};

#endif
