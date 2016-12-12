#include "tscan/stats.h"

using namespace std;

/*****
 * LSA
 *****/

void parStats::setLSAvalues( double suc, double net, double ctx ) {
  if ( suc > 0 )
    lsa_sent_suc = suc;
  if ( net > 0 )
    lsa_sent_net = net;
  if ( ctx > 0 )
    lsa_sent_ctx = ctx;
}

/**************
 * FOLIA OUTPUT
 **************/

void parStats::addMetrics() const {
  folia::FoliaElement *el = folia_node;
  structStats::addMetrics();
  addOneMetric( el->doc(), el,
		"sentence_count", TiCC::toString(sentCnt) );
}
