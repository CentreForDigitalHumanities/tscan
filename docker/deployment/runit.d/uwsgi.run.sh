#!/bin/sh

# This script is meant to be invoked via runit (installed in /etc/service/nginx/run), not directly
if [ -e /deployment/clam_custom.config.yml ];
then
    export CONFIGFILE=/deployment/clam_custom.config.yml
fi
uwsgi --plugin python3 \
      --uid ${UWSGI_UID} \
      --gid ${UWSGI_GID} \
      --master \
      --socket "127.0.0.1:8888" \
      --wsgi-file /src/tscan/webservice/tscanservice/tscan.wsgi \
      --processes ${UWSGI_PROCESSES:-2} \
      --threads ${UWSGI_THREADS:-2} \
      --manage-script-name
