#!/usr/bin/env bash
set -e
directory=$1
repository=$2
version=$3

cd /src
prebuilt="$(echo "/src/tscan/docker/data/packages/"$directory"_*.deb")"
if [[ -e $(echo $prebuilt) ]];
then
    echo "Installing pre-built package"
    dpkg -i $prebuilt

    # retain the prebuilt package in /src/
    mv $prebuilt .
else
    git clone $repository
    cd $directory
    old_commit_hash=$(git rev-parse HEAD)

    if [[ -z $version ]];
    then
        echo "Using latest version"
    elif [[ $version != $old_commit_hash ]];
    then
        echo "Resetting to $version"
        git reset --hard $version
    fi

    echo "Building $1..."
    bash bootstrap.sh
    ./configure
    make

    checkinstall -y

    # collect all the packages in the /src/ folder
    mv *.deb ..
fi
