#!/bin/bash -x
FROGPATH="/usr/local/"
LOGDIR="${LOGDIR=/var/log}"
LOGFILE="${LOGDIR}/frog-tscan.log"
THREADS=0 #all we can get

PORT=7001
ID=tscan

mv $LOGFILE $LOGFILE.sav # poor mans solution
cd $LOGDIR # make sure the debug log ends up in the log directory
# Remove old logs
rm -f core.*
rm -f `ls -t *.debug | awk 'NR>4'`
frog -X --deep-morph --id=${ID} --skip=mp -S"${PORT}" 2> $LOGFILE
