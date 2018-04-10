#include "tscan/stats.h"

using namespace std;

vector<const wordStats*> wordStats::collectWords() const {
  vector<const wordStats*> result;
  result.push_back( this );
  return result;
}

bool wordStats::setPersRef() {
  return ( sem_type == SEM::CONCRETE_HUMAN_NOUN ||
       nerProp == NER::PER_B ||
       prop == CGN::ISPPRON1 || prop == CGN::ISPPRON2 || prop == CGN::ISPPRON3 );
}

bool wordStats::checkContent( bool strict ) const {
  // Head verbs are content words
  if ( tag == CGN::WW ){
    if ( wwform == HEAD_VERB ){
      return true;
    }
  }
  // In strict mode, only adverbs of the subtype "Manner" are content words
  else if ( tag == CGN::BW ) {
    return !strict || Adverb::isContent(adverb_sub_type);
  }
  // Names, nouns and adjectives are content words
  else {
    return ( prop == CGN::ISNAME || tag == CGN::N || tag == CGN::ADJ );
  }
  return false;
}

/****************
 * NOMINALISATION
 ****************/

bool match_tail( const string& word, const string& tail ){
  //  cerr << "match tail " << word << " tail=" << tail << endl;
  if ( tail.size() > word.size() ){
    //    cerr << "match tail failed" << endl;
    return false;
  }
  string::const_reverse_iterator wir = word.rbegin();
  string::const_reverse_iterator tir = tail.rbegin();
  while ( tir != tail.rend() ){
    if ( *tir != *wir ){
      //      cerr << "match tail failed" << endl;
      return false;
    }
    ++tir;
    ++wir;
  }
  //  cerr << "match tail succes" << endl;
  return true;
}

//#define DEBUG_NOMINAL

bool wordStats::checkNominal( const xmlNode *alpWord ) const {
  static string morphList[] = { "ing", "sel", "nis", "enis", "heid", "te",
				"schap", "dom", "sie", "ie", "iek", "iteit",
				"isme", "age", "atie", "esse",	"name" };
  static set<string> morphs( morphList,
			     morphList + sizeof(morphList)/sizeof(string) );
#ifdef DEBUG_NOMINAL
  cerr << "check Nominal " << word << " tag=" << tag << " morphemes=" << morphemes << endl;
#endif
  if ( tag == CGN::N && morphemes.size() > 1 ){
    // morphemes.size() > 1 check mijdt false hits voor "dom", "schap".
    string last_morph = morphemes[morphemes.size()-1];
#ifdef DEBUG_NOMINAL
    cerr << "check morphemes, last= " << last_morph << endl;
#endif
    if ( last_morph == "en" || last_morph == "s" || last_morph == "n" ) {
      last_morph =  morphemes[morphemes.size()-2];
    }
#ifdef DEBUG_NOMINAL
    cerr << "check morphemes, last= " << last_morph << endl;
#endif
    if ( morphs.find( last_morph ) != morphs.end() ){
#ifdef DEBUG_NOMINAL
      cerr << "check Nominal, MATCHED morpheme " << last_morph << endl;
#endif
      return true;
    }

    if ( last_morph.size() > 4 ){
      // avoid false positives for words like oase, base, rose, fase
      bool matched = match_tail( last_morph, "ose" ) ||
	match_tail( last_morph, "ase" ) ||
	match_tail( last_morph, "ese" ) ||
	match_tail( last_morph, "isme" ) ||
	match_tail( last_morph, "sie" ) ||
	match_tail( last_morph, "tie" );
      if ( matched ){
#ifdef DEBUG_NOMINAL
	cerr << "check Nominal, MATCHED tail of morpheme " << last_morph << endl;
#endif
	return true;
      }
    }
  }
  if ( morphemes.size() < 2 && word.size() > 4 ){
    bool matched = match_tail( word, "ose" ) ||
      match_tail( word, "ase" ) ||
      match_tail( word, "ese" ) ||
      match_tail( word, "isme" ) ||
      match_tail( word, "sie" ) ||
      match_tail( word, "tie" );
    if (matched ){
#ifdef DEBUG_NOMINAL
      cerr << "check Nominal, MATCHED tail " <<  word << endl;
#endif
      return true;
    }
  }

  string pos = TiCC::getAttribute( alpWord, "pos" );
  if ( pos == "verb" ){
    // Alpino heeft de voor dit feature prettige eigenschap dat het nogal
    // eens nominalisaties wil taggen als werkwoord dat onder een
    // NP knoop hangt
    alpWord = alpWord->parent;
    string cat = TiCC::getAttribute( alpWord, "cat" );
    if ( cat == "np" ){
#ifdef DEBUG_NOMINAL
      cerr << "Alpino says NOMINAL!" << endl;
#endif
      return true;
    }
  }
#ifdef DEBUG_NOMINAL
  cerr << "check Nominal. NO WAY" << endl;
#endif
  return false;
}

/**********
 * CGNProps
 **********/

void wordStats::setCGNProps( const folia::PosAnnotation* pa ) {
  if ( tag == CGN::LET )
    prop = CGN::ISLET;
  else if ( tag == CGN::SPEC && pos.find("eigen") != string::npos )
    prop = CGN::ISNAME;
  else if ( tag == CGN::WW ){
    string wvorm = pa->feat("wvorm");
    if ( wvorm == "inf" ){
      prop = CGN::ISINF;
      string pos = pa->feat("positie");
      if ( pos == "vrij" ){
	position = CGN::VRIJ;
      }
      else if ( pos == "prenom" ){
	position = CGN::PRENOM;
      }
      else if ( pos == "nom" ){
	position = CGN::NOMIN;
      }
    }
    else if ( wvorm == "vd" ){
      prop = CGN::ISVD;
      string pos = pa->feat("positie");
      if ( pos == "vrij" ){
	position = CGN::VRIJ;
      }
      else if ( pos == "prenom" ){
	position = CGN::PRENOM;
      }
      else if ( pos == "nom" ){
	position = CGN::NOMIN;
      }
    }
    else if ( wvorm == "od" ){
      prop = CGN::ISOD;
      string pos = pa->feat("positie");
      if ( pos == "vrij" ){
	position = CGN::VRIJ;
      }
      else if ( pos == "prenom" ){
	position = CGN::PRENOM;
      }
      else if ( pos == "nom" ){
	position = CGN::NOMIN;
      }
    }
    else if ( wvorm == "pv" ){
      string tijd = pa->feat("pvtijd");
      if ( tijd == "tgw" )
	prop = CGN::ISPVTGW;
      else if ( tijd == "verl" )
	prop = CGN::ISPVVERL;
      else if ( tijd == "conj" )
	prop = CGN::ISSUBJ;
      else {
	cerr << "cgnProps: een onverwachte ww tijd: " << tijd << endl;
      }
    }
    else if ( wvorm == "" ){
      // probably WW(dial)
    }
    else {
      cerr << "cgnProps: een onverwachte ww vorm: " << wvorm << endl;
    }
  }
  else if ( tag == CGN::VNW ){
    string vwtype = pa->feat("vwtype");
    isBetr = vwtype == "betr";
    if ( l_word != "men"
	 && l_word != "er"
	 && l_word != "het" ){
      string cas = pa->feat("naamval");
      archaic = ( cas == "gen" || cas == "dat" );
      if ( vwtype == "pers" || vwtype == "refl"
	   || vwtype == "pr" || vwtype == "bez" ) {
	string persoon = pa->feat("persoon");
	if ( !persoon.empty() ){
	  if ( persoon[0] == '1' )
	    prop = CGN::ISPPRON1;
	  else if ( persoon[0] == '2' )
	    prop = CGN::ISPPRON2;
	  else if ( persoon[0] == '3' ){
	    prop = CGN::ISPPRON3;
	    isPronRef = ( vwtype == "pers" || vwtype == "bez" );
	  }
	  else {
	    cerr << "cgnProps: een onverwachte PRONOUN persoon : " << persoon
		 << " for word " << word << endl;
	  }
	}
      }
      else if ( vwtype == "aanw" ){
	prop = CGN::ISAANW;
	isPronRef = true;
      }
    }
  }
  else if ( tag == CGN::LID ) {
    string cas = pa->feat("naamval");
    archaic = ( cas == "gen" || cas == "dat" );
  }
  else if ( tag == CGN::VG ) {
    string cp = pa->feat("conjtype");
    isOnder = cp == "onder";
  }
}

/**********
 * NEGATION
 **********/

const string negativesA[] = { "geeneens", "geenszins", "kwijt", "nergens",
			      "niet", "niets", "nooit", "allerminst",
			      "allesbehalve", "amper", "behalve", "contra",
			      "evenmin", "geen", "generlei", "nauwelijks",
			      "niemand", "niemendal", "nihil", "niks", "nimmer",
			      "nimmermeer", "noch", "ongeacht", "slechts",
			      "tenzij", "ternauwernood", "uitgezonderd",
			      "weinig", "zelden", "zeldzaam", "zonder" };
static set<string> negatives = set<string>( negativesA,
					    negativesA + sizeof(negativesA)/sizeof(string) );

const string negmorphA[] = { "mis", "de", "non", "on" };
static set<string> negmorphs = set<string>( negmorphA,
					    negmorphA + sizeof(negmorphA)/sizeof(string) );

const string negminusA[] = { "mis-", "non-", "niet-", "anti-",
			     "ex-", "on-", "oud-" };
static vector<string> negminus = vector<string>( negminusA,
						 negminusA + sizeof(negminusA)/sizeof(string) );

bool wordStats::checkPropNeg() const {
  if ( negatives.find( l_word ) != negatives.end() ){
    return true;
  }
  else if ( tag == CGN::BW &&
	    ( l_word == "moeilijk" || l_word == "weg" ) ){
    // "moeilijk" en "weg" mochten kennelijk alleen als bijwoord worden
    // meegeteld (in het geval van weg natuurlijk duidelijk ivm "de weg")
    return true;
  }
  return false;
}

bool wordStats::checkMorphNeg() const {
  string m1 = morphemes[0];
  string m2;
  if ( morphemes.size() > 1 ){
    m2 = morphemes[1];
  }
  if ( negmorphs.find( m1 ) != negmorphs.end() && m2 != "en" && !m2.empty() ){
    // dit om gevallen als "nonnen" niet mee te rekenen
    return true;
  }
  else {
    for ( size_t i=0; i < negminus.size(); ++i ){
      if ( word.find( negminus[i] ) != string::npos )
	return true;
    }
  }
  return false;
}

/*********
 * Overlap
 *********/

//#define DEBUG_OL

bool wordStats::isOverlapCandidate() const {
  if ( ( tag == CGN::VNW && prop != CGN::ISAANW ) ||
       ( tag == CGN::N ) ||
       ( prop == CGN::ISNAME ) ||
       ( tag == CGN::WW && wwform == HEAD_VERB ) ){
    return true;
  }
  else {
#ifdef DEBUG_OL
    if ( tag == CGN::WW ){
      cerr << "WW overlapcandidate REJECTED " << toString(wwform) << " " << word << endl;
    }
    else if ( tag == CGN::VNW ){
      cerr << "VNW overlapcandidate REJECTED " << toString(prop) << " " << word << endl;
    }
#endif
    return false;
  }
}

void wordStats::getSentenceOverlap( const vector<string>& wordbuffer,
            const vector<string>& lemmabuffer ){
  if ( isOverlapCandidate() ){
    // get the words and lemmas' of the previous sentence
#ifdef DEBUG_OL
    cerr << "call word sentenceOverlap, word = " << l_word;
    int tmp2 = wordOverlapCnt;
#endif
    argument_overlap( l_word, wordbuffer, wordOverlapCnt );
#ifdef DEBUG_OL
    if ( tmp2 != wordOverlapCnt ){
      cerr << " OVERLAPPED " << endl;
    }
    else
      cerr << endl;
    cerr << "call lemma sentenceOverlap, lemma= " << l_lemma;
    tmp2 = lemmaOverlapCnt;
#endif
    argument_overlap( l_lemma, lemmabuffer, lemmaOverlapCnt );
#ifdef DEBUG_OL
    if ( tmp2 != lemmaOverlapCnt ){
      cerr << " OVERLAPPED " << endl;
    }
    else
      cerr << endl;
#endif
  }
}

/************
 * CSV OUTPUT
 ************/

/**
 * Adds cnt times "NA," to the outputstream
 * @param os  the current outputstream
 * @param cnt the number of times to print "NA,"
 */
void na( ostream& os, int cnt ){
  for ( int i=0; i < cnt; ++i ){
    os << "NA,";
  }
  os << endl;
}

/**
 * Sets all headers of the wordStats .csv-output.
 */
void wordStats::CSVheader( ostream& os, const string& ) const {
  wordSortHeader( os );
  wordDifficultiesHeader( os );
  coherenceHeader( os );
  concreetHeader( os );
  compoundHeader( os );
  persoonlijkheidHeader( os );
  miscHeader( os );
  os << endl;
}

/**
 * Sets all .csv-output for wordStats.
 * @param os the current outputstream
 */
void wordStats::toCSV( ostream& os ) const {
  wordSortToCSV( os );
  if ( parseFail )
    return;
  wordDifficultiesToCSV( os );
  coherenceToCSV( os );
  concreetToCSV( os );
  compoundToCSV( os );
  persoonlijkheidToCSV( os );
  miscToCSV( os );
  os << endl;
}

void wordStats::wordSortHeader( ostream& os ) const {
  os << "InputFile,Segment,Woord,lemma,Voll_lemma,morfemen,Samenst_delen_Frog,Wrdsoort,Afk,";
}

void wordStats::wordSortToCSV( ostream& os ) const {
  os << id << ",";
  os << '"' << word << "\",";
  if ( parseFail ){
    na( os, 56 );
    return;
  }
  os << '"' << lemma << "\",";
  os << '"' << full_lemma << "\",";

  // Morphemes
  os << '"';
  for( size_t i=0; i < morphemes.size(); ++i ){
    os << "[" << morphemes[i] << "]";
  }
  os << "\",";

  os << (!compstr.empty() ? compstr : "-") << ",";

  os << tag << ",";
  if ( afkType == Afk::NO_A ) {
    os << "0,";
  }
  else {
    os << afkType << ",";
  }
}

void wordStats::wordDifficultiesHeader( ostream& os ) const {
  os << "Let_per_wrd,Wrd_per_let,Let_per_wrd_zn,Wrd_per_let_zn,"
     << "Morf_per_wrd,Wrd_per_morf,Morf_per_wrd_zn,Wrd_per_morf_zn,"
     << "Wrd_prev,Wrd_prev_z,"
     << "Wrd_freq_log,Wrd_freq_zn_log,"
     << "Wrd_freq_log_corr,Wrd_freq_zn_log_corr,"
     << "Lem_freq_log,Lem_freq_zn_log,"
     << "Freq1000,Freq2000,Freq3000,Freq5000,Freq10000,Freq20000,";
}

void wordStats::wordDifficultiesToCSV( ostream& os ) const {
  os << std::showpoint
     << double(charCnt) << ","
     << 1.0/double(charCnt) <<  ",";
  if ( prop == CGN::ISNAME ){
    os << "NA,NA,";
  }
  else {
    os << double(charCnt) << "," << 1.0/double(charCnt) <<  ",";
  }
  if ( morphCnt == 0 ){
    // LET() zaken o.a.
    os << 1.0 << "," << 1.0 << ",";
  }
  else {
    os << double(morphCnt) << ","
       << 1.0/double(morphCnt) << ",";
  }
  if ( prop == CGN::ISNAME ){
    os << "NA,NA,";
  }
  else {
    if ( morphCnt == 0 ){
      os << 1.0 << "," << 1.0 << ",";
    }
    else {
      os << double(morphCnt) << ","
	 << 1.0/double(morphCnt) << ",";
    }
  }

  if ( std::isnan(prevalenceP) )
    os << "NA,";
  else
    os << prevalenceP << ",";
  if ( std::isnan(prevalenceZ) )
    os << "NA,";
  else
    os << prevalenceZ << ",";

  if ( std::isnan(word_freq_log) )
    os << "NA,";
  else
    os << word_freq_log << ",";
  if ( prop == CGN::ISNAME || std::isnan(word_freq_log) )
    os << "NA" << ",";
  else
    os << word_freq_log << ",";

  if ( std::isnan(word_freq_log_corr) )
    os << "NA,";
  else
    os << word_freq_log_corr << ",";
  if ( prop == CGN::ISNAME || std::isnan(word_freq_log_corr) )
    os << "NA" << ",";
  else
    os << word_freq_log_corr << ",";

  if ( std::isnan(lemma_freq_log) )
    os << "NA,";
  else
    os << lemma_freq_log << ",";
  if ( prop == CGN::ISNAME || std::isnan(lemma_freq_log) )
    os << "NA" << ",";
  else
    os << lemma_freq_log << ",";
  os << (top_freq==top1000?1:0) << ",";
  os << (top_freq<=top2000?1:0) << ",";
  os << (top_freq<=top3000?1:0) << ",";
  os << (top_freq<=top5000?1:0) << ",";
  os << (top_freq<=top10000?1:0) << ",";
  os << (top_freq<=top20000?1:0) << ",";
}

void wordStats::coherenceHeader( ostream& os ) const {
  os << "Conn_type,Conn_combi,Vnw_ref,";
}

void wordStats::coherenceToCSV( ostream& os ) const {
  if ( connType == Conn::NOCONN )
    os << "0,";
  else
    os << connType << ",";
  os << isMultiConn << ","
     << isPronRef << ",";
}

void wordStats::concreetHeader( ostream& os ) const {
  os << "Semtype_nw,";
  os << "Alg_nw,"; // 20150721: Feature added
  os << "Conc_nw_strikt,";
  os << "Conc_nw_ruim,";
  os << "Semtype_bvnw,";
  os << "Conc_bvnw_strikt,";
  os << "Conc_bvnw_ruim,";
  os << "Semtype_ww,";
  os << "Alg_ww,"; // 20150821: Feature added
  os << "Semtype_bw,"; // 20150821: Feature added
}

void wordStats::concreetToCSV( ostream& os ) const {
  if ( tag == CGN::N || prop == CGN::ISNAME ) {
    os << sem_type << ",";
  }
  else {
    os << "0,";
  }
  if ( tag == CGN::N ) {
    os << general_noun_type << ",";
  }
  else {
    os << "0,";
  }
  os << SEM::isStrictNoun(sem_type) << "," << SEM::isBroadNoun(sem_type) << ",";
  if ( tag == CGN::ADJ ) {
    os << sem_type << ",";
  }
  else {
    os << "0,";
  }
  os << SEM::isStrictAdj(sem_type) << "," << SEM::isBroadAdj(sem_type) << ",";
  if ( tag == CGN::WW ) {
    os << sem_type << ",";
    os << general_verb_type << ",";
  }
  else {
    os << "0,";
    os << "0,";
  }
  if ( tag == CGN::BW ) {
    os << adverb_type << ",";
  }
  else {
    os << "0,";
  }
}

void wordStats::compoundHeader( ostream& os ) const {
  os << "Samenst,Samenst_delen,";
  os << "Let_per_wrd_hfdwrd,Let_per_wrd_satwrd,";
  os << "Wrd_freq_log_hfdwrd,Wrd_freq_log_satwrd,Wrd_freq_log_(hfd_sat),";
  os << "Freq1000_hfdwrd,Freq5000_hfdwrd,Freq20000_hfdwrd,";
  os << "Freq1000_satwrd,Freq5000_satwrd,Freq20000_satwrd,";
  os << "Samenst_Frog,";
}

void wordStats::compoundToCSV( ostream& os ) const {
  os << (is_compound ? 1 : 0) << ",";
  if (is_compound) {
    os << double(compound_parts) << ",";
    os << double(charCntHead) << ",";
    os << double(charCntSat) << ",";
    if ( std::isnan(word_freq_log_head) )
      os << "NA,";
    else
      os << word_freq_log_head << ",";
    if ( std::isnan(word_freq_log_sat) )
      os << "NA,";
    else
      os << word_freq_log_sat << ",";
    if ( std::isnan(word_freq_log_head_sat) )
      os << "NA,";
    else
      os << word_freq_log_head_sat << ",";
    os << (top_freq_head == top1000 ? 1 : 0) << ",";
    os << (top_freq_head <= top5000 ? 1 : 0) << ",";
    os << (top_freq_head <= top20000 ? 1 : 0) << ",";
    os << (top_freq_sat == top1000 ? 1 : 0) << ",";
    os << (top_freq_sat <= top5000 ? 1 : 0) << ",";
    os << (top_freq_sat <= top20000 ? 1 : 0) << ",";
  }
  else {
    // For non-compounds, just print not applicable for each attribute
    for (int i = 0; i < 12; i++) {
      os << "NA,";
    }
  }

  os << (!compstr.empty() ? 1 : 0) << ",";
}

void wordStats::persoonlijkheidHeader( ostream& os ) const {
  os << "Pers_ref,Pers_vnw1,Pers_vnw2,Pers_vnw3,Pers_vnw,"
     << "Naam_POS,Naam_NER," // 20141125: Feature Naam_POS moved
     << "Imp_ellips,"; // 20141125: Feature Pers_nw moved (deleted 20150703) and Emo_bvn deleted
}

void wordStats::persoonlijkheidToCSV( ostream& os ) const {
  os << isPersRef << ","
     << (prop == CGN::ISPPRON1 ) << ","
     << (prop == CGN::ISPPRON2 ) << ","
     << (prop == CGN::ISPPRON3 ) << ","
     << (prop == CGN::ISPPRON1 || prop == CGN::ISPPRON2 || prop == CGN::ISPPRON3) << ",";
  os << (prop == CGN::ISNAME) << ",";
  if ( nerProp == NER::NONER )
    os << "0,";
  else
    os << nerProp << ",";
  os << isImperative << ",";
}


void wordStats::miscHeader( ostream& os ) const {
  os << "Ww_vorm,Ww_tt,Vol_dw,Onvol_dw,Infin,Archaisch,Log_prob_fwd,Log_prob_bwd,Intens,Op_stoplijst,Eigen_classificatie";
}

void wordStats::miscToCSV( ostream& os ) const {
  if ( wwform == ::NO_VERB ){
    os << "0,";
  }
  else {
    os << wwform << ",";
  }
  os << (prop == CGN::ISPVTGW) << ",";
  os << (prop == CGN::ISVD?toString(position):"0") << ","
     << (prop == CGN::ISOD?toString(position):"0") << ","
     << (prop == CGN::ISINF?toString(position):"0") << ",";
  os << archaic << ",";
  if ( std::isnan(logprob10_fwd) )
    os << "NA,";
  else
    os << logprob10_fwd << ",";
  if ( std::isnan(logprob10_bwd) )
    os << "NA,";
  else
    os << logprob10_bwd << ",";
  os << (intensify_type != Intensify::NO_INTENSIFY ? "1," : "0,");
  os << (on_stoplist ? "1" : "0") << ",";
  os << my_classification << ",";
}

/**************
 * FOLIA OUTPUT
 **************/

/**
 * Add Metrics to a FoLiA Document.
 */
void wordStats::addMetrics( ) const {
  folia::FoliaElement *el = folia_node;
  folia::Document *doc = el->doc();
  if ( wwform != ::NO_VERB ){
    folia::KWargs args;
    args["set"] = "tscan-set";
    args["class"] = "wwform(" + toString(wwform) + ")";
    el->addPosAnnotation( args );
  }
  if ( !full_lemma.empty() ){
    addOneMetric( doc, el, "full-lemma", full_lemma );
  }
  if ( isPersRef )
    addOneMetric( doc, el, "pers_ref", "true" );
  if ( isPronRef )
    addOneMetric( doc, el, "pron_ref", "true" );
  if ( archaic )
    addOneMetric( doc, el, "archaic", "true" );
  if ( isContent )
    addOneMetric( doc, el, "content_word", "true" );
  if ( isContentStrict )
    addOneMetric( doc, el, "content_word_strict", "true" );
  if ( isNominal )
    addOneMetric( doc, el, "nominalization", "true" );
  if ( isOnder )
    addOneMetric( doc, el, "subordinate", "true" );
  if ( isImperative )
    addOneMetric( doc, el, "imperative", "true" );
  if ( isBetr )
    addOneMetric( doc, el, "betrekkelijk", "true" );
  if ( isPropNeg )
    addOneMetric( doc, el, "proper_negative", "true" );
  if ( isMorphNeg )
    addOneMetric( doc, el, "morph_negative", "true" );
  if ( connType != Conn::NOCONN )
    addOneMetric( doc, el, "connective", Conn::toString(connType) );
  if ( sitType != Situation::NO_SIT )
    addOneMetric( doc, el, "situation", Situation::toString(sitType) );
  if ( isMultiConn )
    addOneMetric( doc, el, "multi_connective", "true" );
  if ( !std::isnan(prevalenceP) )
    addOneMetric( doc, el, "prevalenceP", TiCC::toString(prevalenceP) );
  if ( !std::isnan(prevalenceZ) )
    addOneMetric( doc, el, "prevalenceZ", TiCC::toString(prevalenceZ) );
  if ( f50 )
    addOneMetric( doc, el, "f50", "true" );
  if ( f65 )
    addOneMetric( doc, el, "f65", "true" );
  if ( f77 )
    addOneMetric( doc, el, "f77", "true" );
  if ( f80 )
    addOneMetric( doc, el, "f80", "true" );
  if ( top_freq == top1000 )
    addOneMetric( doc, el, "top1000", "true" );
  else if ( top_freq == top2000 )
    addOneMetric( doc, el, "top2000", "true" );
  else if ( top_freq == top3000 )
    addOneMetric( doc, el, "top3000", "true" );
  else if ( top_freq == top5000 )
    addOneMetric( doc, el, "top5000", "true" );
  else if ( top_freq == top10000 )
    addOneMetric( doc, el, "top10000", "true" );
  else if ( top_freq == top20000 )
    addOneMetric( doc, el, "top20000", "true" );
  addOneMetric( doc, el, "word_freq", TiCC::toString(word_freq) );
  if ( !std::isnan(word_freq_log) )
    addOneMetric( doc, el, "log_word_freq", TiCC::toString(word_freq_log) );
  addOneMetric( doc, el, "lemma_freq", TiCC::toString(lemma_freq) );
  if ( !std::isnan(lemma_freq_log) )
    addOneMetric( doc, el, "log_lemma_freq", TiCC::toString(lemma_freq_log) );
  addOneMetric( doc, el,
    "word_overlap_count", TiCC::toString( wordOverlapCnt ) );
  addOneMetric( doc, el,
    "lemma_overlap_count", TiCC::toString( lemmaOverlapCnt ) );
  if ( !std::isnan(logprob10_fwd) )
    addOneMetric( doc, el, "lprob10_fwd", TiCC::toString(logprob10_fwd) );
  if ( !std::isnan(logprob10_bwd) )
    addOneMetric( doc, el, "lprob10_bwd", TiCC::toString(logprob10_bwd) );
  if ( prop != CGN::JUSTAWORD )
    addOneMetric( doc, el, "property", TiCC::toString(prop) );
  if ( sem_type != SEM::NO_SEMTYPE )
    addOneMetric( doc, el, "semtype", SEM::toString(sem_type) );
  if ( intensify_type != Intensify::NO_INTENSIFY )
    addOneMetric( doc, el, "intensifytype", Intensify::toString(intensify_type) );
  if ( general_noun_type != General::NO_GENERAL )
    addOneMetric( doc, el, "generalnountype", General::toString(general_noun_type) );
  if ( general_verb_type != General::NO_GENERAL )
    addOneMetric( doc, el, "generalverbtype", General::toString(general_verb_type) );
  if ( adverb_type != Adverb::NO_ADVERB )
    addOneMetric( doc, el, "adverbtype", Adverb::toString(adverb_type) );
  if ( afkType != Afk::NO_A )
    addOneMetric( doc, el, "afktype", Afk::toString(afkType) );
  if ( on_stoplist )
    addOneMetric( doc, el, "on_stoplist", "true" );
  if ( !my_classification.empty() )
    addOneMetric( doc, el, "my_classification", my_classification );
}
