#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('locales')
apt_packages+=('python3-pip')
apt_packages+=('python3-aiohttp')
apt_packages+=('shellcheck')
apt_packages+=('sudo')

apt-get update
apt-get install "${apt_packages[@]}"

su -c 'git config --global user.email "worker@mozilla.test"' worker
su -c 'git config --global user.name "worker"' worker

rm -rf /setup
