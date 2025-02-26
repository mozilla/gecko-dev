# Build
The fuzz targets can be build with `./build.sh --fuzz [--disable-tests]`. They compile with ASan and UBSan by default, see `coreconf/fuzz.sh`.

# OSS-Fuzz
All fuzz targets run continuously on oss-fuzz, the respective `project.yaml` can be found at https://github.com/google/oss-fuzz/blob/master/projects/nss/project.yaml. An overview with code coverage is available at https://introspector.oss-fuzz.com/project-profile?project=nss, as well as a link to a more detailed fuzz introspector report.

# MozillaSecurity/orion
We regularly run two services, one to collect coverage information ourselves and another one to mirror the public oss-fuzz corpora and populate the private bucket with new testcases. Code coverage reports can be found at https://fuzzmanager.fuzzing.mozilla.org/covmanager/reports/.

- nss-coverage service: https://github.com/MozillaSecurity/orion/tree/master/services/nss-coverage
- nss-corpus-update service: https://github.com/MozillaSecurity/orion/tree/master/services/nss-corpus-update

# Adding a new fuzz target
The fuzz targets are located at `fuzz/targets`. Some additional things to keep in my mind when adding a new fuzz target:
- Every fuzz target needs a `.options` file at `fuzz/options`, other fuzz tooling depends on it.
- For CI integration, schedule the corresponding fuzzing runs at `automation/taskcluster/graph/src/extend.js`.
- Testcases can be extracted from the existing tests by adding hooks to `fuzz/config/frida_corpus/hooks.js` and `fuzz/config/frida_corpus/cli.py`.

# Useful Links
- https://oss-fuzz.com/
- https://introspector.oss-fuzz.com/project-profile?project=nss
- https://fuzzmanager.fuzzing.mozilla.org/covmanager/reports/
- https://github.com/MozillaSecurity/orion
- https://treeherder.mozilla.org/jobs?repo=nss-try
