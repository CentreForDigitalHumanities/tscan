#!/bin/bash
cd /deployment
./store-dependencies.sh

# Default values
export UWSGI_UID=$(id -u)
export UWSGI_GID=$(id -g)
export URLPREFIX=
export TSCAN_DIR=/usr/local/bin
export TSCAN_DATA=/usr/local/share/tscan
export TSCAN_SRC=/src/tscan
export CLAM_DEBUG=false
export CLAM_OAUTH_CLIENT_URL=
export CLAM_OAUTH_REVOKE_URL=
# This must be a well-formed JSON!
export CLAM_OAUTH_SCOPE="{}"
export CLAM_CUSTOMHTML_INDEX=
export CLAM_CUSTOMHTML_PROJECTSTART=
export CLAM_CUSTOMHTML_PROJECTDONE=
export CLAM_CUSTOMHTML_PROJECTFAILED=
export CLAM_INTERFACEOPT=

if [[ -e config.sh ]];
then
    source config.sh
fi

cd /src/tscan/webservice
./startalpino.sh &
./startcompound.sh &
./startfrog.sh &
./startwopr02.sh &
./startwopr20.sh &

# Configure webserver and uwsgi server
mkdir -p /etc/service/nginx /etc/service/uwsgi
ln -sf /deployment/runit.d/nginx.run.sh /etc/service/nginx/run
ln -sf /deployment/runit.d/uwsgi.run.sh /etc/service/uwsgi/run
envsubst '$URLPREFIX' < /deployment/tscan_webservice.nginx.conf > /etc/nginx/sites-enabled/default

runsvdir -P /etc/service
