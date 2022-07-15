#!/bin/bash
source lamachine-activate

bash /deployment/store-compound-splitter-dependencies.sh

# work around for https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=934731
sudo chmod -f 600 /run/uwsgi-emperor.pid
sudo lamachine-stop-webserver
sudo pkill uwsgi

# sudo for moving data-folder required
sudo lamachine-start-webserver

# keep it open
bash
