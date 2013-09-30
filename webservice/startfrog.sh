#! /bin/bash
if [ `hostname` == "applejack" ]; then
	FROGPATH="/vol/customopt/uvt-ru/"
    LOGFILE= "/scratch2/www/webservices-lst/live/writable/tscan/frog.log"
else
	FROGPATH="/home/sloot/usr/local/"
    LOGFILE="/tmp/frog-tscan.log"
fi
PORT=7345
ID=tscan
CONFIG=${FROGPATH}/etc/frog/tscan-frog.cfg

${FROGPATH}/bin/frog -X --id=${ID} -c ${CONFIG} --skip=mp -S${PORT} 2> $LOGFILE
