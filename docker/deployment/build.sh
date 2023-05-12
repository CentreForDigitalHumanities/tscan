#!/bin/bash
# BUILD

cd /src/tscan/
# might already have been built on the host
make clean
bash bootstrap.sh
./configure
make

checkinstall -y

# collect all the packages in the /src/ folder
mv *.deb ..
