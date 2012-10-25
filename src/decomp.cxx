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

#include <cstdio> // for remove()
#include <cstring> // for strerror()
#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <iostream>
#include <fstream>
#include "config.h"
#include "libfolia/folia.h"
#include "libfolia/document.h"
#include "ticcutils/PrettyPrint.h"
#include "tscan/decomp.h"

using namespace std;
using namespace folia;
using namespace TiCC;

int runDecompoundWord( const std::string& word ){
  struct stat sbuf;
  int res = stat( "/tmp/decomp", &sbuf );
  if ( !S_ISDIR(sbuf.st_mode) ){
    res = mkdir( "/tmp/decomp/", S_IRWXU|S_IRWXG );
    if ( res ){
      cerr << "problem: " << res << endl;
      exit( EXIT_FAILURE );
    }
  }
  ofstream os( "/tmp/decomp/decomp.in" );
  os << word << endl;
  os.close();
  string cmd = "/home/sloot/svn/decompounder/decompound.sh /tmp/decomp/decomp.in /tmp/decomp/decomp.out";
  res = system( cmd.c_str() );
  if ( res ){
    cerr << "RES = " << res << endl;
  }
  remove( "/tmp/decomp/decomp.in" );
  ifstream is( "/tmp/decomp/decomp.out" );
  if( is ){
    string line;
    getline( is, line );
    vector<string> v;
    int n = split( line, v );
    if ( n > 1 ){
      string comp = v[n-1];
      n = split_at( comp, v, "#" );
      if ( n <= 1 )
	return 0;
      return n;
    }
  }
  return 0;
}

