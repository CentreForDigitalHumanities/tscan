# Convenience method for on the server:
# By placing alpino.tar.gz.o in the data/ folder the download
# can be skipped
if [ -e /usr/local/src/tscan/data/alpino.tar.gz.o ]; then
    cd /usr/local/src
    echo "Placing pre-loaded Alpino"
    # This fools the version check:
    wget http://www.let.rug.nl/vannoord/alp/Alpino/versions/binary/ -O .alpino_index_old
    tar -xvzf /usr/local/src/tscan/data/alpino.tar.gz.o -C /usr/local/opt/
fi

# source lamachine-activate
# lamachine-add alpino
# lamachine-update --only alpino
