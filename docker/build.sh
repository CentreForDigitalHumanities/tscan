# BUILD

source lamachine-activate

sudo chown -R lamachine:lamachine /usr/local/src/tscan

if [ -e clam_custom.config.yml ]; then
    cp -f clam_custom.config.yml /usr/local/etc/
    lamachine-config clam_include /usr/local/etc/clam_custom.config.yml
else
    echo "No custom clam_include found"
fi

lamachine-add tscan
lamachine-update --only alpino,tscan

# clean up
rm -rf /usr/local/src/tscan/data
