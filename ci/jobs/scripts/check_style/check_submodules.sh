#!/usr/bin/env bash

# The script checks if all submodules defined in .gitmodules exist in contrib/,
# validates their URLs and names, and ensures there are no recursive submodules.

set -e

cd "$(git rev-parse --show-toplevel)"

# For each registered submodule: ensure its bare repo is present and that
# the pinned commit does not pull in nested submodules. We read .gitmodules
# directly from the bare repo so we don't need the submodule working tree.
# Process substitution (not a pipe) so `exit 1` aborts the whole script.
while IFS= read -r -d '' submodule_path; do
    if ! test -d "$submodule_path"; then
        echo "Directory for submodule $submodule_path is not found"
        exit 1
    fi
    # An initialized submodule has its bare repo at .git/modules/<path>;
    # without it, we cannot inspect the pinned commit.
    submodule_git_dir=".git/modules/$submodule_path"
    if [ ! -d "$submodule_git_dir" ]; then
        echo "Submodule $submodule_path is not initialized; run 'git submodule init'."
        exit 1
    fi
    if git --git-dir="$submodule_git_dir" show HEAD:.gitmodules 2>/dev/null | grep -q '\[submodule'; then
        echo "Recursive submodules are not allowed: $submodule_path contains its own .gitmodules with submodule entries"
        exit 1
    fi
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
