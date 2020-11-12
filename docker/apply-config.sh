source lamachine-activate

if [ ! -z "$PYTHONPATH" ]; then
    OLDPYTHONPATH="$PYTHONPATH"
    export PYTHONPATH=""
fi

# Force updating the lamachine-core
# (to update the start webserver script and CLAM configuration)
cd "/usr/local/src/LaMachine"
echo "---
- hosts: develop
  roles: [ lamachine-core ]
" > "install.tmp.yml"

ansible-playbook -i "hosts.ini" "install.tmp.yml" -v --tags "webserver-start,webserver-clam-base" --extra-vars "ansible_python_interpreter='$(which python3)'"

if [ ! -z "$OLDPYTHONPATH" ]; then
    export PYTHONPATH="$OLDPYTHONPATH"
fi
