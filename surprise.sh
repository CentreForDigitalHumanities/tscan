#!/bin/bash

if [[ `hostname` == "applejack" ]]; then
	cd /scratch2/www/webservices-lst/live/repo/tscan/HumDep3.0/Code
else
	cd /home/sloot/HumDep3.0/Code
fi

./depParse -train Dutch -file $1

mv ../surprisal.txt $2
