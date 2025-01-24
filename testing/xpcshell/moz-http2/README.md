# XPCShellTests with npm packages

XPCShellTests that spawn a node server may use non-vendored npm packages.
Make sure that the test using the npm packages has `usesNPM = true` added to `xpshell.toml`.

## Adding a new dependency

```
  cd testing/xpcshell/moz-http2
  npm install newPackageThatIsPublishedToNPM
  # commit package.json and package-lock.json
```

## Running a test that uses npm packages

Just run it normally. For example

```
./mach text netwerk/test/unit/test_node_execute_npm.js
```

`runxpcshelltests.py` will automatically install the npm packages when one of
the tests in the test list has `usesNPM = true`
