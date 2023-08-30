#!/bin/bash
export SRCDIR=/src
export SPLITTERDIR=${SRCDIR}/compound-splitter
export DEPDIR=${SRCDIR}/tscan/docker/data/compound-dependencies
export DIST=$(ls ${DEPDIR}/dist/*.zip 2>/dev/null)

# retrieve source again, make sure to clear prepared binaries
# otherwise an old version of a splitter method might linger
if [[ -d $SPLITTERDIR ]]
then
    rm -rf $SPLITTERDIR
fi

if [[ ! -z "$DIST" ]]
then
    echo "Existing dist reused"
    mkdir -p $SPLITTERDIR/dist
    cp $DIST $SPLITTERDIR/dist/
    exit 0
fi

cd $SRCDIR
git clone https://github.com/UUDigitalHumanitieslab/compound-splitter
cd $SPLITTERDIR
# TODO: pinned to latest version known to work
git checkout 3f8b787471f9e03215d7b2a78a94a76e4e039bcc

# dependencies might already have been retrieved in the tscan clone
# itself (tscan/docker/data/compound-dependencies)
# this way a rebuild doesn't need to retrieve all this data from
# scratch again
if [[ -d $DEPDIR ]]
then
    echo "Existing dependencies reused"
    ls -l $DEPDIR
    mkdir -p dependencies
    mv $DEPDIR/* dependencies
fi

pip install -r requirements.txt

python3 retrieve.py
python3 prepare.py

python3 setup.py sdist --formats zip
