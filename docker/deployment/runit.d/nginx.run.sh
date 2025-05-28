#!/bin/sh

# This script is meant to be invoked via runit (installed in /etc/service/nginx/run), not directly

mkdir -p ${LOGDIR=/var/log}/nginx
LOGFILE_ACCESS=${LOGDIR=/var/log}/nginx/access.log
LOGFILE_ERROR=${LOGDIR=/var/log}/nginx/error.log
touch $LOGFILE_ACCESS
touch $LOGFILE_ERROR
nginx -g "daemon off; error_log $LOGFILE_ERROR info;"
