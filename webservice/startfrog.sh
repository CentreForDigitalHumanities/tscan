#! /bin/bash
if [ `hostname` == "applejack" ]; then
	FROGPATH="/vol/customopt/uvt-ru/"
else
	FROGPATH="/home/sloot/usr/local/"
fi
PORT=7345
ID=tscan
CONFIG=${FROGPATH}/etc/frog/tscan-frog.cfg

${FROGPATH}/bin/frog -X --id=${ID} -c ${CONFIG} --skip=mp -S${PORT}
