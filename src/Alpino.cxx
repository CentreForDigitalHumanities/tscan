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

#ifdef OLD_STUFF
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
	FoliaElement *h = dep->append( new Headwords() );
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
#endif // OLD_STUFF

xmlNode *getAlpWord( xmlNode *node, const string& pos ){
  xmlNode *result = 0;
  xmlNode *pnt = node->children;
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE
	 && Name( pnt ) == "node" ){
      KWargs atts = getAttributes( pnt );
      string epos = atts["end"];
      if ( epos == pos ){
	string bpos = atts["begin"];
	int start = stringTo<int>( bpos );
	int finish = stringTo<int>( epos );
	//	cerr << "begin = " << start << ", end=" << finish << endl;
	if ( start + 1 == finish ){
	  result = pnt;
	}
	else {
	  result = getAlpWord( pnt, pos );
	}
      }
      else {
	result = getAlpWord( pnt, pos );
      }
    }
    if ( result )
      return result;
    pnt = pnt->next;
  }
  return result;
}

xmlNode *getAlpWord( xmlDoc *doc, const Word *w ){
  string id = w->id();
  string::size_type ppos = id.find_last_of( '.' );
  string posS = id.substr( ppos + 1 );
  if ( posS.empty() ){
    cerr << "unable to extract a word index from " << id << endl;
    return 0;
  }
  else {
    xmlNode *root = xmlDocGetRootElement( doc );
    return getAlpWord( root, posS );
  }
}

vector< xmlNode*> getSibblings( xmlNode *node ){
  vector<xmlNode *> result;
  xmlNode *pnt = node->prev;
  while ( pnt ){
    if ( pnt->prev )
      pnt = pnt->prev;
    else
      break;
  }
  if ( !pnt )
    pnt = node;
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE )
      result.push_back( pnt );
    pnt = pnt->next;
  }
  return result;
}

xmlNode *node_search( const xmlNode* node,
		      const string& att,
		      const string& val ){
  xmlNode *pnt = node->children;
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE ){
      KWargs atts = getAttributes( pnt );
      if ( atts[att] == val ){
	return pnt;
      }
      else if ( atts["root"] == "" ){
	xmlNode *tmp = node_search( pnt, att, val );
	if ( tmp )
	  return tmp;
      }
    }
    pnt = pnt->next;;
  }
  return 0;
}

xmlNode *node_search( const xmlNode* node,
		      const string& att,
		      const set<string>& values ){
  xmlNode *pnt = node->children;
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE ){
      KWargs atts = getAttributes( pnt );
      if ( values.find(atts[att]) != values.end() ){
	return pnt;
      }
      else if ( atts["root"] == "" ){
	xmlNode *tmp = node_search( pnt, att, values );
	if ( tmp )
	  return tmp;
      }
    }
    pnt = pnt->next;;
  }
  return 0;
}

const string modalA[] = { "kunnen", "moeten", "hoeven", "behoeven", "mogen",
			  "willen", "blijken", "lijken", "schijnen", "heten" };

const string koppelA[] = { "zijn", "worden", "blijven", "lijken", "schijnen",
			   "heten", "dunken", "voorkomen" };

set<string> modals = set<string>( modalA, modalA + 10 );
set<string> koppels = set<string>( koppelA, koppelA + 8 );

string classifyVerb( Word *w, xmlDoc *alp ){
  xmlNode *wnode = getAlpWord( alp, w );
  if ( wnode ){
    vector< xmlNode *> siblinglist = getSibblings( wnode );
    string pos = w->pos();
    string lemma = w->lemma();
    if ( lemma == "zijn" || lemma == "worden" ){
      xmlNode *obj_node = 0;
      xmlNode *su_node = 0;
      for ( size_t i=0; i < siblinglist.size(); ++i ){
	KWargs atts = getAttributes( siblinglist[i] );
	if ( atts["rel"] == "su" ){
	  su_node = siblinglist[i];
	  //	    cerr << "Found a SU node!" << su_node << endl;
	}
      }
      for ( size_t i=0; i < siblinglist.size(); ++i ){
	KWargs atts = getAttributes( siblinglist[i] );
	if ( atts["rel"] == "vc" && atts["cat"] == "ppart" ){
	  obj_node = node_search( siblinglist[i], "rel", "obj1" );
	  if ( obj_node ){
	    //	      cerr << "found an obj node! " << obj_node << endl;
	    KWargs atts = getAttributes( obj_node );
	    string index = atts["index"];
	    if ( !index.empty() ){
	      KWargs atts = getAttributes( su_node );
	      if ( atts["index"] == index ){
		return "passiefww";
	      }
	    }
	  }
	}
      }
    }
    if ( koppels.find( lemma ) != koppels.end() ){
      for ( size_t i=0; i < siblinglist.size(); ++i ){
	KWargs atts = getAttributes( siblinglist[i] );
	if ( atts["rel"] == "predc" )
	  return "passiefww";
      }
    }
    if ( lemma == "schijnen" ){
      for ( size_t i=0; i < siblinglist.size(); ++i ){
	KWargs atts = getAttributes( siblinglist[i] );
	if ( atts["rel"] == "su" ){
	  static string schijn_words[] = { "zon", "ster", "maan", "lamp", "licht" };
	  static set<string> sws( schijn_words, schijn_words+5 );
	  xmlNode *node = node_search( siblinglist[i], 
				       "root", sws );
	  if ( node )
	    return "hoofdww";
	}
      }
    }
    string data = XmlContent( wnode->children );
    if ( data == "zul" || data == "zal" 
	 || data == "zullen" || data == "zult" ) {
      return "tijdww";
    }
    if ( modals.find( lemma ) != modals.end() ){
      return "modaalww";
    }      
    if ( lemma == "hebben" ){
      for ( size_t i=0; i < siblinglist.size(); ++i ){
	KWargs atts = getAttributes( siblinglist[i] );
	if ( atts["rel"] == "vc" && atts["cat"] == "ppart" )
	  return "tijdww";
      }
      return "hoofdww";
    }
    if ( lemma == "zijn" ){
      return "tijdww";
    }
    return "hoofdww";
  }
  else
    return "none";
}


xmlDoc *AlpinoParse( folia::Sentence *s ){
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
    // extractAndAppendParse( xmldoc, s );
    // xmlFreeDoc( xmldoc );
    return xmldoc;
  }
  return 0;
}

