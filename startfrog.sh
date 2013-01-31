#! /bin/sh
PATH=/home/sloot/usr/local/
PORT=7345
ID=tscan
CONFIG=${PATH}/etc/frog/tscan-frog.cfg

${PATH}/bin/frog -X --id=${ID} --config=${CONFIG} --skip=mp -S${PORT}
