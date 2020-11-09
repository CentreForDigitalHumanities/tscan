# TODO: this should be overwritable
source lamachine-activate
lamachine-config force_https true
lamachine-config hostname tst.tscan.hum.uu.nl

# TODO: this is probably some bug or configuration issue in LaMachine?
cp /usr/local/src/LaMachine/host_vars/develop.yml /usr/local/src/LaMachine/host_vars/localhost.yml
