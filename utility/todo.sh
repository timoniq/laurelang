#!bin/bash

if [ -z $1 ]; then 
    echo "Usage:"
    echo "  todo.sh {path}"
    exit 0
fi

nc="\033[0m"
gray="\033[0;37m"
yellow="\033[0;33m"

echo
echo "[\033[0;32mtodo.sh$nc] Searching unfinished tasks..."
echo

smt_to_do=false

find $1 -type f \( -iname \*.c -o -iname \*.h \) | while read f; do
    todo=$(egrep --include=\*.{c,h} -in "//! todo|//! " "$f")

    if [ ! -z "$todo" ]; then
        smt_to_do=true
        printf $f
        echo $todo | perl -pe "s/([0-9]+)\:\ \/\/\! todo/\n $nc\$1$gray:$yellow/g" | perl -pe "s/([0-9]+)\:\ \/\/\! /\n $nc\$1$gray: /g"
        printf $nc
        echo
    fi

done

echo "That's all for today!"