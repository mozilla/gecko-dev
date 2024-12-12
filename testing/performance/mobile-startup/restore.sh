#!/bin/bash

#name: tab-restore-shopify
#owner: perftest
#description: Runs the shopify mobile restore test for chrome/fenix

# Path to the Python script
SCRIPT_PATH="testing/performance/mobile-startup/android_startup_videoapplink.py"

# Run the Python script
$PYTHON_PATH_SHELL_SCRIPT $SCRIPT_PATH $APP mobile_restore
