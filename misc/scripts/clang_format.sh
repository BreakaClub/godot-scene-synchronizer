#!/usr/bin/env bash

set -e

while IFS= read -r -d '' filepath; do
    clang-format --style=file:godot-cpp/.clang-format --Wno-error=unknown -i "$filepath"
done < <(git ls-files -z -- '*.c' '*.h' '*.cpp' '*.hpp')

diff=$(git diff --color)

if [ -n "$diff" ] ; then
    echo "Amend your commit to comply with the Godot formatting rules:\n\n$diff"
    exit 1
fi
