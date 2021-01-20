#!/bin/bash
source lamachine-activate
export SRCDIR=${LM_PREFIX}/src
export DEPDIR=${SRCDIR}/compound-splitter/dependencies

echo "Moving updated dependencies from $DEPDIR"
mkdir -p /data/compound-dependencies
cp -up $DEPDIR/* /data/compound-dependencies/
