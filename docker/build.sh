# BUILD

source lamachine-activate

sudo chown -R lamachine:lamachine /usr/local/src/tscan

lamachine-add tscan
lamachine-update --only tscan

# clean up
rm -rf /usr/local/src/tscan/data
