#include "tscan/stats.h"

using namespace std;

/****
 * AL
 ****/

double sentStats::getMeanAL() const {
  double result = NAN;
  size_t len = distances.size();
  if ( len > 0 ) {
    result = 0;
    for ( multimap<DD_type, int>::const_iterator pos = distances.begin(); pos != distances.end(); ++pos ) {
      result += pos->second;
    }
    result = result / len;
  }
  return result;
}

double sentStats::getHighestAL() const {
  double result = 0;
  for( multimap<DD_type, int>::const_iterator pos = distances.begin(); pos != distances.end(); ++pos ) {
    if ( pos->second > result )
      result = pos->second;
  }
  return result;
}

/*************
 * CONNECTIVES
 *************/

void sentStats::incrementConnCnt( Conn::Type t ) {
  switch ( t ){
  case Conn::TEMPOREEL:
    tempConnCnt++;
    break;
  case Conn::OPSOMMEND_WG:
    opsomWgConnCnt++;
    break;
  case Conn::OPSOMMEND_ZIN:
    opsomZinConnCnt++;
    break;
  case Conn::CONTRASTIEF:
    contrastConnCnt++;
    break;
  case Conn::COMPARATIEF:
    compConnCnt++;
    break;
  case Conn::CAUSAAL:
    break;
  default:
    break;
  }
}

void sentStats::resolveConnectives() {
  const string neg_longA[] = { "afgezien van", "zomin als", "met uitzondering van"};
  static set<string> negatives_long = set<string>( neg_longA, neg_longA + sizeof(neg_longA)/sizeof(string) );

  if ( sv.size() > 1 ){
    for ( size_t i=0; i < sv.size()-2; ++i ){
      string word = sv[i]->ltext();
      string multiword2 = word + " " + sv[i+1]->ltext();
      if ( !checkAls( i ) ){
	// "als" is speciaal als het matcht met eerdere woorden.
	// (evenmin ... als) (zowel ... als ) etc.
	// In dat geval niet meer zoeken naar "als ..."
	//      cerr << "zoek op " << multiword2 << endl;
	Conn::Type conn = checkMultiConnectives( multiword2 );
	if ( conn != Conn::NOCONN ){
	  sv[i]->setMultiConn();
	  sv[i+1]->setMultiConn();
	  sv[i]->setConnType( conn );
	  sv[i+1]->setConnType( Conn::NOCONN );
	}
      }
      if ( negatives_long.find( multiword2 ) != negatives_long.end() ){
	propNegCnt++;
      }
      string multiword3 = multiword2 + " " + sv[i+2]->ltext();
      //      cerr << "zoek op " << multiword3 << endl;
      Conn::Type conn = checkMultiConnectives( multiword3 );
      if ( conn != Conn::NOCONN ){
	sv[i]->setMultiConn();
	sv[i+1]->setMultiConn();
	sv[i+2]->setMultiConn();
	sv[i]->setConnType( conn );
	sv[i+1]->setConnType( Conn::NOCONN );
	sv[i+2]->setConnType( Conn::NOCONN );
      }
      if ( negatives_long.find( multiword3 ) != negatives_long.end() )
	propNegCnt++;
    }
    // don't forget the last 2 words
    string multiword2 = sv[sv.size()-2]->ltext() + " "
      + sv[sv.size()-1]->ltext();
    //    cerr << "zoek op " << multiword2 << endl;
    Conn::Type conn = checkMultiConnectives( multiword2 );
    if ( conn != Conn::NOCONN ){
      sv[sv.size()-2]->setMultiConn();
      sv[sv.size()-1]->setMultiConn();
      sv[sv.size()-2]->setConnType( conn );
      sv[sv.size()-1]->setConnType( Conn::NOCONN );
    }
    if ( negatives_long.find( multiword2 ) != negatives_long.end() ){
      propNegCnt++;
    }
  }
  for ( size_t i=0; i < sv.size(); ++i ){
    switch( sv[i]->getConnType() ){
    case Conn::TEMPOREEL:
      unique_temp_conn[sv[i]->ltext()]++;
      tempConnCnt++;
      break;
    case Conn::OPSOMMEND_WG:
      unique_reeks_wg_conn[sv[i]->ltext()]++;
      opsomWgConnCnt++;
      break;
    case Conn::OPSOMMEND_ZIN:
      unique_reeks_zin_conn[sv[i]->ltext()]++;
      opsomZinConnCnt++;
      break;
    case Conn::CONTRASTIEF:
      unique_contr_conn[sv[i]->ltext()]++;
      contrastConnCnt++;
      break;
    case Conn::COMPARATIEF:
      unique_comp_conn[sv[i]->ltext()]++;
      compConnCnt++;
      break;
    case Conn::CAUSAAL:
      unique_cause_conn[sv[i]->ltext()]++;
      causeConnCnt++;
      break;
    default:
      break;
    }
  }
}

bool sentStats::checkAls( size_t index ) {
  static string compAlsList[] = { "net", "evenmin", "zo", "zomin" };
  static set<string> compAlsSet( compAlsList,
         compAlsList + sizeof(compAlsList)/sizeof(string) );
  static string opsomAlsList[] = { "zowel" };
  static set<string> opsomAlsSet( opsomAlsList,
          opsomAlsList + sizeof(opsomAlsList)/sizeof(string) );

  string als = sv[index]->ltext();
  if ( als == "als" ){
    if ( index == 0 ){
      // eerste woord, terugkijken kan dus niet
      sv[0]->setConnType( Conn::CAUSAAL );
    }
    else {
      for ( size_t i = index-1; i+1 != 0; --i ){
  string word = sv[i]->ltext();
  if ( compAlsSet.find( word ) != compAlsSet.end() ){
    // kijk naar "evenmin ... als" constructies
    sv[i]->setConnType( Conn::COMPARATIEF );
    sv[index]->setConnType( Conn::COMPARATIEF );
    //  cerr << "ALS comparatief:" << word << endl;
    return true;
  }
  else if ( opsomAlsSet.find( word ) != opsomAlsSet.end() ){
    // kijk naar "zowel ... als" constructies
    sv[i]->setConnType( Conn::OPSOMMEND_WG );
    sv[index]->setConnType( Conn::OPSOMMEND_WG );
    //  cerr << "ALS opsommend:" << word << endl;
    return true;
  }
      }
      if ( sv[index]->postag() == CGN::VG ){
  if ( sv[index-1]->postag() == CGN::ADJ ){
    // "groter als"
    //  cerr << "ALS comparatief: ADJ: " << sv[index-1]->text() << endl;
    sv[index]->setConnType( Conn::COMPARATIEF );
  }
  else {
    //  cerr << "ALS causaal: " << sv[index-1]->text() << endl;
    sv[index]->setConnType( Conn::CAUSAAL );
  }
  return true;
      }
    }
    if ( index < sv.size() &&
   sv[index+1]->postag() == CGN::TW ){
      // "als eerste" "als dertigste"
      sv[index]->setConnType( Conn::COMPARATIEF );
      return true;
    }
  }
  return false;
}

/************
 * SITUATIONS
 ************/

void sentStats::resolveSituations() {
  if ( sv.size() > 1 ){
    for ( size_t i=0; (i+3) < sv.size(); ++i ){
      string word = sv[i]->Lemma();
      string multiword2 = word + " " + sv[i+1]->Lemma();
      string multiword3 = multiword2 + " " + sv[i+2]->Lemma();
      string multiword4 = multiword3 + " " + sv[i+3]->Lemma();
      //      cerr << "zoek 4 op '" << multiword4 << "'" << endl;
      Situation::Type sit = checkMultiSituations( multiword4 );
      if ( sit != Situation::NO_SIT ){
	//	cerr << "found " << sit << "-situation: " << multiword4 << endl;
	sv[i]->setSitType( Situation::NO_SIT );
	sv[i+1]->setSitType( Situation::NO_SIT );
	sv[i+2]->setSitType( Situation::NO_SIT );
	sv[i+3]->setSitType( sit );
	i += 3;
      }
      else {
	//cerr << "zoek 3 op '" << multiword3 << "'" << endl;
	sit = checkMultiSituations( multiword3 );
	if ( sit != Situation::NO_SIT ){
	  // cerr << "found " << sit << "-situation: " << multiword3 << endl;
	  sv[i]->setSitType( Situation::NO_SIT );
	  sv[i+1]->setSitType( Situation::NO_SIT );
	  sv[i+2]->setSitType( sit );
	  i += 2;
	}
	else {
	  //cerr << "zoek 2 op '" << multiword2 << "'" << endl;
	  sit = checkMultiSituations( multiword2 );
	  if ( sit != Situation::NO_SIT ){
	    //	    cerr << "found " << sit << "-situation: " << multiword2 << endl;
	    sv[i]->setSitType( Situation::NO_SIT);
	    sv[i+1]->setSitType( sit );
	    i += 1;
	  }
	}
      }
    }
    // don't forget the last 2 and 3 words
    Situation::Type sit = Situation::NO_SIT;
    if ( sv.size() > 2 ){
      string multiword3 = sv[sv.size()-3]->Lemma() + " "
	+ sv[sv.size()-2]->Lemma() + " " + sv[sv.size()-1]->Lemma();
      //cerr << "zoek final 3 op '" << multiword3 << "'" << endl;
      sit = checkMultiSituations( multiword3 );
      if ( sit != Situation::NO_SIT ){
	//	cerr << "found " << sit << "-situation: " << multiword3 << endl;
	sv[sv.size()-3]->setSitType( Situation::NO_SIT );
	sv[sv.size()-2]->setSitType( Situation::NO_SIT );
	sv[sv.size()-1]->setSitType( sit );
      }
      else {
	string multiword2 = sv[sv.size()-3]->Lemma() + " "
	  + sv[sv.size()-2]->Lemma();
	//cerr << "zoek first final 2 op '" << multiword2 << "'" << endl;
	sit = checkMultiSituations( multiword2 );
	if ( sit != Situation::NO_SIT ){
	  //	  cerr << "found " << sit << "-situation: " << multiword2 << endl;
	  sv[sv.size()-3]->setSitType( Situation::NO_SIT);
	  sv[sv.size()-2]->setSitType( sit );
	}
	else {
	  string multiword2 = sv[sv.size()-2]->Lemma() + " "
	    + sv[sv.size()-1]->Lemma();
	  //cerr << "zoek second final 2 op '" << multiword2 << "'" << endl;
	  sit = checkMultiSituations( multiword2 );
	  if ( sit != Situation::NO_SIT ){
	    //	    cerr << "found " << sit << "-situation: " << multiword2 << endl;
	    sv[sv.size()-2]->setSitType( Situation::NO_SIT);
	    sv[sv.size()-1]->setSitType( sit );
	  }
	}
      }
    }
    else {
      string multiword2 = sv[sv.size()-2]->Lemma() + " "
	+ sv[sv.size()-1]->Lemma();
      // cerr << "zoek second final 2 op '" << multiword2 << "'" << endl;
      sit = checkMultiSituations( multiword2 );
      if ( sit != Situation::NO_SIT ){
	//	cerr << "found " << sit << "-situation: " << multiword2 << endl;
	sv[sv.size()-2]->setSitType( Situation::NO_SIT);
	sv[sv.size()-1]->setSitType( sit );
      }
    }
  }
  for ( size_t i=0; i < sv.size(); ++i ){
    switch( sv[i]->getSitType() ){
    case Situation::TIME_SIT:
      unique_tijd_sits[sv[i]->Lemma()]++;
      timeSitCnt++;
      break;
    case Situation::CAUSAL_SIT:
      unique_cause_sits[sv[i]->Lemma()]++;
      causeSitCnt++;
      break;
    case Situation::SPACE_SIT:
      unique_ruimte_sits[sv[i]->Lemma()]++;
      spaceSitCnt++;
      break;
    case Situation::EMO_SIT:
      unique_emotion_sits[sv[i]->Lemma()]++;
      emoSitCnt++;
      break;
    default:
      break;
    }
  }
}

/*****
 * LSA
 *****/

void sentStats::setLSAvalues( double suc, double net, double ctx ) {
  if ( suc > 0 )
    lsa_word_suc = suc;
  if ( net > 0 )
    lsa_word_net = net;
  if ( ctx > 0 )
    throw logic_error("context cannot be !=0 for sentstats");
}

void sentStats::resolveLSA( const map<string,double>& LSAword_dists ) {
  if ( sv.size() < 1 )
    return;

  int lets = 0;
  double suc = 0;
  double net = 0;
  size_t node_count = 0;
  for ( size_t i=0; i < sv.size(); ++i ){
    double context = 0.0;
    for ( size_t j=0; j < sv.size(); ++j ){
      if ( j == i )
	continue;
      string word1 = sv[i]->ltext();
      string word2 = sv[j]->ltext();
      string call = word1 + "\t" + word2;
      map<string,double>::const_iterator it = LSAword_dists.find(call);
      if ( it != LSAword_dists.end() ){
	context += it->second;
      }
    }
    sv[i]->setLSAcontext(context/(sv.size()-1));
    for ( size_t j=i+1; j < sv.size(); ++j ){
      if ( sv[i]->wordProperty() == CGN::ISLET ){
	continue;
      }
      if ( sv[j]->wordProperty() == CGN::ISLET ){
	if ( i == 0 )
	  ++lets;
	continue;
      }
      ++node_count;
      string word1 = sv[i]->ltext();
      string word2 = sv[j]->ltext();
      string call = word1 + "\t" + word2;
      double result = 0;
      map<string,double>::const_iterator it = LSAword_dists.find(call);
      if ( it != LSAword_dists.end() ){
	result = it->second;
	if ( j == i+1 ){
	  sv[i]->setLSAsuc(result);
	  suc += result;
	}
	net += result;
      }
#ifdef DEBUG_LSA
      cerr << "LSA-sent lookup '" << call << "' ==> " << result << endl;
#endif
    }
  }
#ifdef DEBUG_LSA
  cerr << "size = " << sv.size() << ", LETS = " << lets << endl;
  cerr << "LSA-sent-SUC sum = " << suc << endl;
  cerr << "LSA-sent=NET sum = " << net << ", node count = " << node_count << endl;
#endif
  suc = suc/(sv.size()-lets);
  net = net/node_count;
#ifdef DEBUG_LSA
  cerr << "LSA-sent-SUC result = " << suc << endl;
  cerr << "LSA-sent-NET result = " << net << endl;
#endif
  setLSAvalues( suc, net, 0 );
}

/******************
 * RELATIVE CLAUSES
 ******************/

// Finds nodes of relative clauses and reports counts
void sentStats::resolveRelativeClauses( xmlDoc *alpDoc ) {
  string hasFiniteVerb = "//node[@cat='ssub']";
  string hasDirectFiniteVerb = "/node[@cat='ssub']";
  string hasFiniteVerbSv1 = "//node[@cat='ssub' or @cat='sv1']";
  string hasDirectFiniteVerbSv1 = "/node[@cat='ssub' or @cat='sv1']";

  // Betrekkelijke/bijvoeglijke bijzinnen (zonder/met nevenschikking)
  list<xmlNode*> relNodes = getNodesByRelCat(alpDoc, "mod", "rel", hasFiniteVerb);
  relNodes.merge(getNodesByRelCat(alpDoc, "mod", "whrel", hasFiniteVerb));
  string relConjPath = ".//node[@rel='mod' and @cat='conj']//node[@rel='cnj' and (@cat='rel' or @cat='whrel')]" + hasDirectFiniteVerb;
  relNodes.merge(TiCC::FindNodes(alpDoc, relConjPath));

  // Bijwoordelijke bijzinnen (zonder/met nevenschikking + licht afwijkende bijzinnen)
  list<xmlNode*> cpNodes = getNodesByRelCat(alpDoc, "mod", "cp", hasFiniteVerbSv1);
  string cpConjPath = ".//node[@rel='mod' and @cat='conj']//node[@rel='cnj' and @cat='cp']" + hasDirectFiniteVerbSv1;
  cpNodes.merge(TiCC::FindNodes(alpDoc, cpConjPath));
  string cpNuclAExtra = "(@cat!='cp' or not(descendant::node[@rel='cnj' and @cat='ssub']))";
  string nuclPath = "(following-sibling::node[@rel='nucl'] or preceding-sibling::node[@rel='nucl'])";
  string cpNuclAPath = ".//node[(@cat='sv1' or @cat='cp') and " + nuclPath + " and " + cpNuclAExtra + "]";
  cpNodes.merge(TiCC::FindNodes(alpDoc, cpNuclAPath));
  string cpNuclBPath = ".//node[@rel='sat' and " + nuclPath + "]/node[@rel='cnj' and @cat='sv1']";
  cpNodes.merge(TiCC::FindNodes(alpDoc, cpNuclBPath));
  string cpNuclCPath = ".//node[@rel='sat' and " + nuclPath + "]//node[@rel='cnj' and @cat='ssub']";
  cpNodes.merge(TiCC::FindNodes(alpDoc, cpNuclCPath));

  // Finiete complementszinnen
  // Check whether the previous node is not the top node to prevent clashes with loose clauses below
  string notTop = ".//node[@cat!='top']";
  string complWhsubPath = notTop + "/node[@cat='whsub']" + hasFiniteVerb;
  string complWhrelPath = notTop + "/node[@cat='whrel']" + hasFiniteVerb;
  string complCpPath = notTop + "/node[@rel!='sat' and @cat='cp']" + hasFiniteVerb;
  list<xmlNode*> complNodes = TiCC::FindNodes(alpDoc, complWhsubPath);
  complNodes.merge(complementNodes(TiCC::FindNodes(alpDoc, complWhrelPath), relNodes));
  complNodes.merge(complementNodes(TiCC::FindNodes(alpDoc, complCpPath), cpNodes));

  // Infinietcomplementen
  list<xmlNode*> tiNodes = getNodesByCat(alpDoc, "ti");

  // Save counts
  betrCnt = relNodes.size();
  bijwCnt = cpNodes.size();
  complCnt = complNodes.size();
  infinComplCnt = tiNodes.size();

  // Checks for embedded finite clauses
  list<xmlNode*> allRelNodes (relNodes);
  allRelNodes.merge(cpNodes);
  allRelNodes.merge(complNodes);
  list<string> ids;
  for (auto& node : allRelNodes) {
    list<xmlNode*> embedRelNodes = getNodesByRelCat(node, "mod", "rel", hasFiniteVerb);
    embedRelNodes.merge(getNodesByRelCat(node, "mod", "whrel", hasFiniteVerb));
    embedRelNodes.merge(TiCC::FindNodes(node, relConjPath));
    ids.merge(getNodeIds(embedRelNodes));

    list<xmlNode*> embedCpNodes = getNodesByRelCat(node, "mod", "cp", hasFiniteVerbSv1);
    embedCpNodes.merge(TiCC::FindNodes(node, cpConjPath));
    embedCpNodes.merge(TiCC::FindNodes(node, cpNuclAPath));
    embedCpNodes.merge(TiCC::FindNodes(node, cpNuclBPath));
    embedCpNodes.merge(TiCC::FindNodes(node, cpNuclCPath));
    ids.merge(getNodeIds(embedCpNodes));

    ids.merge(getNodeIds(TiCC::FindNodes(node, complWhsubPath)));
    ids.merge(getNodeIds(complementNodes(TiCC::FindNodes(node, complWhrelPath), embedRelNodes)));
    ids.merge(getNodeIds(complementNodes(TiCC::FindNodes(node, complCpPath), embedCpNodes)));
  }
  set<string> mvFinEmbedIds(ids.begin(), ids.end());
  mvFinInbedCnt = mvFinEmbedIds.size();

  // Checks for all embedded clauses
  allRelNodes.merge(tiNodes);
  ids.clear();
  for (auto& node : allRelNodes) {
    list<xmlNode*> embedRelNodes = getNodesByRelCat(node, "mod", "rel", hasFiniteVerb);
    embedRelNodes.merge(getNodesByRelCat(node, "mod", "whrel", hasFiniteVerb));
    embedRelNodes.merge(TiCC::FindNodes(node, relConjPath));
    ids.merge(getNodeIds(embedRelNodes));

    list<xmlNode*> embedCpNodes = getNodesByRelCat(node, "mod", "cp", hasFiniteVerbSv1);
    embedCpNodes.merge(TiCC::FindNodes(node, cpConjPath));
    embedCpNodes.merge(TiCC::FindNodes(node, cpNuclAPath));
    embedCpNodes.merge(TiCC::FindNodes(node, cpNuclBPath));
    embedCpNodes.merge(TiCC::FindNodes(node, cpNuclCPath));
    ids.merge(getNodeIds(embedCpNodes));

    ids.merge(getNodeIds(TiCC::FindNodes(node, complWhsubPath)));
    ids.merge(getNodeIds(complementNodes(TiCC::FindNodes(node, complWhrelPath), embedRelNodes)));
    ids.merge(getNodeIds(complementNodes(TiCC::FindNodes(node, complCpPath), embedCpNodes)));

    ids.merge(getNodeIds(getNodesByCat(node, "ti")));
  }
  set<string> mvInbedIds(ids.begin(), ids.end());
  mvInbedCnt = mvInbedIds.size();

  // Count 'loose' (directly under top node) relative clauses
  string losBetr = "//node[@cat='top']/node[@cat='rel' or @cat='whrel']" + hasFiniteVerb;
  losBetrCnt = TiCC::FindNodes(alpDoc, losBetr).size();
  string losBijw = "//node[@cat='top']/node[@cat='cp']" + hasFiniteVerb;
  losBijwCnt = TiCC::FindNodes(alpDoc, losBijw).size();
}

/**************
 * FINITE VERBS
 **************/

// Finds nodes of finite verbs and reports counts
void sentStats::resolveFiniteVerbs( xmlDoc *alpDoc ) {
  smainCnt = getNodesByCat(alpDoc, "smain").size();
  ssubCnt = getNodesByCat(alpDoc, "ssub").size();
  sv1Cnt = getNodesByCat(alpDoc, "sv1").size();

  clauseCnt = smainCnt + ssubCnt + sv1Cnt;
  correctedClauseCnt = clauseCnt > 0 ? clauseCnt : 1; // Correct clause count to 1 if there are no verbs in the sentence
}

/**************
 * CONJUNCTIONS
 **************/

// Finds nodes of coordinating conjunctions and reports counts
void sentStats::resolveConjunctions( xmlDoc *alpDoc ) {
  smainCnjCnt = getNodesByRelCat(alpDoc, "cnj", "smain").size();
  // For cnj-ssub, also allow that the cnj node dominates the ssub node
  ssubCnjCnt = TiCC::FindNodes(alpDoc, ".//node[@rel='cnj'][descendant-or-self::node[@cat='ssub']]").size();
  sv1CnjCnt = getNodesByRelCat(alpDoc, "cnj", "sv1").size();
}

// Finds nodes of small conjunctions and reports counts
void sentStats::resolveSmallConjunctions( xmlDoc *alpDoc ) {
  // Small conjunctions have 'cnj' as relation and do not form a "bigger" sentence
  string cats = "|smain|ssub|sv1|rel|whrel|cp|oti|ti|whsub|";
  string smallCnjPath = ".//node[@rel='cnj' and not(contains('" + cats + "', concat('|', @cat, '|')))]";
  smallCnjCnt = TiCC::FindNodes(alpDoc, smallCnjPath).size();

  // smallCnjExtraCnt count elements that have 'conj' as a category and do not govern a "bigger" sentence
  // This amount is then substracted from the number of small conjunctions.
  string smallCnjExtraPath = ".//node[@cat='conj' and not(descendant::node[contains('" + cats + "', concat('|', @cat, '|'))])]";
  smallCnjExtraCnt = smallCnjCnt - TiCC::FindNodes(alpDoc, smallCnjExtraPath).size();
}

/**************
 * FOLIA OUTPUT
 **************/

void sentStats::addMetrics() const {
  structStats::addMetrics();
  folia::FoliaElement *el = folia_node;
  folia::Document *doc = el->doc();
  if ( passiveCnt > 0 )
    addOneMetric( doc, el, "isPassive", "true" );
  if ( questCnt > 0 )
    addOneMetric( doc, el, "isQuestion", "true" );
  if ( impCnt > 0 )
    addOneMetric( doc, el, "isImperative", "true" );
}
