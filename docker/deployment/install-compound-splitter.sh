SPLITTERDIR=/data/compound-splitter

# retrieve source again, make sure to clear prepared binaries
# otherwise an old version of a splitter method might linger
if [[ -d $SPLITTERDIR ]]
then
    sudo rm -rf $SPLITTERDIR
fi

cd /data
git clone https://github.com/UUDigitalHumanitieslab/compound-splitter
cd $SPLITTERDIR

# put dependencies in shared folder
# this way a restart doesn't need to retrieve all this data
# from scratch again
mkdir -p /data/compound-dependencies
ln -s /data/compound-dependencies dependencies

source lamachine-activate
pip3 install -r requirements.txt

python3 retrieve.py
python3 prepare.py

sudo python3 setup.py install
