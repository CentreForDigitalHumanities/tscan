#!/bin/bash
source lamachine-activate
export SRCDIR=${LM_PREFIX}/src
export SPLITTERDIR=${SRCDIR}/compound-splitter

# retrieve source again, make sure to clear prepared binaries
# otherwise an old version of a splitter method might linger
if [[ -d $SPLITTERDIR ]]
then
    sudo rm -rf $SPLITTERDIR
fi

cd $SRCDIR
git clone https://github.com/UUDigitalHumanitieslab/compound-splitter
cd $SPLITTERDIR

# dependencies might already have been retrieved in the tscan clone
# itself (tscan/docker/data/compound-dependencies)
# this way a rebuild doesn't need to retrieve all this data from
# scratch again
export DEPDIR=${SRCDIR}/tscan/docker/data/compound-dependencies
if [[ -d $DEPDIR ]]
then
    echo "Existing dependencies reused"
    ls -l $DEPDIR
    mkdir -p dependencies
    sudo mv $DEPDIR/* dependencies
fi

pip3 install -r requirements.txt

python3 retrieve.py
python3 prepare.py

# compound-splitter runs from the src-folder (startcompound.sh)
