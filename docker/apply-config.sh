source lamachine-activate

if [ ! -z "$PYTHONPATH" ]; then
    OLDPYTHONPATH="$PYTHONPATH"
    export PYTHONPATH=""
fi

echo "---
- hosts: develop
  roles: [ lamachine-core ]" > "install.tmp.yml"

# Force updating the lamachine-core
# (to update the start webserver script and CLAM configuration)
cd "/usr/local/src/LaMachine"
ansible-playbook -i "hosts.ini" "install.tmp.yml" -v --extra-vars "ansible_python_interpreter='$(which python3)'"

if [ ! -z "$OLDPYTHONPATH" ]; then
    export PYTHONPATH="$OLDPYTHONPATH"
fi
