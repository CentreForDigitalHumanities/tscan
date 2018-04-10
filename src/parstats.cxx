#include "tscan/stats.h"

using namespace std;

/**************
 * FOLIA OUTPUT
 **************/

void parStats::addMetrics() const {
  folia::FoliaElement *el = folia_node;
  structStats::addMetrics();
  addOneMetric( el->doc(), el,
		"sentence_count", TiCC::toString(sentCnt) );
}
