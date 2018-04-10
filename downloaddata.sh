#!/bin/bash
cd data/
if [ $? -ne 0 ]; then
    echo "Run this script from the t-scan directory"
    exit 2
fi

osname=`uname`
case ${osname} in
    Linux*)
	mystat='stat -c "%X"'
	;;
    Darwin*)
	mystat='stat -f "%X"'
	;;
esac

echo ${mystat}

echo "Downloading big data"
PREDOWNLOAD=`$mystat tscan-bigdata.tar.bz2 2> /dev/null`
echo pre=$PREDOWNLOAD
wget -c -N https://resources.lab.hum.uu.nl/resources/tscan/tscan-bigdata.tar.bz2
POSTDOWNLOAD=`mystat tscan-bigdata.tar.bz2 2> /dev/null`
echo post=$POSTDOWNLOAD

if [ "$PREDOWNLOAD" != "$POSTDOWNLOAD" ]; then
    echo "extracting the big data"
    tar -xvjf tscan-bigdata.tar.bz2
fi

#Dummies for RBN-derived data, not redistributable due to overly restrictive license (http://tst.inl.nl/producten/rbn/toonlicentie.php)
RESTRICTEDDATA="adjs_semtype.data general_nouns.data general_verbs.data intensiveringen.data nouns_semtype.data verbs_semtype.data"
for file in $RESTRICTEDDATA; do
    if [ ! -f $file ]; then
        touch $file
    fi
done
