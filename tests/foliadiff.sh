#!/usr/bin/env bash
if [ $# -ne 2 ]
  then
      echo "two arguments expected"
      exit 1
fi

if [[ ! -f $1 ]]; then
    echo "$1 does not exist"
    exit 1
fi
if [[ ! -f $2 ]]; then
    echo "$2 does not exist"
    exit 1
fi

t1=/tmp/${1##*/}.s1
t2=/tmp/${2##*/}.s2
./foliastrip.sh "$1" > $t1
./foliastrip.sh "$2" > $t2
diff -w --ignore-matching-lines=".?*-annotation .?*" --ignore-matching-lines=".*generator=.*" $t1 $t2
if [ $? -ne 0 ]
then
    exit 1
fi

exit 0
