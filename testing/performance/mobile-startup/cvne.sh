#!/bin/bash

#name: cold_view_nav_end mobile startup script
#owner: perftest
#description: Runs the cold_view_nav_end(cvne) startup app link test for fenix

# Path to the Python script
SCRIPT_PATH="testing/performance/mobile-startup/android_startup_videoapplink.py"

# Run the Python script
$PYTHON_PATH_SHELL_SCRIPT $SCRIPT_PATH $APP
