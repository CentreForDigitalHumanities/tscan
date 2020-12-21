#!/bin/bash
VENV_PATH=$PWD/.venv
if [[ -z $LM_PREFIX ]]
then
    LOGDIR=/tmp
else
    LOGDIR=${LM_PREFIX}/var/log
fi
SPLITTERDIR=$PWD/tscanservice/compound_splitter

# retrieve source
if [[ -d $SPLITTERDIR ]]
then
    cd $SPLITTERDIR
    git pull
else
    cd tscanservice
    git clone https://github.com/UUDigitalHumanitieslab/compound-splitter
    cd $SPLITTERDIR

    if [[ ! -z $LM_DATA_PATH ]]
    then
        # link dependencies folder to shared (LaMachine)
        mkdir -p $LM_DATA_PATH/compound-dependencies
        ln -s $LM_DATA_PATH/compound-dependencies dependencies
    fi
fi

if [[ ! -d $VENV_PATH ]]
then
    python3 -m venv $VENV_PATH
fi

source $VENV_PATH/bin/activate

# install requirements
pip install -r requirements.txt

# retrieve dependencies
python retrieve.py

# start API (localhost:7005)
python -m compound_splitter.socket_server 2> $LOGDIR/compound-splitter.log
