#!/bin/bash
export TARGET_VERSION="0.0.0"

export SRCDIR=/src
export SPLITTERDIR=${SRCDIR}/compound-splitter
export DEPDIR=${SRCDIR}/tscan/docker/data/compound-dependencies
export DIST=$(ls ${DEPDIR}/dist/*.tar.gz 2>/dev/null)
export DIST_VERSION=$(cat ${DEPDIR}/dist/.version 2>/dev/null)

if [[ ! -z "$DIST" && "$TARGET_VERSION" == "$DIST_VERSION" ]]
then
    echo "Existing dist reused"
    mkdir -p $SPLITTERDIR/dist
    cp $DIST $SPLITTERDIR/dist/
    exit 0
fi

mkdir -p $SPLITTERDIR/dist
cd $SPLITTERDIR/dist
wget https://github.com/UUDigitalHumanitieslab/compound-splitter/releases/download/$TARGET_VERSION/compound-splitters-nl-$TARGET_VERSION.tar.gz -O compound-splitters-nl.tar.gz
echo $TARGET_VERSION > .version
