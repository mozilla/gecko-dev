#!/bin/bash

#name: applink-startup-cnve
#owner: perftest
#description: Runs the applink startup(cvne) test for chrome/fenix

# Path to the Python script
SCRIPT_PATH="testing/performance/mobile-startup/android_startup_videoapplink.py"

# Run the Python script
$PYTHON_PATH_SHELL_SCRIPT $SCRIPT_PATH $APP
