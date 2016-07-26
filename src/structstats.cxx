#include <cmath>
#include <iostream>
#include "libfolia/folia.h"
#include "tscan/Alpino.h"
#include "tscan/cgn.h"
#include "tscan/sem.h"
#include "tscan/intensify.h"
#include "tscan/conn.h"
#include "tscan/general.h"
#include "tscan/situation.h"
#include "tscan/afk.h"
#include "tscan/adverb.h"
#include "tscan/ner.h"
#include "tscan/stats.h"

using namespace std;

structStats::~structStats(){
  vector<basicStats *>::iterator it = sv.begin();
  while ( it != sv.end() ){
    delete( *it );
    ++it;
  }
}

vector<const wordStats*> structStats::collectWords() const {
  vector<const wordStats*> result;
  vector<basicStats *>::const_iterator it = sv.begin();
  while ( it != sv.end() ){
    vector<const wordStats*> tmp = (*it)->collectWords();
    result.insert( result.end(), tmp.begin(), tmp.end() );
    ++it;
  }
  return result;
}

double structStats::getMeanAL() const {
  double sum = 0;
  for ( size_t i=0; i < sv.size(); ++i ){
    double val = sv[i]->get_al_gem();
    if ( !std::isnan(val) ){
      sum += val;
    }
  }
  if ( sum == 0 )
    return NAN;
  else
    return sum/sv.size();
}

double structStats::getHighestAL() const {
  double sum = 0;
  for ( size_t i=0; i < sv.size(); ++i ){
    double val = sv[i]->get_al_max();
    if ( !std::isnan(val) ){
      sum += val;
    }
  }
  if ( sum == 0 )
    return NAN;
  else
    return sum/sv.size();
}
