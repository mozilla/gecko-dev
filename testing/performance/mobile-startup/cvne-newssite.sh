#!/bin/bash

#name: newssite-applink-startup
#owner: perftest
#description: Runs the newssite applink startup(cvne) test for chrome/fenix

# Path to the Python script
SCRIPT_PATH="testing/performance/mobile-startup/android_startup_videoapplink.py"

# Start up a local http server.
$PYTHON_PATH_SHELL_SCRIPT -m http.server --directory testing/performance/mobile-startup/newssite-nuxt \
    > $TESTING_DIR/server.log 2>&1 &
SERVER_PID=$!

echo "HTTP server started with PID $SERVER_PID, logs are in server.log"

# Reroute localhost:8000 on the device to the host.
adb reverse tcp:8000 tcp:8000

# Run the Python script
$PYTHON_PATH_SHELL_SCRIPT $SCRIPT_PATH $APP cold_view_nav_end http://localhost:8000

# Remove all reverse rules
adb reverse --remove-all

# Kill python server
kill $SERVER_PID
