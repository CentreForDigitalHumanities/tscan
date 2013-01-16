/*
  $Id$
  $URL$

  Copyright (c) 1998 - 2013
 
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
xmlDoc *AlpinoParse( folia::Sentence * );
xmlNode *getAlpWord( xmlDoc *, const folia::Word * );
bool checkImp( folia::Word *, xmlDoc * );
void countCrdCnj( xmlDoc *, int&, int&, int& );
std::string classifyVerb( folia::Word *, xmlDoc * );

void countCrdCnj( xmlDoc *, int& , int&, int& );
void mod_stats( xmlDoc *, int&, int&, int& );

bool checkImp( folia::Word *, xmlDoc * );

enum DD_type { SUB_VERB, OBJ1_VERB, OBJ2_VERB, VERB_PP, VERB_VC,
	       VERB_COMP, NOUN_DET, PREP_OBJ1, CRD_CNJ, COMP_BODY, NOUN_VC };

std::string toString( const DD_type& );

inline std::ostream& operator<< (std::ostream&os, const DD_type& t ){
  os << toString( t );
  return os;
}


std::multimap<DD_type,int> getDependencyDist( folia::Word *, xmlDoc *, 
					      const std::set<size_t>& );
int get_d_level( folia::Sentence *s, xmlDoc *alp );
int indef_npcount( xmlDoc *alp );

#endif // ALPINO_H
