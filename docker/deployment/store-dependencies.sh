#!/bin/bash
export SRCDIR=/src
export DEPDIR=${SRCDIR}/compound-splitter/dependencies

echo "Moving updated dependencies from $DEPDIR"
mkdir -p /data/compound-dependencies
cp -up $DEPDIR/* /data/compound-dependencies/

mkdir -p /data/packages
mv ${SRCDIR}/*.deb /data/packages/

mkdir -p /data/compound-dependencies/dist
mv ${SRCDIR}/compound-splitter/dist/compound-splitters-nl*.zip /data/compound-dependencies/dist/compound-splitters-nl.zip

mkdir -p /data/build-cache
mv ${SRCDIR}/tscan/src/*.o /data/build-cache/
