MY_PATH=$(dirname ${BASH_SOURCE[0]})
for diff in $MY_PATH/*.diff
do
    if [[ ! -e "$diff" ]]
    then
        echo "no errors logged"
        exit 0
    fi

    err=$(echo $diff | sed 's|\.diff|\.err|')
    echo "
    
    DIFFERENCES $diff
    
    "
    cat "$diff"

    echo "
    
    ERRORS $err
    
    "

    cat "$err"
done
