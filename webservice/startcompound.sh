#!/bin/bash
export LOGDIR=/var/log
echo "Compound splitters started..." > ${LOGDIR}/compound-splitter.log
compound-splitters-nl-socket >> ${LOGDIR}/compound-splitter.log 2>&1
