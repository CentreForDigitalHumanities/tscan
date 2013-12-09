#! /bin/bash

PORT=7004
DATADIR=../data

./gensim_lsa_server.py ${PORT} ${DATADIR}/newspaper_contentlemma_dictionary_filtered.dict ${DATADIR}/lsa_newspaper_lemma_model_700_filtered.lsi &