#!/bin/bash



if [[ `hostname` == "applejack" ]]; then
	export LD_LIBRARY_PATH="/scratch2/www/webservices-lst/live/repo/tscan/decompounder:$LD_LIBRARY_PATH"
        cd /scratch2/www/webservices-lst/live/repo/tscan/decompounder
else
	export LD_LIBRARY_PATH="/home/sloot/svn/decompounder:$LD_LIBRARY_PATH"
	cd /home/sloot/svn/decompounder
fi

./PolderDecompounder <$1 > $2
