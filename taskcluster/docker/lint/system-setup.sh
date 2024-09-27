#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('iproute2')
apt_packages+=('locales')
apt_packages+=('m4')
apt_packages+=('fzf')
apt_packages+=('graphviz')
apt_packages+=('python3-pip')
apt_packages+=('python-is-python3')
apt_packages+=('shellcheck')
apt_packages+=('sudo')
apt_packages+=('wget')
apt_packages+=('unzip')
apt_packages+=('tar')
apt_packages+=('zstd')

apt-get update
apt-get install "${apt_packages[@]}"

# Without this we get spurious "LC_ALL: cannot change locale (en_US.UTF-8)" errors,
# and python scripts raise UnicodeEncodeError when trying to print unicode characters.
locale-gen en_US.UTF-8
dpkg-reconfigure locales

su -c 'git config --global user.email "worker@mozilla.test"' worker
su -c 'git config --global user.name "worker"' worker

cd /build

###
# ESLint Setup
###

# install node
# shellcheck disable=SC1091
. install-node.sh

npm install -g yarn@1.22.18

###
# codespell Setup
###

cd /setup

pip3 install --break-system-packages --require-hashes -r /tmp/codespell_requirements.txt

###
# tox Setup
###

cd /setup

pip3 install --break-system-packages --require-hashes -r /tmp/tox_requirements.txt

cd /
rm -rf /setup
