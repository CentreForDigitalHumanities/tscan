# /bin/sh

if [ "$tscan_bin" = "" ];
then echo "tscan_bin not set";
     exit;
fi

OK="\033[1;32m OK  \033[0m"
FAIL="\033[1;31m  FAILED  \033[0m"
KNOWNFAIL="\033[1;33m  KNOWN FAILURES  \033[0m"

export comm="$VG $tscan_bin/tscan"

for file in $1
do if test -e $file
    then 
	\rm -f $file.diff
	\rm -f $file.err
	\rm -f $file.out
	\rm -f $file.tscan.xml
	echo -n "Tscanning  $file "
	$comm -t $file > $file.out 2> $file.err
	#xmldiff /tmp/folia.2.xml $file.ok >& $file.diff
	diff -w $file.tscan.xml $file.ok >& $file.diff
	if [ $? -ne 0 ];
	then 
	    echo -e $FAIL;	
	    echo "differences logged in $file.diff";
	else
	  echo -e $OK
	  rm $file.diff
	fi
    else
	echo "file $file not found"
    fi
done
