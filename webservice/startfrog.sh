#!/bin/bash -x
FROGPATH="/usr/local/"
LOGFILE="${LOGDIR=/var/log}/frog-tscan.log"
THREADS=0 #all we can get

PORT=7001
ID=tscan

mv $LOGFILE $LOGFILE.sav # poor mans solution

frog -X --deep-morph --id=${ID} --skip=mp -S"${PORT}" 2> $LOGFILE
