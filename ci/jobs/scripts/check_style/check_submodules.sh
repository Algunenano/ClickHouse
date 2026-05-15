#!/usr/bin/env bash

# The script checks if all submodules defined in .gitmodules exist in contrib/,
# validates their URLs and names, and ensures there are no recursive submodules.

set -e

cd "$(git rev-parse --show-toplevel)"

# Check that every submodule directory exists and has a valid status.
# Process substitution (not a pipe) so `exit 1` aborts the whole script.
while IFS= read -r -d '' submodule_path; do
    if ! test -d "$submodule_path"; then
        echo "Directory for submodule $submodule_path is not found"
        exit 1
    fi
    git submodule status -q "$submodule_path"
done < <(git config --file .gitmodules --null --get-regexp path | sed -z 's|.*\n||')

# All submodules should be from https://github.com/
while read -r line; do
    name=${line#submodule.}; name=${name%.url*}
    url=${line#* }
    if [[ "$url" != 'https://github.com/'* ]]; then
        echo "All submodules should be from https://github.com/, submodule '$name' has '$url'"
        exit 1
    fi
done < <(git config --file .gitmodules --get-regexp 'submodule\..+\.url')

# All submodules should be of this form: [submodule "contrib/libxyz"]
# (for consistency, the submodule name should equal its path)
while read -r line; do
    name=${line#submodule.}; name=${name%.path*}
    path=${line#* }
    if [ "$name" != "$path" ]; then
        echo "Submodule name '$name' is not equal to its path '$path'"
        exit 1
    fi
done < <(git config --file .gitmodules --get-regexp 'submodule\..+\.path')

# No recursive submodules allowed: check that no submodule contains its own
# .gitmodules with entries.
while IFS= read -r -d '' submodule_path; do
    if [ -f "$submodule_path/.gitmodules" ] && grep -q '\[submodule' "$submodule_path/.gitmodules"; then
        echo "Recursive submodules are not allowed: $submodule_path contains its own .gitmodules with submodule entries"
        exit 1
    fi
done < <(git config --file .gitmodules --null --get-regexp path | sed -z 's|.*\n||')
