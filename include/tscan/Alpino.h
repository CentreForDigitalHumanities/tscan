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

#ifndef ALPINO_H
#define ALPINO_H
xmlDoc *AlpinoParse( folia::Sentence * );
xmlNode *getAlpWord( xmlDoc *, const folia::Word * );
std::string classifyVerb( folia::Word *, xmlDoc * );

enum DD_type { SUB_VERB, OBJ1_VERB, OBJ2_VERB, VERB_PP, VERB_VC,
	       VERB_COMP, NOUN_DET, PREP_OBJ1, CRD_CNJ, COMP_BODY, NOUN_VC };
struct ddinfo {
ddinfo( DD_type _t, int _d ):type(_t),dist(_d){};
  DD_type type;
  int dist;
};
std::string toString( const DD_type& );

inline std::ostream& operator<< (std::ostream&os, const DD_type& t ){
  os << toString( t );
  return os;
}

inline std::ostream& operator<< (std::ostream&os, const ddinfo& i ){
  os << toString( i.type ) << ":" << i.dist;
  return os;
}

std::vector<ddinfo> getDependencyDist( folia::Word *, xmlDoc * );
int get_d_level( folia::Sentence *s, xmlDoc *alp );
int indef_npcount( xmlDoc *alp );

#endif // ALPINO_H
