#!/bin/bash

#name: applink-startup-first-frame
#owner: perftest
#description: Runs the Cold Main First Frame startup(cmff) test for chrome/fenix/focus/geckoview

# Path to the Python script
SCRIPT_PATH="testing/performance/mobile-startup/android_startup_cmff_cvns.py"

# Run the Python script
$PYTHON_PATH_SHELL_SCRIPT $SCRIPT_PATH $APP cold_main_first_frame
