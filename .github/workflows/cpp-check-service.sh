#!/usr/bin/env bash
port=$1
echo "Checking port $port"
for i in {1..6}
do
    echo -e '\x1dclose\x0d' | telnet localhost $port
    if [ "$?" == "0" ]; then
        exit 0
    fi
    sleep 10
done
exit 1
