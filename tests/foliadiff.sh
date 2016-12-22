# /bin/sh -x
if [ "$exe" = "" ]
then
    exe="$HOME/usr/local/bin/folialint"
    if [ ! -f $exe ]
    then
	exe="/exp/sloot/usr/local/bin/folialint"
	if [ ! -f $exe ]
	then
	    exe="/usr/local/bin/folialint"
	fi
    fi
fi

if [ "$exe" = "" ];
then
    "cannot locate folialint"
    exit 1
fi

if [ $# -ne 2 ]
  then
      echo "two arguments expected"
      exit 1
fi

t1=/tmp/${1##*/}.s1
t2=/tmp/${2##*/}.s2
$exe --strip "$1" > $t1
$exe --strip "$2" > $t2
diff -w --ignore-matching-lines=".?*-annotation .?*" --ignore-matching-lines=".*generator=.*" $t1 $t2
if [ $? -ne 0 ]
then
    exit 1
fi

exit 0
