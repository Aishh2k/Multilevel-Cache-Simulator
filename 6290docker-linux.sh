#!/bin/bash
# Github repo: https://github.gatech.edu/cs6290/cs6290-docker (PRs welcome)
set -e

### Change this to somewhere else if you want. By default, this is mounting
### your current directory in the container when you run this script
workdir=$(pwd)
image=ausbin/cs6290

if ! id -nzG | grep -qzxF docker; then
    # If not running as the docker group, re-exec myself
    exec sudo -g docker "$0" "$@"
fi

mkdir -p "$workdir"

# Don't pull every time to avoid rate limits
if [[ $1 = --pull ]] || ! docker image inspect "$image" &>/dev/null; then
    docker pull "$image"
fi

exec docker run -it --rm -v "$workdir:/home/student/workdir" "$image"