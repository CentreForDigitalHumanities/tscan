/*
  $Id$
  $URL$

  Copyright (c) 1998 - 2014

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

#ifndef ALPINO_H
#define ALPINO_H

enum DD_type { SUB_VERB, OBJ1_VERB, OBJ2_VERB, VERB_PP, VERB_VC,
	       VERB_COMP, NOUN_DET, PREP_OBJ1, CRD_CNJ, COMP_BODY, NOUN_VC,
	       VERB_SVP, VERB_PREDC_N, VERB_PREDC_A, VERB_MOD_BW,
	       VERB_MOD_A, VERB_NOUN };

std::string MMtoString( const std::multimap<DD_type, int>& mm, DD_type t );
std::string MMtoString( const std::multimap<DD_type, int>& mm );
void aggregate( std::multimap<DD_type,int>& out, const std::multimap<DD_type,int>& in );
std::string toString( const DD_type& );
inline std::ostream& operator<< (std::ostream&os, const DD_type& t ){
  os << toString( t );
  return os;
}

enum WWform { NO_VERB, PASSIVE_VERB, MODAL_VERB, TIME_VERB,
	      COPULA, HEAD_VERB };
std::string toString( const WWform& );
inline std::ostream& operator<< (std::ostream&os, const WWform& wf ){
  os << toString( wf );
  return os;
}

xmlDoc *AlpinoParse( const folia::Sentence *, const std::string& );
xmlNode *getAlpNodeWord( xmlDoc *, const folia::Word * );
bool checkImp( const xmlNode * );
bool checkModifier( const xmlNode * );
void countCrdCnj( xmlDoc *, int&, int& );
void mod_stats( xmlDoc *, int&, int& );
int get_d_level( const folia::Sentence *s, xmlDoc *alp );
int indef_npcount( xmlDoc *alp );
WWform classifyVerb( const xmlNode *, const std::string&, std::string& );
std::multimap<DD_type,int> getDependencyDist( const xmlNode *,
					      const std::set<size_t>& );
bool isSmallCnj( const xmlNode *);

std::list<xmlNode*> getAdverbialNodes( xmlDoc* );
std::list<xmlNode*> getNodesByCat( xmlDoc*, const std::string&, const std::string& = "" );
std::list<xmlNode*> getNodesByRelCat( xmlDoc*, const std::string&, const std::string&, const std::string& = "" );
std::list<xmlNode*> getNodesByCat( xmlNode*, const std::string&, const std::string& = "" );
std::list<xmlNode*> getNodesByRelCat( xmlNode*, const std::string&, const std::string&, const std::string& = "" );
std::list<std::string> getNodeIds( std::list<xmlNode*> );

#endif // ALPINO_H
