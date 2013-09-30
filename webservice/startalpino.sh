#! /bin/sh
export PORT=6666
export TIMEOUT=60000
export MEMLIMIT=1500M
export TMPDIR=/tmp

## this one is for parsing

PROLOGMAXSIZE=${MEMLIMIT} Alpino -notk -fast user_max=${TIMEOUT}\
 server_kind=parse\
 server_port=${PORT}\
 assume_input_is_tokenized=on\
 debug=1\
 end_hook=xml\
-init_dict_p\
 batch_command=alpino_server 2> ${TMPDIR}/alpino_server.log &

echo "Alpino started"
