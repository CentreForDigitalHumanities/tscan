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
#include "tscan/Alpino.h"

using namespace std;
using namespace folia;

void addSU( xmlNode *n, vector<Word*>& words, FoliaElement *s ){
  if ( Name( n ) == "node" ){
    KWargs atts = getAttributes( n );
    string cls = atts["cat"];
    bool leaf = false;
    if ( cls.empty() ){
      cls = atts["lcat"];
      leaf = true;
    }
    if ( !cls.empty() ){
      FoliaElement *e = 
	s->append( new SyntacticUnit( s->doc(), "cls='" + cls + "'" ) );
      if ( leaf ){
	string posS = atts["begin"];
	int start = stringTo<int>( posS );
	e->append( words[start] );
      }
      else {
	xmlNode *pnt = n->children;
	while ( pnt ){
	  addSU( pnt, words, e );
	  pnt = pnt->next;
	}
      }
    }
  }
}

void extractSyntax( xmlNode *node, Sentence *s ){
  Document *doc = s->doc();
  doc->declare( AnnotationType::SYNTAX, 
		"mysyntaxset", 
		"annotator='alpino'" );
  vector<Word*> words = s->words();
  FoliaElement *layer = s->append( new SyntaxLayer( doc ) );
  FoliaElement *sent = layer->append( new SyntacticUnit( doc, "cls='s'" ) );
  xmlNode *pnt = node->children;
  while ( pnt ){
    addSU( pnt, words, sent );
    pnt = pnt->next;
  }
}

xmlNode *findSubHead( xmlNode *node ){
  xmlNode *pnt = node->children;
  while ( pnt ){
    KWargs atts = getAttributes( pnt );
    string rel = atts["rel"];
    if ( rel == "hd" ){
      return pnt;
      break;
    }
    pnt = pnt->next;
  }
  return 0;
}

void addDep( xmlNode *node, vector<Word*>& words, FoliaElement *layer ){
  KWargs atts = getAttributes( node );
  xmlNode *hd = 0;
  xmlNode *pnt = node->children;
  while ( pnt ){
    KWargs atts = getAttributes( pnt );
    string rel = atts["rel"];
    if ( rel == "hd" ){
      hd = pnt;
      break;
    }
    pnt = pnt->next;
  }
  if ( hd ){
    KWargs atts = getAttributes( hd );
    string posH = atts["begin"];
    int headStart = stringTo<int>( posH );
    pnt = node->children;
    while ( pnt ){
      if ( pnt != hd ){
	KWargs atts = getAttributes( pnt );
	string rel = atts["rel"];
	FoliaElement *dep = layer->append( new Dependency( layer->doc(),
							   "class='" + rel + "'" ) );
	FoliaElement *h = dep->append( new DependencyHead() );
	h->append( words[headStart] );
	xmlNode *sub = findSubHead( pnt );
	if ( !sub ){
	  string posD = atts["begin"];
	  int start = stringTo<int>( posD );
	  FoliaElement *d = dep->append( new DependencyDependent() );
	  d->append( words[start] );
	}
	else {
	  KWargs atts = getAttributes( sub );
	  string posD = atts["begin"];
	  int start = stringTo<int>( posD );
	  FoliaElement *d = dep->append( new DependencyDependent() );
	  d->append( words[start] );
	  addDep( pnt, words, layer );
	}
      }
      pnt = pnt->next;
    }
  }
  else {
    xmlNode *pnt = node->children;
    while ( pnt ){
      addDep( pnt, words, layer );
      pnt = pnt->next;
    }
  }
}

void extractDependency( xmlNode *node, folia::Sentence *s ){
  Document *doc = s->doc();
  doc->declare( AnnotationType::DEPENDENCY, 
		"mysdepset", 
		"annotator='alpino'" );
  vector<Word*> words = s->words();
  FoliaElement *layer = s->append( new DependenciesLayer( doc ) );
  addDep( node, words, layer );
}

void extractAndAppendParse( xmlDoc *doc, folia::Sentence *s ){
  xmlNode *root = xmlDocGetRootElement( doc );
  xmlNode *pnt = root->children;
  while ( pnt ){
    if ( folia::Name( pnt ) == "node" ){
      // 1 top node i hope
      extractSyntax( pnt, s );
      extractDependency( pnt, s );
      break;
    }
    pnt = pnt->next;
  }
}

bool AlpinoParse( folia::Sentence *s ){
  string txt = folia::UnicodeToUTF8(s->toktext());
  //  cerr << "parse line: " << txt << endl;
  struct stat sbuf;
  int res = stat( "/tmp/alpino", &sbuf );
  if ( !S_ISDIR(sbuf.st_mode) ){
    res = mkdir( "/tmp/alpino/", S_IRWXU|S_IRWXG );
  }
  res = stat( "/tmp/alpino/parses", &sbuf );
  if ( !S_ISDIR(sbuf.st_mode) ){
    res = mkdir( "/tmp/alpino/parses",  S_IRWXU|S_IRWXG );
  }
  ofstream os( "/tmp/alpino/tempparse.txt" );
  os << txt;
  os.close();
  string parseCmd = "Alpino -fast -flag treebank /tmp/alpino/parses end_hook=xml -parse < /tmp/alpino/tempparse.txt -notk > /dev/null 2>&1";
  res = system( parseCmd.c_str() );
  if ( res ){
    cerr << "RES = " << res << endl;
  }
  remove( "/tmp/alpino/tempparse.txt" );
  xmlDoc *xmldoc = xmlReadFile( "/tmp/alpino/parses/1.xml", 0, XML_PARSE_NOBLANKS );
  if ( xmldoc ){
    extractAndAppendParse( xmldoc, s );
  }
  return (xmldoc != 0 );
}

