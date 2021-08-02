#!/bin/bash

# https://stackoverflow.com/questions/4774054/reliable-way-for-a-bash-script-to-get-the-full-path-to-itself/4774063
SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cd ${SCRIPTPATH}/..

if clang=$(which clang-format) ; then
    echo "using $clang to format the files"
else
    echo "clang-format missing!"
    exit 1
fi

echo "Format cpps: "
find src -type f -name "*.cpp" | while read file; do
    printf "... $file ..."
    if clang-format -style=file -i $file ; then
        printf " OK\n"
    else
        printf " FAILED\n"
    fi
done

echo "Format hpps: "
find src -type f -name "*.hpp" | while read file; do
    printf "... $file ..."
    if clang-format -style=file -i $file ; then
        printf " OK\n"
    else
        printf " FAILED\n"
    fi
done
