#! /bin/bash
if [ `hostname` == "applejack" ]; then
	WOPRPATH="/vol/customopt/uvt-ru/"
else
	WOPRPATH="/home/sloot/usr/local/"
fi
PORT=6345
DATADIR=../data

${WOPRPATH}/bin/wopr -r xmlserver -p ibasefile:${DATADIR}/sonar_newspapercorp_tokenized.3.txt.l2r0_-a4+D.ibase,lexicon:${DATADIR}/sonar_newspapercorp_tokenized.3.txt.lex,keep:1,mode:1,verbose:0,port:${PORT}

