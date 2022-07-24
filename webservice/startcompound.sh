#!/bin/bash
if [[ -z $LM_PREFIX ]]
then
    export LOGDIR=/tmp
else
    export LOGDIR=${LM_PREFIX}/var/log
fi
source lamachine-activate
export SRCDIR=${LM_PREFIX}/src
export SPLITTERDIR=${SRCDIR}/compound-splitter

# start API (localhost:7005)
if [ ! -d $SPLITTERDIR ]
then
    # Make sure it is retrieved
    # https://github.com/UUDigitalHumanitieslab/compound-splitter
    echo "Compound splitters not found!" > ${LOGDIR}/compound-splitter.log
else
    echo "Compound splitters started..." > ${LOGDIR}/compound-splitter.log
    compound-splitters-nl-socket >> ${LOGDIR}/compound-splitter.log 2>&1
fi
