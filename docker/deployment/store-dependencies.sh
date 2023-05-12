#!/bin/bash
export SRCDIR=/src
export DEPDIR=${SRCDIR}/compound-splitter/dependencies

echo "Moving updated dependencies from $DEPDIR"
mkdir -p /data/compound-dependencies
cp -up $DEPDIR/* /data/compound-dependencies/

mkdir -p /data/packages
mv ${SRCDIR}/*.deb /data/packages/
