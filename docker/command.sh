source lamachine-activate
# work around for https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=934731
sudo chmod -f 600 /run/uwsgi-emperor.pid

# # call moving data manually if it has been configured
# # (the lamachine-start-webserver script hasn't been updated
# # to reflect that)
# LAMACHINE_MOVE_SHARE_WWW_DATA=$(lamachine-config move_share_www_data | cut -c22-)
# if [ "$LAMACHINE_MOVE_SHARE_WWW_DATA" = "true" ]; then
#     /usr/local/bin/lamachine-move-www-data
# else
#     echo "www-data isn't going anywhere"
# fi

lamachine-start-webserver

# keep it open
bash
