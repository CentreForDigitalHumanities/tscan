#!/bin/bash
if [[ -z $LM_PREFIX ]]
then
    export LOGDIR=/tmp
else
    export LOGDIR=${LM_PREFIX}/var/log
fi

# start API (localhost:7005)
if ! command -v compound-splitters-nl-socket
then
    # Make sure it is installed
    # https://github.com/UUDigitalHumanitieslab/compound-splitter
    echo "Compound splitters not found!" > ${LOGDIR}/compound-splitter.log
else
    echo "Compound splitters started..." > ${LOGDIR}/compound-splitter.log
    compound-splitters-nl-socket >> ${LOGDIR}/compound-splitter.log 2>&1
fi
