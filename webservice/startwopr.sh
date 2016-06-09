#! /bin/bash
PORT=7002
DATADIR=../data

/home/sloot/usr/local/bin/wopr -r xmlserver -p ibasefile:${DATADIR}/sonar_newspapercorp_tokenized.3.txt.l2r0_-a4+D.ibase,lexicon:${DATADIR}/sonar_newspapercorp_tokenized.3.txt.lex,keep:1,mode:1,verbose:0,lb:1,port:${PORT}
