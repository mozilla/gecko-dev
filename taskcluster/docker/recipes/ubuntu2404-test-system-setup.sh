#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

# Cleanup
cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
rm -f "$0"
