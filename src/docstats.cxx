#include "tscan/stats.h"

using namespace std;

/*****
 * LSA
 *****/

void docStats::setLSAvalues( double suc, double net, double ctx ){
  if ( suc > 0 )
    lsa_par_suc = suc;
  if ( net > 0 )
    lsa_par_net = net;
  if ( ctx > 0 )
    lsa_par_ctx = ctx;
}

/********
 * RARITY
 ********/

double docStats::rarity( int level ) const {
  map<string,int>::const_iterator it = unique_lemmas.begin();
  int rare = 0;
  while ( it != unique_lemmas.end() ){
    if ( it->second <= level )
      ++rare;
    ++it;
  }
  return rare / double( unique_lemmas.size() );
}

/************
 * CSV OUTPUT
 ************/

void docStats::toCSV( const string& name, csvKind what ) const {
  if ( what == DOC_CSV ){
    string fname = name + ".document.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      // 20141003: New features: paragraphs/sentences/words per document
      CSVheader( out, "Inputfile,Par_per_doc,Zin_per_doc,Word_per_doc" );
      out << name << "," << sv.size() << ",";
      structStats::toCSV( out );
      cerr << "stored document statistics in " << fname << endl;
    }
    else {
      cerr << "storing document statistics in " << fname << " FAILED!" << endl;
    }
  }
  else if ( what == PAR_CSV ){
    string fname = name + ".paragraphs.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      for ( size_t par=0; par < sv.size(); ++par ){
	if ( par == 0 )
    // 20141003: New features: sentences/words per paragraph
	  sv[0]->CSVheader( out, "Inputfile,Segment,Zin_per_par,Wrd_per_par" );
	out << name << "," << sv[par]->id << ",";
	sv[par]->toCSV( out );
    }
      cerr << "stored paragraph statistics in " << fname << endl;
    }
    else {
      cerr << "storing paragraph statistics in " << fname << " FAILED!" << endl;
    }
  }
  else if ( what == SENT_CSV ){
    string fname = name + ".sentences.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      for ( size_t par=0; par < sv.size(); ++par ){
	for ( size_t sent=0; sent < sv[par]->sv.size(); ++sent ){
	  if ( par == 0 && sent == 0 )
	    sv[0]->sv[0]->CSVheader( out, "Inputfile,Segment,Getokeniseerde_zin" );
	  out << name << "," << sv[par]->sv[sent]->id << ",";
	  sv[par]->sv[sent]->toCSV( out );
	}
      }
      cerr << "stored sentence statistics in " << fname << endl;
    }
    else {
      cerr << "storing sentence statistics in " << fname << " FAILED!" << endl;
    }
  }
  else if ( what == WORD_CSV ){
    string fname = name + ".words.csv";
    ofstream out( fname.c_str() );
    if ( out ){
      for ( size_t par=0; par < sv.size(); ++par ){
	for ( size_t sent=0; sent < sv[par]->sv.size(); ++sent ){
	  for ( size_t word=0; word < sv[par]->sv[sent]->sv.size(); ++word ){
	    if ( par == 0 && sent == 0 && word == 0 )
	      sv[0]->sv[0]->sv[0]->CSVheader( out );
	    out << name << ",";
	    sv[par]->sv[sent]->sv[word]->toCSV( out );
	  }
	}
      }
      cerr << "stored word statistics in " << fname << endl;
    }
    else {
      cerr << "storing word statistics in " << fname << " FAILED!" << endl;
    }
  }
}

/**************
 * FOLIA OUTPUT
 **************/

void docStats::addMetrics() const {
  folia::FoliaElement *el = folia_node;
  structStats::addMetrics();
  addOneMetric( el->doc(), el,
		"sentence_count", TiCC::toString( sentCnt ) );
  addOneMetric( el->doc(), el,
		"paragraph_count", TiCC::toString( sv.size() ) );
  addOneMetric( el->doc(), el,
		"word_ttr", TiCC::toString( unique_words.size()/double(wordCnt) ) );
  addOneMetric( el->doc(), el,
		"word_mtld", TiCC::toString( word_mtld ) );
  addOneMetric( el->doc(), el,
		"lemma_ttr", TiCC::toString( unique_lemmas.size()/double(wordCnt) ) );
  addOneMetric( el->doc(), el,
		"lemma_mtld", TiCC::toString( lemma_mtld ) );
  if ( nameCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "names_ttr", TiCC::toString( unique_names.size()/double(nameCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"name_mtld", TiCC::toString( name_mtld ) );

  if ( contentCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "content_word_ttr", TiCC::toString( unique_contents.size()/double(contentCnt) ) );
  }
  if ( contentStrictCnt != 0 ){
    addOneMetric( el->doc(), el,
      "content_word_ttr_strict", TiCC::toString( unique_contents_strict.size()/double(contentStrictCnt) ) );
  }

  addOneMetric( el->doc(), el,
		"content_mtld", TiCC::toString( content_mtld ) );
  addOneMetric( el->doc(), el,
    "content_mtld_strict", TiCC::toString( content_mtld_strict ) );

  if ( timeSitCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "time_sit_ttr", TiCC::toString( unique_tijd_sits.size()/double(timeSitCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"tijd_sit_mtld", TiCC::toString( tijd_sit_mtld ) );

  if ( spaceSitCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "space_sit_ttr", TiCC::toString( unique_ruimte_sits.size()/double(spaceSitCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"ruimte_sit_mtld", TiCC::toString( ruimte_sit_mtld ) );

  if ( causeSitCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "cause_sit_ttr", TiCC::toString( unique_cause_sits.size()/double(causeSitCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"cause_sit_mtld", TiCC::toString( cause_sit_mtld ) );

  if ( emoSitCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "emotion_sit_ttr", TiCC::toString( unique_emotion_sits.size()/double(emoSitCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"emotion_sit_mtld", TiCC::toString( emotion_sit_mtld ) );

  if ( allConnCnt != 0 ){
    addOneMetric( el->doc(), el,
      "all_conn_ttr", TiCC::toString( unique_all_conn.size()/double(allConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
    "all_conn_mtld", TiCC::toString(all_conn_mtld) );

  if ( tempConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "temp_conn_ttr", TiCC::toString( unique_temp_conn.size()/double(tempConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"temp_conn_mtld", TiCC::toString(temp_conn_mtld) );

  if ( opsomWgConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "opsom_wg_conn_ttr", TiCC::toString( unique_reeks_wg_conn.size()/double(opsomWgConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"opsom_wg_conn_mtld", TiCC::toString(reeks_wg_conn_mtld) );

  if ( opsomZinConnCnt != 0 ){
    addOneMetric( el->doc(), el,
      "opsom_zin_conn_ttr", TiCC::toString( unique_reeks_zin_conn.size()/double(opsomZinConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
    "opsom_zin_conn_mtld", TiCC::toString(reeks_zin_conn_mtld) );

  if ( contrastConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "contrast_conn_ttr", TiCC::toString( unique_contr_conn.size()/double(contrastConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"contrast_conn_mtld", TiCC::toString(contr_conn_mtld) );

  if ( compConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "comp_conn_ttr", TiCC::toString( unique_comp_conn.size()/double(compConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"comp_conn_mtld", TiCC::toString(comp_conn_mtld) );


  if ( causeConnCnt != 0 ){
    addOneMetric( el->doc(), el,
		  "cause_conn_ttr", TiCC::toString( unique_cause_conn.size()/double(causeConnCnt) ) );
  }
  addOneMetric( el->doc(), el,
		"cause_conn_mtld", TiCC::toString( cause_conn_mtld ) );


  addOneMetric( el->doc(), el,
		"rar_index", TiCC::toString( rarity_index ) );
  addOneMetric( el->doc(), el,
		"document_word_argument_overlap_count", TiCC::toString( doc_word_overlapCnt ) );
  addOneMetric( el->doc(), el,
		"document_lemma_argument_overlap_count", TiCC::toString( doc_lemma_overlapCnt ) );
}
