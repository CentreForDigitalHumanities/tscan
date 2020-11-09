# BUILD
source lamachine-activate
sudo chown -R lamachine:lamachine /usr/local/src/tscan
cp -R /deployment/ansible/* /usr/local/src/LaMachine/roles/tscan

lamachine-add alpino # TODO: in separate file for caching
lamachine-add tscan
lamachine-update --only alpino,tscan

# clean up
rm -rf /usr/local/src/tscan/data
