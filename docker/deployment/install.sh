#!/usr/bin/env bash
cd /src
dpkg -i *.deb
# This data is used through a volume binding
rm -rf /usr/local/share/tscan

cd /src/tscan/webservice
pip install .
ln -s /usr/local/lib/python3.*/dist-packages/clam /opt/clam

cd /src/compound-splitter
pip install dist/*.zip

# Patch to set proper mimetype for CLAM's logs; maximum upload size
sed -i 's/txt;/txt log;/' /etc/nginx/mime.types
sed -i 's/xml;/xml xsl;/' /etc/nginx/mime.types
sed -i 's/client_max_body_size 1m;/client_max_body_size 1000M;/' /etc/nginx/nginx.conf
