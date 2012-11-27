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
#include "tscan/Alpino.h"

using namespace std;
using namespace folia;
using namespace TiCC;

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
	  return "koppelww";
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


void getNodes( xmlNode *pnt, vector<xmlNode*>& result ){
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE && Name(pnt) == "node" ){
      result.push_back( pnt );
      getNodes( pnt->children, result );
    }
    pnt = pnt->next;
  }
}

vector<xmlNode *> getNodes( xmlDoc *doc ){
  xmlNode *pnt = xmlDocGetRootElement( doc );
  vector<xmlNode*> result;
  getNodes( pnt->children, result );
  return result;
}

int get_d_level( Sentence *s, xmlDoc *alp ){
  vector<PosAnnotation*> poslist;
  vector<Word*> wordlist = s->words();
  int pv_counter = 0;
  int neven_counter = 0;
  for ( size_t i=0; i < wordlist.size(); ++i ){
    Word *w = wordlist[i];
    vector<PosAnnotation*> posV = w->select<PosAnnotation>("http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn");
    if ( posV.size() != 1 )
      throw ValueError( "word doesn't have POS tag info" );
    PosAnnotation *pa = posV[0];
    string pos = pa->feat("head");
    poslist.push_back( pa );
    if ( pos == "WW" ){
      //      cerr << "WW " << pa->xmlstring() << endl;
      string wvorm = pa->feat("wvorm");
      if( wvorm == "pv" )
	++pv_counter;
      //      cerr << "pv_counter= " << pv_counter << endl;
    }
    if ( pos == "VG" ){
      //      cerr << "VG " << pa->xmlstring() << endl;
      string cp = pa->feat("conjtype");
      if ( cp == "neven" )
	++neven_counter;
      //      cerr << "neven_counter= " << neven_counter << endl;
    }
  }
  if ( pv_counter - neven_counter > 2 ){
    // op niveau 7 staan zinnen met meerdere bijzinnen, maar deelzinnen die 
    // in nevenschikking staan tellen hiervoor niet mee
    return 7;
  }

  // < 7
  vector<xmlNode *> nodelist = getNodes( alp );
  for ( size_t i=0; i < nodelist.size(); ++i ){  
    // we kijken of het om een level 6 zin gaat:
    // Zinnen met een betrekkelijke bijzin die het subject modificeert 
    //    ("De man, die erg op Pietje leek, zette het op een lopen.")
    // Het onderwerp van de zin is genominaliseerd 
    //    ("Het weigeren van Pietje was voor Jantje reden om ermee te stoppen.")
    xmlNode *node = nodelist[i];
    KWargs atts = getAttributes( node );
    if ( atts["rel"] == "mod" && atts["cat"] == "rel" ){
      //      cerr << "HIT MOD node " << atts << endl;
      KWargs attsp = getAttributes( node->parent );
      //      cerr << "parent: " << attsp << endl;
      if ( attsp["rel"] == "su" )
	return 6;
    }
    else if ( atts["rel"] == "su" && 
	      ( atts["cat"] == "cp" 
		|| atts["cat"] == "whsub" || atts["cat"] == "whrel"
		|| atts["cat"] == "ti"  || atts["cat"] == "oti" 
		|| atts["cat"] == "inf" ) ){
      //      cerr << "HIT SU node " << atts << endl;
      return 6;
    }
    else if ( atts["pos"] == "verb" ){
      //      cerr << "HIT verb node " << atts << endl;
      KWargs attsp = getAttributes( node->parent );
      //      cerr << "parent: " << attsp << endl;
      if ( attsp["rel"] == "su" && attsp["cat"] == "np" )
	return 6;
    }
  }
  
  // < 6
  for ( size_t i=0; i < poslist.size(); ++i ){
    // we kijken of het om een level 5 zin gaat
    // Zinnen met ondergeschikte bijzinnen 
    //     ("Pietje wilde naar huis, omdat het regende.")
    string pos = poslist[i]->feat("head");
    if ( pos == "VG" ){
      string cp = poslist[i]->feat("conjtype");
      if ( cp == "onder" ){
	if ( poslist[i]->parent()->text() != "dat" )
	  return 5;
      }
    }
  }

  // < 5
  for ( size_t i=0; i < nodelist.size(); ++i ){  
    // we kijken of het om een level 4 zin gaat
    //  "Non-finite complement with its own understood subject". Kan ik even geen voorbeeld van bedenken :p
    // comparatieven met een object van vergelijking 
    //    ("Pietje is groter dan Jantje.")                           
    KWargs atts = getAttributes( nodelist[i] );
    if ( atts["rel"] == "obcomp" )
      return 4;
  }  
  vector<xmlNode*> vcnodes;
  for ( size_t i=0; i < nodelist.size(); ++i ){  
    KWargs atts = getAttributes( nodelist[i] );
    if ( atts["rel"] == "vc" )
      vcnodes.push_back( nodelist[i] );
  }  
  bool found4 = false;
  for ( size_t i=0; i < vcnodes.size(); ++i ){  
    xmlNode *node = vcnodes[i];
    xmlNode *pnt = node->children;
    string index;
    while ( pnt ){
      if ( pnt->type == XML_ELEMENT_NODE ){
	KWargs atts = getAttributes( pnt );
	string index = atts["index"];
	if ( !index.empty() && atts["rel"] == "su" ){
	  found4 = true;
	  break;
	}
      }
      pnt = pnt->next;
    }
    if ( found4 ){
      vector< xmlNode *> siblinglist = getSibblings( node );
      for ( size_t i=0; i < siblinglist.size(); ++i ){
	KWargs atts = getAttributes( siblinglist[i] );
	if ( atts["index"] == index && atts["rel"] == "obj" )
	  return 4;
      }
    }
  }

  // < 4
  //  cerr << "DLEVEL < 4 " << endl;
  for ( size_t i=0; i < nodelist.size(); ++i ){ 
    // we kijken of het om een level 3 zin gaat
    // Zinnen met een objectsmodificerende betrekkelijke bijzin: 
    //    "Ik keek naar de man die de straat overstak."
    // Bijzin als object van het hoofdww: 
    //     "Ik wist dat hij boos was"
    // Subject extraposition: zinnen met een uitgesteld onderwerp 
    //     "Het verbaast me dat je dat weet."
    //   Kun je in Alpino detecteren met aan het 'sup' label voor een 
    //   voorlopig onderwerp 
    KWargs atts = getAttributes( nodelist[i] );
    //    cerr << "bekijk " << atts << endl;
    if ( atts["rel"] == "mod" && atts["cat"] == "rel" ){
      //      cerr << "case mod/rel " << endl;
      KWargs attsp = getAttributes( nodelist[i]->parent );
      //      cerr << "bekijk " << attsp << endl;
      if ( attsp["rel"] == "obj1" )
	return 3;
    }
    else if ( atts["pos"] == "verb" ){
      //      cerr << "case VERB " << endl;
      KWargs attsp = getAttributes( nodelist[i]->parent );
      //      cerr << "bekijk " << attsp << endl;
      if ( attsp["rel"] == "obj1" && attsp["cat"] == "np" )
	return 3;
    }
    else if ( atts["rel"] == "vc" && 
	      ( atts["cat"] == "cp" || 
		atts["cat"] == "whsub" ) ){
      //      cerr << "case VC" << endl;
      return 3;
    }
    else if ( atts["rel"] == "sup" ){
      //      cerr << "case sup" << endl;
      return 3;
    }
  }
  
  // < 3 
  for ( size_t i=0; i < poslist.size(); ++i ){
    // we kijken of het om een level 2 zin gaat
    // zinnen met nevenschikkingen
    cerr << "bekijk " << poslist[i] << endl;
    cerr << "head=" << poslist[i]->feat("head") << endl;
    cerr << "head=" << poslist[i]->feat("headfeature") << endl;
    string pos = poslist[i]->feat("head");
    if ( pos == "VG" ){
      string cp = poslist[i]->feat("conjtype");
      if ( cp == "neven" )
	return 2;
    }
  }
  
  // < 2
  for ( size_t i=0; i < nodelist.size(); ++i ){ 
    // we kijken of het om een level 1 zin gaat
    // Zinnen met een infinitief waarbij infinitief en persoonsvorm hetzelfde
    // onderwerp hebben 
    //     ("Pietje vergat zijn haar te kammen.")
    KWargs atts = getAttributes( nodelist[i] );
    if ( atts["rel"] == "vc" ){
      //      cerr << "VC node " << atts << endl;
      if ( atts["cat"] == "ti" 
	   || atts["cat"] == "oti"
	   || atts["cat"] == "inf" ){
	xmlNode *su_node = node_search( nodelist[i], "rel", "su" );
	if ( su_node ){
	  KWargs atts1 = getAttributes( su_node );
	  //	  cerr << "su node 1 " << atts1 << endl;
	  string node_index = atts1["index"];
	  if ( !node_index.empty() ){
	    vector< xmlNode *> siblinglist = getSibblings( nodelist[i] );
	    for ( size_t i=0; i < siblinglist.size(); ++i ){
	      KWargs atts2 = getAttributes( siblinglist[i] );
	      if ( atts2["rel"] == "su" ){
		//		cerr << "su node 2 " << atts2 << endl;
		if ( atts2["index"] == node_index )
		  return 1;
	      }
	    }
	  }
	}
      }
    }
  }
  
  // < 1
  return 0;
}

xmlDoc *AlpinoParse( folia::Sentence *s ){
  string txt = folia::UnicodeToUTF8(s->toktext());
  //  cerr << "parse line: " << txt << endl;
  struct stat sbuf;
  int res = stat( "/tmp/alpino", &sbuf );
  if ( !S_ISDIR(sbuf.st_mode) ){
    res = mkdir( "/tmp/alpino/", S_IRWXU|S_IRWXG );
    if ( res ){
      cerr << "problem: " << res << endl;
      exit( EXIT_FAILURE );
    }
  }
  res = stat( "/tmp/alpino/parses", &sbuf );
  if ( !S_ISDIR(sbuf.st_mode) ){
    res = mkdir( "/tmp/alpino/parses",  S_IRWXU|S_IRWXG );
    if ( res ){
      cerr << "problem: " << res << endl;
      exit( EXIT_FAILURE );
    }
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

