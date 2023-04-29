#ifndef STATS_H
#define	STATS_H

#include <cmath>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include "ticcutils/XMLtools.h"
#include "libfolia/folia.h"
#include "tscan/Alpino.h"
#include "tscan/cgn.h"
#include "tscan/sem.h"
#include "tscan/intensify.h"
#include "tscan/formal.h"
#include "tscan/conn.h"
#include "tscan/general.h"
#include "tscan/situation.h"
#include "tscan/afk.h"
#include "tscan/adverb.h"
#include "tscan/ner.h"
#include "tscan/utils.h"

struct sentStats; // Forward declaration
struct wordStats; // Forward declaration

enum top_val { top1000, top2000, top3000, top5000, top10000, top20000, notFound };
enum csvKind { DOC_CSV, PAR_CSV, SENT_CSV, WORD_CSV };

struct basicStats {
  basicStats( int pos, folia::FoliaElement* el, const std::string& cat ):
    folia_node( el ),
    category( cat ),
    index(pos),
    charCnt(0),
    charCntExNames(0),
    morphCnt(0),
    morphCntExNames(0)
  { if ( el ){
      id = el->id();
    }
    else {
      id = "document";
    }
  };
  virtual ~basicStats(){};
  virtual void CSVheader( std::ostream&, const std::string& = "" ) const = 0;
  virtual void wordDifficultiesHeader( std::ostream& ) const = 0;
  virtual void wordDifficultiesToCSV( std::ostream& ) const = 0;
  virtual void compoundHeader( std::ostream& ) const = 0;
  virtual void compoundToCSV( std::ostream& ) const = 0;
  virtual void sentDifficultiesHeader( std::ostream& ) const = 0;
  virtual void sentDifficultiesToCSV( std::ostream& ) const = 0;
  virtual void infoHeader( std::ostream& ) const = 0;
  virtual void informationDensityToCSV( std::ostream& ) const = 0;
  virtual void coherenceHeader( std::ostream& ) const = 0;
  virtual void coherenceToCSV( std::ostream& ) const = 0;
  virtual void concreetHeader( std::ostream& ) const = 0;
  virtual void concreetToCSV( std::ostream& ) const = 0;
  virtual void persoonlijkheidHeader( std::ostream& ) const = 0;
  virtual void persoonlijkheidToCSV( std::ostream& ) const = 0;
  virtual void verbHeader( std::ostream& ) const = 0;
  virtual void verbToCSV( std::ostream& ) const = 0;
  virtual void imperativeHeader( std::ostream& ) const = 0;
  virtual void imperativeToCSV( std::ostream& ) const = 0;
  virtual void wordSortHeader( std::ostream& ) const = 0;
  virtual void wordSortToCSV( std::ostream& ) const = 0;
  virtual void prepPhraseHeader( std::ostream& ) const = 0;
  virtual void prepPhraseToCSV( std::ostream& ) const = 0;
  virtual void intensHeader( std::ostream& ) const = 0;
  virtual void intensToCSV( std::ostream& ) const = 0;
  virtual void formalHeader( std::ostream& ) const = 0;
  virtual void formalToCSV( std::ostream& ) const = 0;
  virtual void miscToCSV( std::ostream& ) const = 0;
  virtual void miscHeader( std::ostream& ) const = 0;
  virtual double rarity( int ) const { return NAN; };
  virtual void toCSV( std::ostream& ) const = 0;
  virtual void addMetrics() const = 0;
  virtual std::string text() const { return ""; };
  virtual std::string ltext() const { return ""; };
  virtual std::string Lemma() const { return ""; };
  virtual std::string llemma() const { return ""; };
  virtual CGN::Type postag() const { return CGN::UNASS; };
  virtual CGN::Prop wordProperty() const { return CGN::NOTAWORD; };
  virtual Conn::Type getConnType() const { return Conn::NOCONN; };
  virtual void setConnType( Conn::Type ){
    throw std::logic_error("setConnType() only valid for words" );
  };
  virtual void setMultiConn(){
    throw std::logic_error("setMultiConn() only valid for words" );
  };
  virtual void setSitType( Situation::Type ){
    throw std::logic_error("setSitType() only valid for words" );
  };
  virtual Situation::Type getSitType() const { return Situation::NO_SIT; };
  virtual std::vector<const wordStats*> collectWords() const = 0;
  virtual double get_al_gem() const { return NAN; };
  virtual double get_al_max() const { return NAN; };
  folia::FoliaElement* folia_node;
  std::string category;
  std::string id;
  int index;
  int charCnt;
  int charCntExNames;
  int morphCnt;
  int morphCntExNames;
  std::vector<basicStats*> sv;
};


struct wordStats : public basicStats {
  wordStats( int, folia::Word*, const xmlNode*, const std::set<size_t>&, bool );
  void CSVheader( std::ostream &, const std::string & ) const override;
  void wordDifficultiesHeader( std::ostream & ) const override;
  void wordDifficultiesToCSV( std::ostream & ) const override;
  void sentDifficultiesHeader( std::ostream & ) const override{};
  void sentDifficultiesToCSV( std::ostream & ) const override{};
  void infoHeader( std::ostream & ) const override{};
  void informationDensityToCSV( std::ostream & ) const override{};
  void coherenceHeader( std::ostream & ) const override;
  void coherenceToCSV( std::ostream & ) const override;
  void concreetHeader( std::ostream & ) const override;
  void concreetToCSV( std::ostream & ) const override;
  void compoundHeader( std::ostream & ) const override;
  void compoundToCSV( std::ostream & ) const override;
  void persoonlijkheidHeader( std::ostream & ) const override;
  void persoonlijkheidToCSV( std::ostream & ) const override;
  void verbHeader( std::ostream & ) const override{};
  void verbToCSV( std::ostream & ) const override{};
  void imperativeHeader( std::ostream & ) const override{};
  void imperativeToCSV( std::ostream & ) const override{};
  void wordSortHeader( std::ostream & ) const override;
  void wordSortToCSV( std::ostream & ) const override;
  void prepPhraseHeader( std::ostream & ) const override{};
  void prepPhraseToCSV( std::ostream & ) const override{};
  void intensHeader( std::ostream & ) const override{};
  void intensToCSV( std::ostream & ) const override{};
  void formalHeader( std::ostream & ) const override{};
  void formalToCSV( std::ostream & ) const override{};
  void miscHeader( std::ostream &os ) const override;
  void miscToCSV( std::ostream & ) const override;
  void toCSV( std::ostream & ) const override;
  std::string text() const override { return word; };
  std::string ltext() const override { return l_word; };
  std::string Lemma() const override { return lemma; };
  std::string llemma() const override { return l_lemma; };
  CGN::Type postag() const override { return tag; };
  Conn::Type getConnType() const override { return connType; };
  void setConnType( Conn::Type t ) override { connType = t; };
  void setMultiConn() override { isMultiConn = true; };
  bool setPersRef();
  void setSitType( Situation::Type t ) override { sitType = t; };
  Situation::Type getSitType() const override { return sitType; };
  void addMetrics() const override;
  bool checkContent( bool ) const;
  Conn::Type checkConnective() const;
  Situation::Type checkSituation() const;
  bool checkNominal( const xmlNode* ) const;
  void setCGNProps( const folia::PosAnnotation* );
  CGN::Prop wordProperty() const override { return prop; };
  void checkNoun();
  SEM::Type checkSemProps() const;
  Intensify::Type checkIntensify(const xmlNode*) const;
  Formal::Type checkFormal() const;
  General::Type checkGeneralNoun() const;
  General::Type checkGeneralVerb() const;
  Afk::Type checkAfk() const;
  std::string checkMyClassification() const;
  bool checkStoplist() const;
  bool checkPropNeg() const;
  bool checkMorphNeg() const;
  void prevalenceLookup();
  void formalLookup();
  void staphFreqLookup();
  top_val topFreqLookup(const std::string&) const;
  int wordFreqLookup(const std::string&) const;
  void freqLookup();
  void getSentenceOverlap( const std::vector<std::string>&, const std::vector<std::string>& );
  bool isOverlapCandidate() const;
  std::vector<const wordStats *> collectWords() const override;
  bool parseFail;
  std::string word;
  std::string l_word;
  std::string pos;
  CGN::Type tag;
  std::string lemma;
  std::string full_lemma; // scheidbare ww hebben dit
  std::string l_lemma;
  WWform wwform;
  bool isPersRef;
  bool isPronRef;
  bool archaic;
  bool isContent;
  bool isContentStrict;
  bool isNominal;
  bool isOnder;
  bool isImperative;
  bool isBetr;
  bool isPropNeg;
  bool isMorphNeg;
  NER::Type nerProp;
  Conn::Type connType;
  bool isMultiConn;
  Situation::Type sitType;
  double prevalenceP;
  double prevalenceZ;
  bool f50;
  bool f65;
  bool f77;
  bool f80;
  top_val top_freq;
  int word_freq;
  int lemma_freq;
  int wordOverlapCnt;
  int lemmaOverlapCnt;
  double word_freq_log;
  double lemma_freq_log;
  double logprob10_fwd;
  double logprob10_bwd;
  CGN::Prop prop;
  CGN::Position position;
  SEM::Type sem_type;
  Intensify::Type intensify_type;
  Formal::Type formal_type;
  General::Type general_noun_type;
  General::Type general_verb_type;
  Adverb::Type adverb_type;
  Adverb::SubType adverb_sub_type;
  std::vector<std::string> morphemes;
  std::multimap<DD_type,int> distances;
  Afk::Type afkType;
  bool is_compound;
  int compound_parts;
  std::string compound_head;
  std::string compound_sat;
  int charCntHead;
  int charCntSat;
  double word_freq_log_head;
  double word_freq_log_sat;
  double word_freq_log_head_sat;
  double word_freq_log_corr;
  top_val top_freq_head;
  top_val top_freq_sat;
  std::string compstr;
  bool on_stoplist;
  std::string my_classification;
};

struct structStats: public basicStats {
  structStats( int index, folia::FoliaElement* el, const std::string& cat ):
    basicStats( index, el, cat ),
    wordCnt(0),
    wordInclCnt(0), // wordCnt including stopwords
    sentCnt(0),
    parseFailCnt(0),
    vdBvCnt(0),
    vdNwCnt(0),
    vdVrijCnt(0),
    odBvCnt(0),
    odNwCnt(0),
    odVrijCnt(0),
    infBvCnt(0),
    infNwCnt(0),
    infVrijCnt(0),
    smainCnt(0),
    ssubCnt(0),
    sv1Cnt(0),
    clauseCnt(0),
    correctedClauseCnt(0),
    smainCnjCnt(0),
    ssubCnjCnt(0),
    sv1CnjCnt(0),
    presentCnt(0),
    pastCnt(0),
    subjonctCnt(0),
    nameCnt(0),
    nameInclCnt(0), // nameCnt including stopwords
    pron1Cnt(0),
    pron2Cnt(0),
    pron3Cnt(0),
    passiveCnt(0),
    modalCnt(0),
    timeVCnt(0),
    koppelCnt(0),
    persRefCnt(0),
    pronRefCnt(0),
    archaicsCnt(0),
    contentCnt(0),
    contentInclCnt(0), // contentCnt including stopwords
    contentStrictCnt(0),
    contentStrictInclCnt(0), // contentStrictCnt including stopwords
    nominalCnt(0),
    adjCnt(0),
    adjInclCnt(0), // adjCnt including stopwords
    vgCnt(0),
    vnwCnt(0),
    lidCnt(0),
    vzCnt(0),
    bwCnt(0),
    twCnt(0),
    nounCnt(0),
    nounInclCnt(0), // nounCnt including stopwords
    verbCnt(0),
    verbInclCnt(0), // verbCnt including stopwords
    tswCnt(0),
    specCnt(0),
    letCnt(0),
    betrCnt(0),
    bijwCnt(0),
    complCnt(0),
    mvFinInbedCnt(0),
    infinComplCnt(0),
    infinComplBepCnt(0),
    mvInbedCnt(0),
    losBetrCnt(0),
    losBijwCnt(0),
    allConnCnt(0),
    tempConnCnt(0),
    opsomWgConnCnt(0),
    opsomZinConnCnt(0),
    contrastConnCnt(0),
    compConnCnt(0),
    causeConnCnt(0),
    timeSitCnt(0),
    spaceSitCnt(0),
    causeSitCnt(0),
    emoSitCnt(0),
    propNegCnt(0),
    morphNegCnt(0),
    multiNegCnt(0),
    wordOverlapCnt(0),
    lemmaOverlapCnt(0),
    prevalenceP(0),
    prevalenceZ(0),
    prevalenceContentP(0),
    prevalenceContentZ(0),
    prevalenceCovered(0),
    prevalenceContentCovered(0),
    f50Cnt(0),
    f65Cnt(0),
    f77Cnt(0),
    f80Cnt(0),
    top1000Cnt(0),
    top2000Cnt(0),
    top3000Cnt(0),
    top5000Cnt(0),
    top10000Cnt(0),
    top20000Cnt(0),
    top1000ContentCnt(0),
    top2000ContentCnt(0),
    top3000ContentCnt(0),
    top5000ContentCnt(0),
    top10000ContentCnt(0),
    top20000ContentCnt(0),
    top1000ContentStrictCnt(0),
    top2000ContentStrictCnt(0),
    top3000ContentStrictCnt(0),
    top5000ContentStrictCnt(0),
    top10000ContentStrictCnt(0),
    top20000ContentStrictCnt(0),
    word_freq(0),
    word_freq_n(0),
    word_freq_strict(0),
    word_freq_n_strict(0),
    word_freq_log(NAN),
    word_freq_log_n(NAN),
    word_freq_log_strict(NAN),
    word_freq_log_n_strict(NAN),
    lemma_freq(0),
    lemma_freq_n(0),
    lemma_freq_strict(0),
    lemma_freq_n_strict(0),
    lemma_freq_log(NAN),
    lemma_freq_log_n(NAN),
    lemma_freq_log_strict(NAN),
    lemma_freq_log_n_strict(NAN),
    avg_prob10_fwd(0),
    avg_prob10_fwd_content(0),
    avg_prob10_fwd_ex_names(0),
    avg_prob10_fwd_content_ex_names(0),
    avg_prob10_bwd(0),
    avg_prob10_bwd_content(0),
    avg_prob10_bwd_ex_names(0),
    avg_prob10_bwd_content_ex_names(0),
    entropy_fwd(0),
    entropy_fwd_norm(0),
    entropy_bwd(0),
    entropy_bwd_norm(0),
    perplexity_fwd(0),
    perplexity_fwd_norm(0),
    perplexity_bwd(0),
    perplexity_bwd_norm(0),
    al_gem(NAN),
    al_max(NAN),
    intensCnt(0),
    intensBvnwCnt(0),
    intensBvbwCnt(0),
    intensBwCnt(0),
    intensCombiCnt(0),
    intensNwCnt(0),
    intensTussCnt(0),
    intensWwCnt(0),
    formalCnt(0),
    formalBvnwCnt(0),
    formalBwCnt(0),
    formalVgwCnt(0),
    formalVnwCnt(0),
    formalVzCnt(0),
    formalVzgCnt(0),
    formalWwCnt(0),
    formalZnwCnt(0),
    generalNounCnt(0),
    generalNounSepCnt(0),
    generalNounRelCnt(0),
    generalNounActCnt(0),
    generalNounKnowCnt(0),
    generalNounDiscCnt(0),
    generalNounDeveCnt(0),
    generalVerbCnt(0),
    generalVerbSepCnt(0),
    generalVerbRelCnt(0),
    generalVerbActCnt(0),
    generalVerbKnowCnt(0),
    generalVerbDiscCnt(0),
    generalVerbDeveCnt(0),
    generalAdverbCnt(0),
    specificAdverbCnt(0),
    broadNounCnt(0),
    strictNounCnt(0),
    broadAdjCnt(0),
    strictAdjCnt(0),
    subjectiveAdjCnt(0),
    abstractWwCnt(0),
    concreteWwCnt(0),
    undefinedWwCnt(0),
    undefinedATPCnt(0),
    stateCnt(0),
    actionCnt(0),
    processCnt(0),
    humanAdjCnt(0),
    emoAdjCnt(0),
    nonhumanAdjCnt(0),
    shapeAdjCnt(0),
    colorAdjCnt(0),
    matterAdjCnt(0),
    soundAdjCnt(0),
    nonhumanOtherAdjCnt(0),
    techAdjCnt(0),
    timeAdjCnt(0),
    placeAdjCnt(0),
    specPosAdjCnt(0),
    specNegAdjCnt(0),
    posAdjCnt(0),
    negAdjCnt(0),
    evaluativeAdjCnt(0),
    epiPosAdjCnt(0),
    epiNegAdjCnt(0),
    abstractAdjCnt(0),
    undefinedNounCnt(0),
    uncoveredNounCnt(0),
    undefinedAdjCnt(0),
    uncoveredAdjCnt(0),
    uncoveredVerbCnt(0),
    humanCnt(0),
    nonHumanCnt(0),
    artefactCnt(0),
    concrotherCnt(0),
    substanceConcCnt(0),
    foodcareCnt(0),
    timeCnt(0),
    placeCnt(0),
    measureCnt(0),
    dynamicConcCnt(0),
    substanceAbstrCnt(0),
    dynamicAbstrCnt(0),
    nonDynamicCnt(0),
    institutCnt(0),
    npCnt(0),
    indefNpCnt(0),
    npSize(0),
    vcModCnt(0),
    vcModSingleCnt(0),
    adjNpModCnt(0),
    npModCnt(0),
    smallCnjCnt(0),
    smallCnjExtraCnt(0),
    dLevel(-1),
    dLevel_gt4(0),
    impCnt(0),
    questCnt(0),
    prepExprCnt(0),
    word_mtld(0),
    lemma_mtld(0),
    content_mtld(0),
    content_mtld_strict(0),
    name_mtld(0),
    all_conn_mtld(0),
    temp_conn_mtld(0),
    reeks_wg_conn_mtld(0),
    reeks_zin_conn_mtld(0),
    contr_conn_mtld(0),
    comp_conn_mtld(0),
    cause_conn_mtld(0),
    tijd_sit_mtld(0),
    ruimte_sit_mtld(0),
    cause_sit_mtld(0),
    emotion_sit_mtld(0),
    nerCnt(0),
    compoundCnt(0),
    compound3Cnt(0),
    charCntNoun(0),
    charCntNonComp(0),
    charCntComp(0),
    charCntHead(0),
    charCntSat(0),
    charCntNounCorr(0),
    charCntCorr(0),
    word_freq_log_noun(0),
    word_freq_log_non_comp(0),
    word_freq_log_comp(0),
    word_freq_log_head(0),
    word_freq_log_sat(0),
    word_freq_log_head_sat(0),
    word_freq_log_noun_corr(0),
    word_freq_log_corr(0),
    word_freq_log_corr_strict(0),
    word_freq_log_n_corr(0),
    word_freq_log_n_corr_strict(0),
    top1000CntNoun(0),
    top1000CntNonComp(0),
    top1000CntComp(0),
    top1000CntHead(0),
    top1000CntSat(0),
    top1000CntNounCorr(0),
    top1000CntCorr(0),
    top5000CntNoun(0),
    top5000CntNonComp(0),
    top5000CntComp(0),
    top5000CntHead(0),
    top5000CntSat(0),
    top5000CntNounCorr(0),
    top5000CntCorr(0),
    top20000CntNoun(0),
    top20000CntNonComp(0),
    top20000CntComp(0),
    top20000CntHead(0),
    top20000CntSat(0),
    top20000CntNounCorr(0),
    top20000CntCorr(0),
    rarityLevel(0),
    overlapSize(0)
 {};
  ~structStats();
  void addMetrics() const override;
  void topPredictorsHeader( std::ostream& ) const;
  void topPredictorsToCSV( std::ostream& ) const;
  void wordDifficultiesHeader( std::ostream & ) const override;
  void wordDifficultiesToCSV( std::ostream & ) const override;
  void compoundHeader( std::ostream & ) const override;
  void compoundToCSV( std::ostream & ) const override;
  void sentDifficultiesHeader( std::ostream & ) const override;
  void sentDifficultiesToCSV( std::ostream & ) const override;
  void infoHeader( std::ostream & ) const override;
  void informationDensityToCSV( std::ostream & ) const override;
  void coherenceHeader( std::ostream & ) const override;
  void coherenceToCSV( std::ostream & ) const override;
  void concreetHeader( std::ostream & ) const override;
  void concreetToCSV( std::ostream & ) const override;
  void persoonlijkheidHeader( std::ostream & ) const override;
  void persoonlijkheidToCSV( std::ostream & ) const override;
  void verbHeader( std::ostream & ) const override;
  void verbToCSV( std::ostream & ) const override;
  void imperativeHeader( std::ostream & ) const override;
  void imperativeToCSV( std::ostream & ) const override;
  void wordSortHeader( std::ostream & ) const override;
  void wordSortToCSV( std::ostream & ) const override;
  void prepPhraseHeader( std::ostream & ) const override;
  void prepPhraseToCSV( std::ostream & ) const override;
  void intensHeader( std::ostream & ) const override;
  void intensToCSV( std::ostream & ) const override;
  void formalHeader( std::ostream & ) const override;
  void formalToCSV( std::ostream & ) const override;
  void miscHeader( std::ostream & ) const override;
  void miscToCSV( std::ostream & ) const override;
  void CSVheader( std::ostream &, const std::string & ) const override;
  void toCSV( std::ostream & ) const override;
  void merge( structStats* );
  virtual bool isSentence() const { return false; };
  virtual bool isDocument() const { return false; };
  virtual int word_overlapCnt() const { return -1; };
  virtual int lemma_overlapCnt() const { return -1; };
  std::vector<const wordStats *> collectWords() const override;
  double get_al_gem() const override { return al_gem; };
  double get_al_max() const override { return al_max; };
  virtual double getMeanAL() const;
  virtual double getHighestAL() const;
  void calculate_MTLDs();
  std::string text;
  int wordCnt;
  int wordInclCnt;
  int sentCnt;
  int parseFailCnt;
  int vdBvCnt;
  int vdNwCnt;
  int vdVrijCnt;
  int odBvCnt;
  int odNwCnt;
  int odVrijCnt;
  int infBvCnt;
  int infNwCnt;
  int infVrijCnt;
  int smainCnt;
  int ssubCnt;
  int sv1Cnt;
  int clauseCnt;
  int correctedClauseCnt;
  int smainCnjCnt;
  int ssubCnjCnt;
  int sv1CnjCnt;
  int presentCnt;
  int pastCnt;
  int subjonctCnt;
  int nameCnt;
  int nameInclCnt;
  int pron1Cnt;
  int pron2Cnt;
  int pron3Cnt;
  int passiveCnt;
  int modalCnt;
  int timeVCnt;
  int koppelCnt;
  int persRefCnt;
  int pronRefCnt;
  int archaicsCnt;
  int contentCnt;
  int contentInclCnt;
  int contentStrictCnt;
  int contentStrictInclCnt;
  int nominalCnt;
  int adjCnt;
  int adjInclCnt;
  int vgCnt;
  int vnwCnt;
  int lidCnt;
  int vzCnt;
  int bwCnt;
  int twCnt;
  int nounCnt;
  int nounInclCnt;
  int verbCnt;
  int verbInclCnt;
  int tswCnt;
  int specCnt;
  int letCnt;
  int betrCnt;
  int bijwCnt;
  int complCnt;
  int mvFinInbedCnt;
  int infinComplCnt;
  int infinComplBepCnt;
  int mvInbedCnt;
  int losBetrCnt;
  int losBijwCnt;
  int allConnCnt;
  int tempConnCnt;
  int opsomWgConnCnt;
  int opsomZinConnCnt;
  int contrastConnCnt;
  int compConnCnt;
  int causeConnCnt;
  int timeSitCnt;
  int spaceSitCnt;
  int causeSitCnt;
  int emoSitCnt;
  int propNegCnt;
  int morphNegCnt;
  int multiNegCnt;
  int wordOverlapCnt;
  int lemmaOverlapCnt;
  double prevalenceP;
  double prevalenceZ;
  double prevalenceContentP;
  double prevalenceContentZ;
  int prevalenceCovered;
  int prevalenceContentCovered;
  int f50Cnt;
  int f65Cnt;
  int f77Cnt;
  int f80Cnt;
  int top1000Cnt;
  int top2000Cnt;
  int top3000Cnt;
  int top5000Cnt;
  int top10000Cnt;
  int top20000Cnt;
  int top1000ContentCnt;
  int top2000ContentCnt;
  int top3000ContentCnt;
  int top5000ContentCnt;
  int top10000ContentCnt;
  int top20000ContentCnt;
  int top1000ContentStrictCnt;
  int top2000ContentStrictCnt;
  int top3000ContentStrictCnt;
  int top5000ContentStrictCnt;
  int top10000ContentStrictCnt;
  int top20000ContentStrictCnt;
  double word_freq;
  double word_freq_n;
  double word_freq_strict;
  double word_freq_n_strict;
  double word_freq_log;
  double word_freq_log_n;
  double word_freq_log_strict;
  double word_freq_log_n_strict;
  double lemma_freq;
  double lemma_freq_n;
  double lemma_freq_strict;
  double lemma_freq_n_strict;
  double lemma_freq_log;
  double lemma_freq_log_n;
  double lemma_freq_log_strict;
  double lemma_freq_log_n_strict;
  double avg_prob10_fwd;
  double avg_prob10_fwd_content;
  double avg_prob10_fwd_ex_names;
  double avg_prob10_fwd_content_ex_names;
  double avg_prob10_bwd;
  double avg_prob10_bwd_content;
  double avg_prob10_bwd_ex_names;
  double avg_prob10_bwd_content_ex_names;
  double entropy_fwd;
  double entropy_fwd_norm;
  double entropy_bwd;
  double entropy_bwd_norm;
  double perplexity_fwd;
  double perplexity_fwd_norm;
  double perplexity_bwd;
  double perplexity_bwd_norm;
  double al_gem;
  double al_max;
  int intensCnt;
  int intensBvnwCnt;
  int intensBvbwCnt;
  int intensBwCnt;
  int intensCombiCnt;
  int intensNwCnt;
  int intensTussCnt;
  int intensWwCnt;
  int formalCnt;
  int formalBvnwCnt;
  int formalBwCnt;
  int formalVgwCnt;
  int formalVnwCnt;
  int formalVzCnt;
  int formalVzgCnt;
  int formalWwCnt;
  int formalZnwCnt;
  int generalNounCnt;
  int generalNounSepCnt;
  int generalNounRelCnt;
  int generalNounActCnt;
  int generalNounKnowCnt;
  int generalNounDiscCnt;
  int generalNounDeveCnt;
  int generalVerbCnt;
  int generalVerbSepCnt;
  int generalVerbRelCnt;
  int generalVerbActCnt;
  int generalVerbKnowCnt;
  int generalVerbDiscCnt;
  int generalVerbDeveCnt;
  int generalAdverbCnt;
  int specificAdverbCnt;
  int broadNounCnt;
  int strictNounCnt;
  int broadAdjCnt;
  int strictAdjCnt;
  int subjectiveAdjCnt;
  int abstractWwCnt;
  int concreteWwCnt;
  int undefinedWwCnt;
  int undefinedATPCnt;
  int stateCnt;
  int actionCnt;
  int processCnt;
  int humanAdjCnt;
  int emoAdjCnt;
  int nonhumanAdjCnt;
  int shapeAdjCnt;
  int colorAdjCnt;
  int matterAdjCnt;
  int soundAdjCnt;
  int nonhumanOtherAdjCnt;
  int techAdjCnt;
  int timeAdjCnt;
  int placeAdjCnt;
  int specPosAdjCnt;
  int specNegAdjCnt;
  int posAdjCnt;
  int negAdjCnt;
  int evaluativeAdjCnt;
  int epiPosAdjCnt;
  int epiNegAdjCnt;
  int abstractAdjCnt;
  int undefinedNounCnt;
  int uncoveredNounCnt;
  int undefinedAdjCnt;
  int uncoveredAdjCnt;
  int uncoveredVerbCnt;
  int humanCnt;
  int nonHumanCnt;
  int artefactCnt;
  int concrotherCnt;
  int substanceConcCnt;
  int foodcareCnt;
  int timeCnt;
  int placeCnt;
  int measureCnt;
  int dynamicConcCnt;
  int substanceAbstrCnt;
  int dynamicAbstrCnt;
  int nonDynamicCnt;
  int institutCnt;
  int npCnt;
  int indefNpCnt;
  int npSize;
  int vcModCnt;
  int vcModSingleCnt;
  int adjNpModCnt;
  int npModCnt;
  int smallCnjCnt;
  int smallCnjExtraCnt;
  int dLevel;
  int dLevel_gt4;
  int impCnt;
  int questCnt;
  int prepExprCnt;
  std::map<CGN::Type,int> heads;
  std::map<std::string,int> unique_names;
  std::map<std::string,int> unique_contents;
  std::map<std::string,int> unique_contents_strict;
  std::map<std::string,int> unique_tijd_sits;
  std::map<std::string,int> unique_ruimte_sits;
  std::map<std::string,int> unique_cause_sits;
  std::map<std::string,int> unique_emotion_sits;
  std::map<std::string,int> unique_all_conn;
  std::map<std::string,int> unique_temp_conn;
  std::map<std::string,int> unique_reeks_wg_conn;
  std::map<std::string,int> unique_reeks_zin_conn;
  std::map<std::string,int> unique_contr_conn;
  std::map<std::string,int> unique_comp_conn;
  std::map<std::string,int> unique_cause_conn;
  std::map<std::string,int> unique_words;
  std::map<std::string,int> unique_lemmas;
  double word_mtld;
  double lemma_mtld;
  double content_mtld;
  double content_mtld_strict;
  double name_mtld;
  double all_conn_mtld;
  double temp_conn_mtld;
  double reeks_wg_conn_mtld;
  double reeks_zin_conn_mtld;
  double contr_conn_mtld;
  double comp_conn_mtld;
  double cause_conn_mtld;
  double tijd_sit_mtld;
  double ruimte_sit_mtld;
  double cause_sit_mtld;
  double emotion_sit_mtld;
  std::map<NER::Type, int> ners;
  int nerCnt;
  std::map<Afk::Type, int> afks;
  std::multimap<DD_type,int> distances;
  int compoundCnt;
  int compound3Cnt;
  int charCntNoun;
  int charCntNonComp;
  int charCntComp;
  int charCntHead;
  int charCntSat;
  int charCntNounCorr;
  int charCntCorr;
  double word_freq_log_noun;
  double word_freq_log_non_comp;
  double word_freq_log_comp;
  double word_freq_log_head;
  double word_freq_log_sat;
  double word_freq_log_head_sat;
  double word_freq_log_noun_corr;
  double word_freq_log_corr;
  double word_freq_log_corr_strict;
  double word_freq_log_n_corr;
  double word_freq_log_n_corr_strict;
  int top1000CntNoun;
  int top1000CntNonComp;
  int top1000CntComp;
  int top1000CntHead;
  int top1000CntSat;
  int top1000CntNounCorr;
  int top1000CntCorr;
  int top5000CntNoun;
  int top5000CntNonComp;
  int top5000CntComp;
  int top5000CntHead;
  int top5000CntSat;
  int top5000CntNounCorr;
  int top5000CntCorr;
  int top20000CntNoun;
  int top20000CntNonComp;
  int top20000CntComp;
  int top20000CntHead;
  int top20000CntSat;
  int top20000CntNounCorr;
  int top20000CntCorr;
  int rarityLevel;
  unsigned int overlapSize;
  std::map<std::string,int> my_classification;
};


struct sentStats : public structStats {
  sentStats( const std::string&, int, folia::Sentence*, const sentStats* );
  bool isSentence() const override { return true; };
  void resolveConnectives();
  void resolveSituations();
  void resolveMultiWordIntensify();
  void resolveMultiWordFormal();
  // void resolveMultiWordAfks();
  void addMetrics() const override;
  bool checkAls( size_t );
  double getMeanAL() const override;
  double getHighestAL() const override;
  Conn::Type checkMultiConnectives( const std::string& );
  Situation::Type checkMultiSituations( const std::string& );
  void resolvePrepExpr();
  void resolveAdverbials( xmlDoc* );
  void resolveRelativeClauses( xmlDoc* );
  void resolveFiniteVerbs( xmlDoc* );
  void resolveConjunctions( xmlDoc* );
  void resolveSmallConjunctions( xmlDoc* );
  void setCommonCounts( wordStats* );
  void setFormalCounts( wordStats* );
};


struct parStats: public structStats {
  parStats( const std::string&, int, folia::Paragraph* );
  void addMetrics() const override;
};


struct docStats : public structStats {
  explicit docStats( const std::string&, folia::Document* );
  bool isDocument() const override { return true; };
  void toCSV( const std::string&, csvKind ) const;
  double rarity( int level ) const override;
  void addMetrics() const override;
  int word_overlapCnt() const override { return doc_word_overlapCnt; };
  int lemma_overlapCnt() const override { return doc_lemma_overlapCnt; };
  void calculate_doc_overlap();
  int doc_word_overlapCnt;
  int doc_lemma_overlapCnt;
  double rarity_index;
};

template <class T, typename F>
void resolveMultiWord( const std::vector<basicStats *> &sv, const std::map<std::string, T> &m, const size_t &max_length, F &&assign ) {

  for ( size_t i = 0; i < sv.size() - 1; ++i ) {
    std::string startword = sv[i]->ltext();
    std::string multiword = startword;

    for ( size_t j = 1; i + j < sv.size() && j < max_length; ++j ) {
      // Attach the next word to the expression
      multiword += " " + sv[i + j]->ltext();

      // Look for the expression in the list
      auto sit = m.find( multiword );
      // If found, update the counts, if not, continue
      if ( sit != m.end() ) {
        for ( size_t k = i; k <= i + j; k++ ) {
          auto word = dynamic_cast<wordStats *>( sv[k] );
          assign( word, sit->second );
        }
        // Break and skip to the first word after this expression
        i += j;
        break;
      }
    }
  }
}

#endif /* STATS_H */
