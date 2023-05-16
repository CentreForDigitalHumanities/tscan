#!/bin/bash
LOGFILE="${LOGDIR=/var/log}/compound-splitter.log"
echo "Compound splitters started..." > $LOGFILE
compound-splitters-nl-socket >> $LOGFILE 2>&1
