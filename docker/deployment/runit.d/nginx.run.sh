#!/bin/sh

# This script is meant to be invoked via runit (installed in /etc/service/nginx/run), not directly

LOGFILE=${LOGDIR=/var/log}/nginx/access.log
touch $LOGFILE
nginx -g "daemon off; error_log $LOGFILE info;"
