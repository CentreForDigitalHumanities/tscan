#!/usr/bin/env bash
directory=$1
repository=$2
version=$3

cd ~/.tscan-deps
if [ -d $directory ];
then
    cd $directory
    old_commit_hash=$(git rev-parse HEAD)
    if [[ -z $version || $version != $old_commit_hash ]];
    then
        git pull
    fi
else
    git clone $repository
    old_commit_hash='NEW'
    cd $directory
fi
if [[ ! -z $version && $version != $old_commit_hash ]];
then
    echo "Resetting to $version"
    git reset --hard $version
fi

commit_hash=$(git rev-parse HEAD)
# only build if something changed
if [[ $commit_hash != $old_commit_hash ]]; then
    echo "Building $1..."
    bash bootstrap.sh
    ./configure $OPENMPFLAG
    make
else
    echo "Skip building $1: nothing changed"
fi
# always install - the VM is blank
sudo make install
