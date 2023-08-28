# /bin/sh -x

if [ "$tscan_bin" = "" ];
then echo "tscan_bin not set";
	exit;
fi

OK="\033[1;32m OK  \033[0m"
FAIL="\033[1;31m  FAILED  \033[0m"
KNOWNFAIL="\033[1;33m  KNOWN FAILURES  \033[0m"

export comm="$VG $tscan_bin/tscan"

function do_test() {
	local file="$1"
	\rm -f $file.diff
	\rm -f $file.err
	\rm -f $file.out
	\rm -f $file.tscan.xml
	\rm -f $file.*.alpino.xml
	echo -n "Tscanning  $file "
	$comm --config=tscan.cfg --skip=c -t $file > $file.out 2> $file.err
	./foliadiff.sh $file.tscan.xml $file.ok >& $file.diff
	if [ $? -ne 0 ];
	then
		echo -e $FAIL;
		echo "differences logged in $file.diff";
		return 1
	else
		echo -e $OK
		rm $file.diff
		return 0
	fi
}

result=0
attempt=0
for file in $@
do if test -e $file
then
	file_result=1
	while [ $attempt -le 3 ]
	do
		if [ $attempt -gt 0 ];
		then
			echo "retry..."
			sleep 5
		fi
		do_test "$file"
		file_result=$?
		if [ $file_result -eq 0 ];
		then
			break
		fi
		attempt=$((attempt + 1))
	done
else
	echo "file $file not found"
	result=1
fi
done
exit $result
