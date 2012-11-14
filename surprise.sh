# /bin/sh

cd /home/sloot/HumDep3.0/Code

./depParse -train Dutch -file $1

mv ../surprisal.txt $2