# BUILD

source lamachine-activate

LAMACHINE_HOSTNAME=$(lamachine-config hostname | cut -c11-)

echo "flaturl: https://$LAMACHINE_HOSTNAME/flat
forcehttps: true" | sudo tee /usr/local/etc/clam_base.config.yml

sudo chown -R lamachine:lamachine /usr/local/src/tscan
cp -R /deployment/roles/* /usr/local/src/LaMachine/roles

lamachine-add tscan
lamachine-update --only tscan

# clean up
rm -rf /usr/local/src/tscan/data
