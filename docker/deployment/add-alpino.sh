# Convenience method for on the server:
# By placing alpino.tar.gz.o in the data/ folder the download
# can be skipped
if [ ! -e /src/tscan/data/alpino.tar.gz.o ]; then
    cd /src/tscan/data
    wget http://www.let.rug.nl/vannoord/alp/Alpino/versions/binary/latest.tar.gz -O alpino.tar.gz.o
fi
tar -xvzf /src/tscan/data/alpino.tar.gz.o -C /
