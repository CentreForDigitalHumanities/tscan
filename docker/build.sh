# BUILD
source lamachine-activate
sudo chown -R lamachine:lamachine /usr/local/src/tscan
cp -R /deployment/ansible/* /usr/local/src/LaMachine/roles/tscan

lamachine-add tscan
lamachine-update --only tscan

# clean up
rm -rf /usr/local/src/tscan/data
