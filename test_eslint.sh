#!/bin/bash

# if you already have firefox built once before
#./mach build faster

# if you have  never built firefox, can take hours
#./mach build

echo 'Creating directory test_results if it does not exist for test results'
mkdir -p test_results

# test eslint on Frame.js file
echo 'Testing eslint...'
npx eslint ./devtools/client/shared/components/Frame.js > 'test_results/test_eslint_log.txt'
# test to check if Frame.js still works
echo 'Running Frame tests...'
./mach mochitest devtools/client/shared/components/test/chrome/test_frame_01.html >> 'test_results/test_eslint_frame_01.txt'
./mach mochitest devtools/client/shared/components/test/chrome/test_frame_02.html >> 'test_results/test_eslint_frame_02.txt'

if grep -Fxq "Unexpected results: 0" "test_results/test_eslint_frame_01.txt"
then
	echo 'frame_01 test passed!'
else
	echo 'frame_01 test failed!'
fi

if grep -Fxq "Unexpected results: 0" "test_results/test_eslint_frame_02.txt"
then
	echo 'frame_02 test passed!'
else
	echo 'frame_02 test failed!'
fi

if [ -s test_eslint_log.txt ]
then
	echo 'eslint test failed!'
else
	echo 'eslint test passed!'
fi