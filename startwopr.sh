#! /bin/sh
PATH=/home/sloot/usr/local/
PORT=6345
DATADIR=./data

${PATH}/bin/wopr -r xmlserver -p ibasefile:${DATADIR}/sonar_newspapercorp_tokenized.3.txt.l2r0_-a4+D.ibase,lexicon:${DATADIR}/sonar_newspapercorp_tokenized.3.txt.lex,keep:1,mode:1,verbose:2,port:${PORT}

