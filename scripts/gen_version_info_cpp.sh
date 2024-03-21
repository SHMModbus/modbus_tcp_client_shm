#!/bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
cd $SCRIPTPATH/..

if [ $# -ne 1 ]; then
    >&2 echo "usage: $0 project_name"
fi

# "$(git rev-parse HEAD)$(test $(git status --porcelain | wc -l) -gt 0 && printf -- -dirty)"
GIT_HASH=$(git rev-parse HEAD)
GIT_DIRTY=$(git status --porcelain | wc -l)

if [ $GIT_DIRTY -ne 0 ]; then
    GIT_HASH="${GIT_HASH}-dirty"
fi

sed \
  -e "s/###PROJECT_NAME###/$1/g" \
  -e "s/###GIT_HASH###/$GIT_HASH/g" \
  code_generation_templates/version_info.hpp > src/generated/version_info.hpp

sed \
  -e "s/###PROJECT_NAME###/$1/g" \
  -e "s/###GIT_HASH###/$GIT_HASH/g" \
  code_generation_templates/version_info.cpp > src/generated/version_info.cpp
