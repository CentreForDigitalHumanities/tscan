#!/usr/bin/env bash
alpino=$1

cd ~/.tscan-deps
if [ -e alpino.tar.gz ]; then
    mv alpino.tar.gz "$alpino.tar.gz"
fi
if [ ! -e "$alpino.tar.gz" ]; then
    wget http://www.let.rug.nl/vannoord/alp/Alpino/versions/binary/$alpino.tar.gz
    if [ "$?" != "0" ]; then
        exit 1
    fi
else
    echo "Alpino archive already in cache"
fi
rm -rf Alpino
tar -xvzf "$alpino.tar.gz"

# remove other versions
find Alpino*.tar.gz -type f -not -name "$alpino.tar.gz" -print0 | xargs -0 -I {} rm {}
