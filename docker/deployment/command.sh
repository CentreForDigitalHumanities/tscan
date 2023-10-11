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

# same user as dispatcher
sudo -u _apt LD_LIBRARY_PATH="$LD_LIBRARY_PATH" ./restart-projects.py &

# Configure services
cd runit.d
SERVICE_FOLDER=/etc/service
for service in $(ls *run.sh); do
    service_name="${service/\.run\.sh/}"
    mkdir -p $SERVICE_FOLDER/$service_name
    ln -sf $PWD/$service $SERVICE_FOLDER/$service_name/run
done

envsubst '$URLPREFIX' < /deployment/tscan_webservice.nginx.conf > /etc/nginx/sites-enabled/default

runsvdir -P /etc/service
