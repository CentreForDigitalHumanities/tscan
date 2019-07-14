#!/bin/bash -x
if [ `hostname` == "mlp01" ]; then
    FROGPATH="/var/www/lamachine2/weblamachine"
    LOGFILE="/var/www/webservices-lst/live/writable/logs/tscan.frog.err"
    THREADS=8
elif [ ! -z "$LM_PREFIX" ]; then
    #LaMachine v2
    FROGPATH="$LM_PREFIX"
    LOGFILE="/tmp/frog-tscan.log"
else
    FROGPATH="/usr/local/"
    LOGFILE="/tmp/frog-tscan.log"
    THREADS=0 #all we can get
fi
PORT=7001
ID=tscan
#CONFIG=${FROGPATH}/share/frog/nld/frog.cfg

mv $LOGFILE $LOGFILE.sav # poor mans solution

frog -X --id=${ID} --skip=mp -S"${PORT}" 2> $LOGFILE

