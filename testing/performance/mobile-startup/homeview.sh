#!/bin/bash

#name: homeview-startup
#owner: perftest
#description: Runs the homeview startup test for chrome/fenix

# Path to the Python script
SCRIPT_PATH="testing/performance/mobile-startup/android_startup_videoapplink.py"

# Run the Python script
$PYTHON_PATH_SHELL_SCRIPT $SCRIPT_PATH $APP homeview_startup https://theme-crave-demo.myshopify.com