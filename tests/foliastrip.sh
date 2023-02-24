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

if [ $# -ne 1 ]
  then
      echo "one argument expected"
      exit 1
fi

$exe --strip "$1"
