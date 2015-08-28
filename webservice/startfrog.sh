#! /bin/bash -x
if [ `hostname` == "applejack" ]; then
	FROGPATH="/scratch2/www/lamachine"
    LOGFILE="/scratch2/www/webservices-lst/live/writable/tscan/frog.log"
else
    FROGPATH="/usr/local/"
    LOGFILE="/tmp/frog-tscan.log"
fi
PORT=7001
ID=tscan
CONFIG=${FROGPATH}/etc/frog/tscan-frog.cfg

mv $LOGFILE $LOGFILE.sav # poor mans solution

frog -X --id=${ID} -c ${CONFIG} --skip=mp -S${PORT} 2> $LOGFILE
