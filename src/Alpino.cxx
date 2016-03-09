/*
  T-scan

  Copyright (c) 1998 - 2015

  This file is part of tscan

  tscan is free software; you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  tscan is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affere General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

*/

#include <cstdio> // for remove()
#include <unistd.h>
#include <iostream>
#include <vector>
#include <set>
#include <fstream>
#include "config.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/XMLtools.h"
#include "libfolia/folia.h"
#include "tscan/Alpino.h"

using namespace std;
using namespace folia;
using namespace TiCC;


xmlNode *getAlpNodeWord( xmlDoc *doc, const Word *w ){
  // search the XML node that matches de FoLiA word w
  string id = w->id();
  string::size_type ppos = id.find_last_of( '.' );
  string posS = id.substr( ppos + 1 );
  if ( posS.empty() ){
    cerr << "unable to extract a word index from " << id << endl;
    return 0;
  }
  else {
    list<xmlNode*> nodelist = TiCC::FindNodes( doc, "//node" );
    list<xmlNode*>::const_iterator it = nodelist.begin();
    while ( it != nodelist.end() ) {
      xmlNode *pnt = *it;
      KWargs atts = getAttributes( pnt );
      string epos = atts["end"];
      if ( epos == posS ){
	string bpos = atts["begin"];
	int start = TiCC::stringTo<int>( bpos );
	int finish = TiCC::stringTo<int>( epos );
	//	cerr << "begin = " << start << ", end=" << finish << endl;
	if ( start + 1 == finish ){
	  // the node must exactly be 1 long
	  return pnt;
	}
      }
      ++it;
    }
    return 0;
  }
}

vector< xmlNode*> getSibblings( const xmlNode *node ){
  // create a list of all sibblings of 'node'
  vector<xmlNode *> result;
  xmlNode *pnt = node->parent->children;
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE && pnt != node )
      result.push_back( pnt );
    pnt = pnt->next;
  }
  return result;
}

xmlNode *node_search( const xmlNode* node,
		      const string& att,
		      const string& val ){
  // resursively search for a node with att=val
  xmlNode *pnt = node->children;
  while ( pnt ){
    // breadth first search
    if ( pnt->type == XML_ELEMENT_NODE ){
      if ( getAttribute( pnt, att ) == val ){
	return pnt;
      }
    }
    pnt = pnt->next;
  }
  // no luck, so get down the non-root nodes
  pnt = node->children;
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE ){
      if ( getAttribute( pnt, "root" ) == "" ){
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
  // resursively search for a node with an att that has one of the values
  xmlNode *pnt = node->children;
  while ( pnt ){
    // breath first search
    if ( pnt->type == XML_ELEMENT_NODE ){
      string aval = getAttribute( pnt, att );
      if ( values.find( aval ) != values.end() )
	return pnt;
    }
    pnt = pnt->next;;
  }
  pnt = node->children;
  // no luck, so get down the non-root nodes
  pnt = node->children;
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE ){
      if ( getAttribute( pnt, "root" ) == "" ){
	xmlNode *tmp = node_search( pnt, att, values );
	if ( tmp )
	  return tmp;
      }
    }
    pnt = pnt->next;;
  }
  return 0;
}

void get_index_nodes( const xmlNode* node, vector<xmlNode*>& result ){
  // recursively search for nodes with the 'index' attribute
  xmlNode *pnt = node->children;
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE ){
      KWargs atts = getAttributes( pnt );
      if ( atts["index"] != "" &&
	   !(atts["pos"] == "" && atts["cat"] == "" ) ){
	result.push_back( pnt );
      }
      else if ( atts["root"] == "" ){
	get_index_nodes( pnt, result );
      }
    }
    pnt = pnt->next;;
  }
}

vector<xmlNode *> getIndexNodes( xmlDoc *doc ){
  // search for nodes with the 'index' attribute
  xmlNode *pnt = xmlDocGetRootElement( doc );
  vector<xmlNode*> result;
  get_index_nodes( pnt->children, result );
  return result;
}


const string modalA[] = { "kunnen", "moeten", "hoeven", "behoeven", "mogen",
			  "willen", "blijken", "lijken", "schijnen", "heten" };

const string koppelA[] = { "zijn", "worden", "blijven", "lijken", "schijnen",
			   "heten", "blijken", "dunken", "voorkomen" };

set<string> modals = set<string>( modalA,
				  modalA + sizeof(modalA)/sizeof(string) );
set<string> koppels = set<string>( koppelA,
				   koppelA + sizeof(koppelA)/sizeof(string) );

string toString( const DD_type& t ){
  string result;
  switch ( t ){
  case SUB_VERB:
    result = "subject-verb";
    break;
  case OBJ1_VERB:
    result = "object-verb";
    break;
  case OBJ2_VERB:
    result = "lijdend-verb";
    break;
  case VERB_PP:
    result = "verb-pp";
    break;
  case VERB_VC:
    result = "verb-vc";
    break;
  case VERB_COMP:
    result = "verb-comp";
    break;
  case NOUN_DET:
    result = "noun-det";
    break;
  case PREP_OBJ1:
    result = "prep-obj1";
    break;
  case CRD_CNJ:
    result = "crd-cnj";
    break;
  case COMP_BODY:
    result = "comp-body";
    break;
  case NOUN_VC:
    result = "noun-vc";
    break;
  case VERB_SVP:
    result = "verb-svp";
    break;
  case VERB_PREDC_N:
    result = "verb-pred-n";
    break;
  case VERB_PREDC_A:
    result = "verb-pred-adj";
    break;
  case VERB_MOD_BW:
    result = "verb-mod-bw";
    break;
  case VERB_MOD_A:
    result = "verb-mod-adv";
    break;
  case  VERB_NOUN:
    result = "verb-nounc";
    break;
  default:
    result = "ERROR unknown translation for DD_type";
  }
  return result;
}

int get_begin( const xmlNode *n ){
  string bpos = getAttribute( n, "begin" );
  return TiCC::stringTo<int>( bpos );
}

void store_result( multimap<DD_type,int>& result, DD_type type,
		   const xmlNode *n1, const xmlNode*n2,
		   const set<size_t>& puncts ){
  // store distances per type. Compensate for skipped punctuation
  int pos1 = get_begin( n1 );
  int pos2 = get_begin( n2 );
  if ( pos1 > pos2 )
    swap( pos1, pos2 );
  int dist = pos2-pos1-1;
  for ( int i=pos1; i <= pos2; ++i )
    if ( puncts.find( i ) != puncts.end() ){
      //      cerr << "lower the dist, because of a punctuation" << endl;
      --dist;
    }
  //  cerr << "store " << type << "(" << pos1 << "," << pos2 << ")=" << dist << endl;
  if ( dist >= 0 ){
    result.insert( make_pair( type, dist ) );
  }
}

multimap<DD_type, int> getDependencyDist( const xmlNode *head_node,
					  const set<size_t>& puncts ){
  // walk down the Alpino tree and gather all types of distances
  multimap<DD_type,int> result;
  if ( head_node ){
    KWargs atts = getAttributes( head_node );
    string head_rel = atts["rel"];
    string head_cat = atts["cat"];
    string head_pos = atts["pos"];
    if ( head_rel == "hd" && head_pos == "verb" ){
      vector< xmlNode *> head_siblings = getSibblings( head_node );
      for ( vector< xmlNode *>::const_iterator it=head_siblings.begin();
	    it != head_siblings.end();
	    ++it ){
	KWargs args = getAttributes( *it );
	//	cerr << "bekijk " << args << endl;
	if ( args["rel"] == "su" || args["rel"] == "sup" ){
	  if ( !(*it)->children ){
	    //	    cerr << "geval 1 " << endl;
	    xmlNode *target = *it;
	    if ( args["index"] != "" &&
		 args["pos"] == "" && args["cat"] == "" ){
	      //	      cerr << "geval 2 " << endl;
	      vector<xmlNode*> inodes = getIndexNodes( head_node->doc );
	      for ( size_t i=0; i < inodes.size(); ++i ){
		KWargs iatts = getAttributes(inodes[i]);
		if ( iatts["index"] == args["index"] ){
		  target = inodes[i];
		  break;
		}
	      }
	      if ( target->children ){
		xmlNode *res = node_search( target , "rel", "cnj" );
		if ( res ){
		  //		  cerr << "geval 3 " << endl;
		  string root = getAttribute( res, "root" );
		  if ( !root.empty() ){
		    //		    cerr << "geval 3A " << endl;
		    target = res;
		  }
		}
		else {
		  //		  cerr << "geval 4 " << endl;
		  xmlNode *res = node_search( target , "rel", "hd" );
		  if ( res ){
		    //		    cerr << "geval 4A " << endl;
		    target = res;
		  }
		}
	      }
	    }
	    store_result( result, SUB_VERB, head_node, target, puncts );
	  }
	  else {
	    //	    cerr << "geval 6 " << endl;
	    xmlNode *res = node_search( *it, "rel", "hd" );
	    if ( res ){
	      store_result( result, SUB_VERB, head_node, res, puncts );
	    }
	    res = node_search( *it, "rel", "cnj" );
	    if ( res ){
	      store_result( result, SUB_VERB, head_node, res, puncts );
	    }
	  }
	}
	else if ( args["rel"] == "obj1" ){
	  if ( !(*it)->children ){
	    xmlNode *target = *it;
	    if ( args["index"] != "" &&
		 args["pos"] == "" && args["cat"] == "" ){
	      vector<xmlNode*> inodes = getIndexNodes( head_node->doc );
	      for ( size_t i=0; i < inodes.size(); ++i ){
		string myindex = getAttribute( inodes[i], "index" );
		if ( args["index"] == myindex ){
		  target = inodes[i];
		  break;
		}
	      }
	      if ( target->children ){
		xmlNode *res = node_search( target , "rel", "cnj" );
		if ( res ){
		  string root = getAttribute( res, "root" );
		  if ( !root.empty() ){
		    target = res;
		  }
		}
		else {
		  xmlNode *res = node_search( target , "rel", "hd" );
		  if ( res ){
		    target = res;
		  }
		}
	      }
	    }
	    store_result( result, OBJ1_VERB, head_node, target, puncts );
	  }
	  else {
	    xmlNode *res = node_search( *it, "rel", "hd" );
	    if ( res ){
	      store_result( result, OBJ1_VERB, head_node, res, puncts );
	    }
	    res = node_search( *it, "rel", "cnj" );
	    if ( res ){
	      store_result( result, OBJ1_VERB, head_node, res, puncts );
	    }
	  }
	}
	else if ( args["rel"] == "obj2" ){
	  if ( !(*it)->children ){
	    xmlNode *target = *it;
	    if ( args["index"] != "" &&
		 args["pos"] == "" && args["cat"] == "" ){
	      vector<xmlNode*> inodes = getIndexNodes( head_node->doc);
	      for ( size_t i=0; i < inodes.size(); ++i ){
		string myindex = getAttribute( inodes[i], "index" );
		if ( args["index"] == myindex ){
		  target = inodes[i];
		  break;
		}
	      }
	      if ( target->children ){
		xmlNode *res = node_search( target , "rel", "cnj" );
		if ( res ){
		  string root = getAttribute( res, "root" );
		  if ( !root.empty() ){
		    target = res;
		  }
		}
		else {
		  xmlNode *res = node_search( target , "rel", "hd" );
		  if ( res ){
		    target = res;
		  }
		}
	      }
	    }
	    store_result( result, OBJ2_VERB, head_node, target, puncts );
	  }
	  else {
	    xmlNode *res = node_search( *it, "rel", "hd" );
	    if ( res ){
	      store_result( result, OBJ2_VERB, head_node, res, puncts );
	    }
	    res = node_search( *it, "rel", "cnj" );
	    if ( res ){
	      store_result( result, OBJ2_VERB, head_node, res, puncts );
	    }
	  }
	}
	else if ( args["rel"] == "vc" ){
	  xmlNode *res = node_search( *it, "rel", "hd" );
	  if ( res ){
	    store_result( result, VERB_VC, head_node, res, puncts );
	  }
	}
	else if ( args["rel"] == "svp" ){
	  if ( args["lcat"] == "part" )
	    store_result( result, VERB_SVP, head_node, *it, puncts );
	}
	else if ( args["rel"] == "predc" ){
	  if ( args["lcat"] == "np" ){
	    store_result( result, VERB_PREDC_N, head_node, *it, puncts );
	  }
	  else if ( args["lcat"] == "ap" ){
	    store_result( result, VERB_PREDC_A, head_node, *it, puncts );
	  }
	  xmlNode *res = node_search( *it, "rel", "hd" );
	  if ( res ){
	    string lcat = getAttribute( res, "lcat" );
	    if ( lcat == "np" ){
	      store_result( result, VERB_PREDC_N, head_node, res, puncts );
	    }
	    else if ( lcat == "ap" ){
	      store_result( result, VERB_PREDC_A, head_node, res, puncts );
	    }
	  }
	}
	else if ( args["rel"] == "mod" ){
	  if ( args["lcat"] == "advp" ){
	    store_result( result, VERB_MOD_BW, head_node, *it, puncts );
	  }
	  else if ( args["lcat"] == "ap" ){
	    store_result( result, VERB_MOD_A, head_node, *it, puncts );
	  }
	  else if ( args["lcat"] == "np" ){
	    store_result( result, VERB_NOUN, head_node, *it, puncts );
	  }
	  xmlNode *res = node_search( *it, "rel", "hd" );
	  if ( res ){
	    string lcat = getAttribute( res, "lcat" );
	    if ( lcat == "advp" ){
	      store_result( result, VERB_MOD_BW, head_node, res, puncts );
	    }
	    else if ( lcat == "ap" ){
	      store_result( result, VERB_MOD_A, head_node, res, puncts );
	    }
	    else if ( lcat == "np" ){
	      store_result( result, VERB_NOUN, head_node, res, puncts );
	    }
	  }
	}
	if ( args["cat"] == "cp" ){
	  xmlNode *res = node_search( *it, "rel", "cmp" );
	  if ( res ){
	    store_result( result, VERB_COMP, head_node, res, puncts );
	  }
	}
	else if ( args["cat"] == "pp" ){
	  xmlNode *res = node_search( *it, "rel", "hd" );
	  if ( res ){
	    store_result( result, VERB_PP, head_node, res, puncts );
	  }
	}
      }
    }
    else if ( head_rel == "hd" && head_pos == "noun" &&
	      getAttribute( head_node->parent, "cat" ) == "np" ){
      vector< xmlNode *> head_siblings = getSibblings( head_node );
      for ( vector< xmlNode *>::const_iterator it=head_siblings.begin();
	    it != head_siblings.end();
	    ++it ){
	KWargs args = getAttributes( *it );
	//	cerr << "bekijk " << args << endl;
	if ( args["rel"] == "det" ){
	  if ( !(*it)->children ){
	    store_result( result, NOUN_DET, head_node, *it, puncts );
	  }
	  else {
	    xmlNode *res = node_search( *it, "rel", "hd" );
	    if ( res ){
	      store_result( result, NOUN_DET, head_node, res, puncts );
	    }
	    res = node_search( *it, "rel", "mpw" );
	    // determiners kunnen voor Alpino net als een onderwerp of lijdend
	    // voorwerp samengesteld zijn uit meerdere woorden...
	    // weet alleen even geen voorbeeld...
	    if ( res ){
	      string root = getAttribute( *it, "root" );
	      if ( !root.empty() )
		store_result( result, NOUN_DET, head_node, res, puncts );
	    }
	  }
	}
	if ( args["rel"] == "vc" ){
	  xmlNode *res = node_search( *it, "rel", "hd" );
	  if ( res ){
	    store_result( result, NOUN_VC, head_node, res, puncts );
	  }
	}
      }
    }
    else if ( head_rel == "hd" && head_pos == "prep"
	      && getAttribute( head_node->parent, "cat" ) == "pp" ){
      vector< xmlNode *> head_siblings = getSibblings( head_node );
      for ( vector< xmlNode *>::const_iterator it=head_siblings.begin();
	    it != head_siblings.end();
	    ++it ){
	KWargs args = getAttributes( *it );
	//	cerr << "bekijk " << args << endl;
	if ( args["rel"] == "obj1" ){
	  if ( !(*it)->children ){
	    store_result( result, PREP_OBJ1, head_node, *it, puncts );
	  }
	  else {
	    xmlNode *res = node_search( *it, "rel", "hd" );
	    if ( res ){
	      store_result( result, PREP_OBJ1, head_node, res, puncts );
	    }
	    res = node_search( *it, "rel", "cnj" );
	    if ( res ){
	      if ( getAttribute( res, "root" ) != "" )
		store_result( result, NOUN_DET, head_node, res, puncts );
	    }
	  }
	}
      }
    }
    else if ( head_rel == "crd" ){
      vector< xmlNode *> head_siblings = getSibblings( head_node );
      for ( vector< xmlNode *>::const_iterator it=head_siblings.begin();
	    it != head_siblings.end();
	    ++it ){
	KWargs args = getAttributes( *it );
	//	cerr << "bekijk " << args << endl;
	if ( args["rel"] == "cnj" ){
	  if ( !(*it)->children ){
	    store_result( result, CRD_CNJ, head_node, *it, puncts );
	  }
	  else {
	    xmlNode *res = node_search( *it, "rel", "hd" );
	    if ( res ){
	      store_result( result, CRD_CNJ, head_node, res, puncts );
	    }
	  }
	}
      }
    }
    else if ( head_rel == "cmp" &&
	      ( head_pos == "comp" || head_pos == "comparative" ) ){
      string word = getAttribute( head_node, "word" );
      if ( word != "te" ){
	vector< xmlNode *> head_siblings = getSibblings( head_node );
	for ( vector< xmlNode *>::const_iterator it=head_siblings.begin();
	      it != head_siblings.end();
	      ++it ){
	  KWargs args = getAttributes( *it );
	  if ( args["rel"] == "body" ){
	    xmlNode *res = node_search( *it, "rel", "hd" );
	    if ( res ){
	      store_result( result, COMP_BODY, head_node, res, puncts );
	    }
	    res = node_search( *it, "rel", "cnj" );
	    if ( res ){
	      store_result( result, COMP_BODY, head_node, res, puncts );
	    }
	  }
	}
      }
    }
  }
  return result;
}

string toString( const WWform& wf ){
  switch ( wf ){
  case PASSIVE_VERB:
    return "passiefww";
    break;
  case MODAL_VERB:
    return "modaalww";
    break;
  case TIME_VERB:
    return "tijdww";
    break;
  case COPULA:
    return "koppelww";
    break;
  case HEAD_VERB:
    return "hoofdww";
    break;
  default:
    return "NoVerb";
  }
}

//#define WW_DEBUG

WWform classifyVerb( const xmlNode *wnode, const string& lemma,
		     string& full_lemma ){
  // classify a Verb.
  // also detect 'splits' like 'bel op' giving 'opbellen'
  full_lemma.clear();
  if ( wnode ){
    vector< xmlNode *> siblinglist = getSibblings( wnode );
#ifdef WW_DEBUG
    cerr << "classify VERB lemma=" << lemma << endl;
#endif
    if ( lemma == "zijn" || lemma == "worden" ){
#ifdef WW_DEBUG
      cerr << "passief? lemma=" << lemma << endl;
      cerr << "attributes: " << getAttributes( wnode ) << endl;
#endif
      string sc = getAttribute( wnode, "sc" );
      if ( sc == "passive" ){
#ifdef WW_DEBUG
	cerr << "sc=\"passive\" ==> resultaat = passiefww" << endl;
#endif
	return PASSIVE_VERB;
      }
    }
    if ( koppels.find( lemma ) != koppels.end() ){
      for ( size_t i=0; i < siblinglist.size(); ++i ){
	KWargs atts = getAttributes( siblinglist[i] );
	if ( atts["rel"] == "predc" ){
	  //	  cerr << "resultaat = koppelww" << endl;
	  return COPULA;
	}
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
	  if ( node ){
	    //	    cerr << "resultaat 1 = hoofdww" << endl;
	    return HEAD_VERB;
	  }
	}
      }
    }
    if ( lemma == "zullen" ){
      return TIME_VERB;
    }
    if ( modals.find( lemma ) != modals.end() ){
      //      cerr << "resultaat = modaalww" << endl;
      return MODAL_VERB;
    }
    if ( lemma == "hebben" ){
      for ( size_t i=0; i < siblinglist.size(); ++i ){
	KWargs atts = getAttributes( siblinglist[i] );
	if ( atts["rel"] == "vc" && (atts["cat"] == "ppart" || atts["cat"] == "inf" ) ){
	  //	  cerr << "resultaat = tijdww" << endl;
	  return TIME_VERB;
	}
      }
      //      cerr << "resultaat 2 = hoofdww" << endl;
      return HEAD_VERB;
    }
    if ( lemma == "zijn" ){
      //      cerr << "resultaat = tijdww" << endl;
      return TIME_VERB;
    }
    //    cerr << "resultaat 3 = hoofdww" << endl;
    for ( vector< xmlNode *>::const_iterator it=siblinglist.begin();
	  it != siblinglist.end();
	  ++it ){
      KWargs args = getAttributes( *it );
      if ( args["rel"] == "svp" ){
	if ( args["lcat"] == "part" ){
	  full_lemma = args["word"] + lemma;
	}
      }
    }
    return HEAD_VERB;
  }
  else {
    //    cerr << "resultaat = NONE" << endl;
    return NO_VERB;
  }
}

int get_d_level( const Sentence *s, xmlDoc *alp ){
  // determine de d-level of a sentence
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
  list<xmlNode *> nodelist = TiCC::FindNodes( alp, "//node" );
  list<xmlNode *>::const_iterator nit = nodelist.begin();
  while ( nit != nodelist.end() ){
    // we kijken of het om een level 6 zin gaat:
    // Zinnen met een betrekkelijke bijzin die het subject modificeert
    //    ("De man, die erg op Pietje leek, zette het op een lopen.")
    // Het onderwerp van de zin is genominaliseerd
    //    ("Het weigeren van Pietje was voor Jantje reden om ermee te stoppen.")
    xmlNode *node = *nit;
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
    ++nit;
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
  nit = nodelist.begin();
  while ( nit != nodelist.end() ){
    // we kijken of het om een level 4 zin gaat
    //  "Non-finite complement with its own understood subject". Kan ik even geen voorbeeld van bedenken :p
    // comparatieven met een object van vergelijking
    //    ("Pietje is groter dan Jantje.")
    KWargs atts = getAttributes( *nit );
    if ( atts["rel"] == "obcomp" )
      return 4;
    ++nit;
  }
  vector<xmlNode*> vcnodes;
  nit = nodelist.begin();
  while ( nit != nodelist.end() ){
    KWargs atts = getAttributes( *nit );
    if ( atts["rel"] == "vc" )
      vcnodes.push_back( *nit );
    ++nit;
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
  nit = nodelist.begin();
  while ( nit != nodelist.end() ){
    // we kijken of het om een level 3 zin gaat
    // Zinnen met een objectsmodificerende betrekkelijke bijzin:
    //    "Ik keek naar de man die de straat overstak."
    // Bijzin als object van het hoofdww:
    //     "Ik wist dat hij boos was"
    // Subject extraposition: zinnen met een uitgesteld onderwerp
    //     "Het verbaast me dat je dat weet."
    //   Kun je in Alpino detecteren met aan het 'sup' label voor een
    //   voorlopig onderwerp
    KWargs atts = getAttributes( *nit );
    //    cerr << "bekijk " << atts << endl;
    if ( atts["rel"] == "mod" && atts["cat"] == "rel" ){
      //      cerr << "case mod/rel " << endl;
      KWargs attsp = getAttributes( (*nit)->parent );
      //      cerr << "bekijk " << attsp << endl;
      if ( attsp["rel"] == "obj1" )
	return 3;
    }
    else if ( atts["pos"] == "verb" ){
      //      cerr << "case VERB " << endl;
      KWargs attsp = getAttributes( (*nit)->parent );
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
    ++nit;
  }

  // < 3
  for ( size_t i=0; i < poslist.size(); ++i ){
    // we kijken of het om een level 2 zin gaat
    // zinnen met nevenschikkingen
    // cerr << "bekijk " << poslist[i] << endl;
    // cerr << "head=" << poslist[i]->feat("head") << endl;
    // cerr << "head=" << poslist[i]->feat("headfeature") << endl;
    string pos = poslist[i]->feat("head");
    if ( pos == "VG" ){
      string cp = poslist[i]->feat("conjtype");
      if ( cp == "neven" )
	return 2;
    }
  }

  // < 2
  nit = nodelist.begin();
  while ( nit != nodelist.end() ){
    // we kijken of het om een level 1 zin gaat
    // Zinnen met een infinitief waarbij infinitief en persoonsvorm hetzelfde
    // onderwerp hebben
    //     ("Pietje vergat zijn haar te kammen.")
    KWargs atts = getAttributes( *nit );
    if ( atts["rel"] == "vc" ){
      //      cerr << "VC node " << atts << endl;
      if ( atts["cat"] == "ti"
	   || atts["cat"] == "oti"
	   || atts["cat"] == "inf" ){
	xmlNode *su_node = node_search( *nit, "rel", "su" );
	if ( su_node ){
	  KWargs atts1 = getAttributes( su_node );
	  //	  cerr << "su node 1 " << atts1 << endl;
	  string node_index = atts1["index"];
	  if ( !node_index.empty() ){
	    vector< xmlNode *> siblinglist = getSibblings( *nit );
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
    ++nit;
  }

  // < 1
  return 0;
}

bool checkImp( const xmlNode *alp_node ){
  // check if this is an Imperative
  vector< xmlNode *> siblings = getSibblings( alp_node );
  bool su_found = false;
  for ( size_t i=0; i < siblings.size(); ++i ){
    string rel = getAttribute( siblings[i], "rel" );
    if ( rel == "su" || rel == "sup" )
      su_found = true;
  }
  return !su_found;
}

bool checkModifier( const xmlNode *alp_node ){
  // check if this node is directly below:
  // - a form AP, PPART, PPRES or INF (adjective or non-conjugated verb)
  // - a type SMAIN or SSUB (conjugated verb), and the node itself is a MOD
  bool modifies = false;
  string rel = getAttribute(alp_node, "rel");
  string p_cat = getAttribute(alp_node->parent, "cat");

  if (p_cat == "ap" || p_cat == "ppart" ||
      p_cat == "ppres" || p_cat == "inf") {
    modifies = true;
  }
  else if (rel == "mod" && (p_cat == "smain" || p_cat == "ssub")) {
    modifies = true;
  }
  return modifies;
}

// Retrieves counts for adjectives and other noun modifiers
void mod_stats( xmlDoc *doc, int& adjNpMod, int& npMod ) {
  adjNpMod = 0;
  npMod = 0;

  list<xmlNode*> npnodes = TiCC::FindNodes(doc, "//node[@cat='np']");
  for (auto& node : npnodes) {
    adjNpMod += TiCC::FindNodes(node, "./node[@rel='mod' and @pos='adj']").size();
    npMod += TiCC::FindNodes(node, "./node[@rel='mod' or @rel='app' or @rel='vc']").size();
  }
}

bool isSmallCnj( const xmlNode *eNode ){
  // determine if this is a 'small' conjunction
  vector< xmlNode *> sl = getSibblings( eNode );
  string pos;
  for ( size_t i=0; i < sl.size(); ++i ){
    //    cerr << "sibbling: " << getAttributes( sl[i] ) << endl;
    if ( sl[i] == eNode )
      continue;
    string the_pos = getAttribute( sl[i], "pos" );
    if ( the_pos.empty() )
      continue;
    if ( the_pos == pos ){
      // cerr << "POS = " << the_pos
      // 	   << " equals previous ==> Small conjunct detected" << endl;
      return true;
    }
    else {
    }
    if ( pos.empty() ){
      pos = the_pos;
    }
  }
  //  cerr << "No small conjunct detected" << endl;
  return false;
}

// Returns adverbial nodes: "mod" or "predm" directly below a verb (or sentence) instance.
list<xmlNode*> getAdverbialNodes( xmlDoc *doc ) {
  string verbs = "|smain|ssub|sv1|inf|ti|ppart|ppresent|";
  return TiCC::FindNodes(doc, "//node[contains('" + verbs + "', concat('|', @cat, '|'))]/node[@rel='mod' or @rel='predm']");
}

// Returns nodes that have the given cat as attribute in the complete xmlDoc.
list<xmlNode*> getNodesByCat( xmlDoc *doc, const string& cat ) {
  return getNodesByCat(xmlDocGetRootElement(doc), cat);
}

// Returns nodes that have the given rel/cat as attribute in the complete xmlDoc.
list<xmlNode*> getNodesByRelCat( xmlDoc *doc, const string& rel, const string& cat ) {
  return getNodesByRelCat(xmlDocGetRootElement(doc), rel, cat);
}

// Returns nodes that have the given cat as attribute, starting from the given xmlNode.
// The cat parameter can start with a "!" to signal that the attribute should NOT be the given cat/rel.
list<xmlNode*> getNodesByCat( xmlNode *node, const string& cat ) {
  string catAttr = cat.at(0) == '!' ? ("@cat!='" + cat.substr(1) + "'") : ("@cat='" + cat + "'");
  return TiCC::FindNodes( node, ".//node[" + catAttr + "]" );
}

// Returns nodes that have the given rel/cat as attribute, starting from the given xmlNode.
// The cat/rel parameters can start with a "!" to signal that the attribute should NOT be the given cat/rel.
list<xmlNode*> getNodesByRelCat( xmlNode *node, const string& rel, const string& cat ) {
  string relAttr = rel.at(0) == '!' ? ("@rel!='" + rel.substr(1) + "'") : ("@rel='" + rel + "'");
  string catAttr = cat.at(0) == '!' ? ("@cat!='" + cat.substr(1) + "'") : ("@cat='" + cat + "'");
  return TiCC::FindNodes( node, ".//node[" + relAttr + " and " + catAttr + "]" );
}

// Returns the id attribute for each xmlNode in the list.
list<string> getNodeIds( list<xmlNode*> nodes ) {
  list<string> ids;
  for (auto& node : nodes)
  {
    ids.push_back(getAttribute(node, "id"));
  }
  return ids;
}

xmlDoc *AlpinoParse( const folia::Sentence *s, const string& dirname ){
  //  parse a FoLiA Sentence into an Alpino tree.
  string txt = folia::UnicodeToUTF8(s->toktext());
  //  cerr << "parse line: " << txt << endl;
  string txtfile = dirname + "parse.txt";
  ofstream os( txtfile.c_str() );
  os << txt;
  os.close();
  string parseCmd = "Alpino -fast -flag treebank " + dirname +
    " end_hook=xml -parse < " + txtfile + " -notk > /dev/null 2>&1";
  // cerr << "run: " << parseCmd << endl;
  int res = system( parseCmd.c_str() );
  if ( res ){
    cerr << "Alpino failed: RES = " << res << endl;
  }
  remove( txtfile.c_str() );
  string xmlfile = dirname + "1.xml";
  xmlDoc *xmldoc = xmlReadFile( xmlfile.c_str(), 0, XML_PARSE_NOBLANKS );
  if ( xmldoc ){
    return xmldoc;
  }
  return 0;
}
