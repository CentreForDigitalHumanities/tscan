# BUILD

source lamachine-activate

sudo chown -R lamachine:lamachine /usr/local/src/tscan

if [ -e clam_custom.config.yml ]; then
    echo "Set custom clam"
    cp -f clam_custom.config.yml /usr/local/etc/
    lamachine-config clam_include /usr/local/etc/clam_custom.config.yml
else
    echo "No custom clam_include found"
fi

# Make the install.yml point to the right host
cd usr/local/src/LaMachine
mv install.yml install.yml.bak
sed "s/hosts: localhost/hosts: develop/g" install.yml.bak > install.yml

lamachine-add tscan
lamachine-update --only alpino,tscan

# clean up
rm -rf /usr/local/src/tscan/data
