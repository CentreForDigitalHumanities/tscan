#!/bin/bash
export SRCDIR=/src
export DEPDIR=${SRCDIR}/compound-splitter/dependencies

echo "Moving updated dependencies from $DEPDIR"
mkdir -p /data/compound-dependencies
cp -up $DEPDIR/* /data/compound-dependencies/

mkdir -p /data/packages
mv ${SRCDIR}/*.deb /data/packages/

mkdir -p /data/compound-dependencies/dist
mv ${SRCDIR}/compound-splitter/dist/.version /data/compound-dependencies/dist/.version
mv ${SRCDIR}/compound-splitter/dist/compound-splitters-nl*.tar.gz /data/compound-dependencies/dist/compound-splitters-nl.tar.gz

mkdir -p /data/build-cache
mv ${SRCDIR}/tscan/src/*.o /data/build-cache/
