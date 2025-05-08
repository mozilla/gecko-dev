# Url-Classifier Performance Microbenchmarks

This folder contains micro benchmarks using the [mozperftest][] suite.

[mozperftest](https://firefox-source-docs.mozilla.org/testing/perfdocs/mozperftest.html)

## Recording profiles for the Firefox Profiler

```sh
# Run the perftest as an xpcshell test.
MOZ_PROFILER_STARTUP=1 \
  MOZ_PROFILER_SHUTDOWN=/path/to/perf-profile.json \
  ./mach xpcshell-test toolkit/components/url-classifier/tests/performance/perftest_exceptionListLookup.js

# Install the profiler-symbol-server tool.
cargo install profiler-symbol-server

# Open the path to the file.
profiler-symbol-server /path/to/perf-profile.json
```
