#!/bin/sh

# This script is meant to be invoked via runit (installed in /etc/service/nginx/run), not directly

touch /var/log/nginx/access.log
nginx -g 'daemon off; error_log /var/log/nginx/access.log info;'
