#!/bin/bash
cd data/
if [ $? -ne 0 ]; then
    echo "Run this script from the t-scan directory"
    exit 2
fi

echo "Downloading big data"
PREDOWNLOAD=`stat --printf='%X' tscan-bigdata.tar.bz2 2> /dev/null`
wget -c -N http://lst.science.ru.nl/~proycon/tscan-bigdata.tar.bz2
POSTDOWNLOAD=`stat --printf='%X' tscan-bigdata.tar.bz2 2> /dev/null`
if [ "$PREDOWNLOAD" != "$POSTDOWNLOAD" ]; then
    tar -xvjf tscan-bigdata.tar.bz2
fi

#Dummies for RBN-derived data, not redistributable due to overly restrictive license (http://tst.inl.nl/producten/rbn/toonlicentie.php)
RESTRICTEDDATA="adjs_semtype.data  general_nouns.data  general_verbs.data  intensiveringen.data  nouns_semtype.data  verbs_semtype.data"
for file in $RESTRICTEDDATA; do
    if [ -f $file ]; then
        touch $file
    fi
done
