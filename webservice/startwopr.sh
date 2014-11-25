#! /bin/bash
if [ `hostname` == "applejack" ]; then
	WOPRPATH="/vol/customopt/uvt-ru/"
else
	WOPRPATH="/nethome/3020371/tscan/"
fi
PORT=7002
DATADIR=../data

${WOPRPATH}/bin/wopr -r xmlserver -p ibasefile:${DATADIR}/sonar_newspapercorp_tokenized.3.txt.l2r0_-a4+D.ibase,lexicon:${DATADIR}/sonar_newspapercorp_tokenized.3.txt.lex,keep:1,mode:1,verbose:0,lb:1,port:${PORT}
