#!/usr/bin/env bash
set -e
directory=$1
repository=$2
version=$3

if [[ -z $version && $version != "latest" ]];
then
    # use the dependencies
    source dependencies.cfg
    version="${!directory}"
fi

cd ~/.tscan-deps
if [ ! -d $directory ];
then
    git clone $repository
    new=1
else
    new=0
fi
cd $directory
old_commit_hash=$(git rev-parse HEAD)

if [[ -z $version || $version == "latest" ]];
then
    echo "Get latest version"
    git pull
elif [[ $version != $old_commit_hash ]];
then
    echo "Resetting to $version"
    git pull
    git reset --hard $version
else
    echo "No need to reset: $old_commit_hash"
fi

commit_hash=$(git rev-parse HEAD)
# only build if something changed
if [[ $new == 1 || $commit_hash != $old_commit_hash ]]; then
    echo "Building $1..."
    bash bootstrap.sh
    ./configure $OPENMPFLAG
    make
else
    echo "Skip building $1: nothing changed"
fi
# always install - the VM is blank
sudo make install
