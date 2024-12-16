#!/bin/bash

#name: applink-startup-navigation-start
#owner: perftest
#description: Runs the Cold View Nav Start startup(cvns) test for fenix/focus/geckoview

# Path to the Python script
SCRIPT_PATH="testing/performance/mobile-startup/android_startup_cmff_cvns.py"

# Run the Python script
$PYTHON_PATH_SHELL_SCRIPT $SCRIPT_PATH $APP cold_view_nav_start
