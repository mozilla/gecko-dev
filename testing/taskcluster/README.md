# Taskcluster + Gecko Integration

## Directory structure:

  - tasks/  : All task definitions

  - tests/  : Tests for the mach target internals related to task graph
              generation

  - scripts : Various scripts used by taskcluster docker images and
              utilities these exist in tree primarily to avoid rebuilding
              docker images.

## Task conventions

In order to properly enable task reuse there are a small number of
conventions and parameters that are specialized for build tasks vs test
tasks. The goal here should be to provide as much of the power as
taskcluster but not at the cost of making it easy to support the current
model of build/test.


All tasks are in the YAML format and are also processed via mustache to
allow for greater customizations. All tasks have the following
templates variables:


  - `docker_image`: Helper for always using the latest version of a docker
    image that exist in tree.

    ```
    {{#docker_image}}base{{/docker_image}}
    ```

    Will produce something like (see the docker folder):

    ```
    quay.io/mozilla.com/base:0.11
    ```

  - `from_now`: Helper for crafting a JSON date in the future.

    ```
    {{#from_now}}1 year{{/from_now}}
    ```

    Will produce:

    ```
    2014-10-19T22:45:45.655Z
    ```

  - `now`: Current time as a json formatted date.


### Build tasks

By convention build tasks are stored in `tasks/builds/` the location of
each particular type of build is specified in `job_flags.yml` (and more
locations in the future)

#### Task format

To facilitate better reuse of tasks there are some expectations of the
build tasks. These are required for the test tasks to interact with the
builds correctly but may not effect the builds or indexing services.

```yaml

# This is an example of just the special fields. Other fields that are
# required by taskcluster are omitted and documented on http://docs.taskcluster.net/
task:

  payload:
    # Builders usually create at least two important artifacts the build
    # and the tests these can be anywhere in the task and also may have
    # different path names to include things like arch and extension
    artifacts:
      # The build this can be anything as long as its referenced in
      # locations.
      'public/name_i_made_up.tar.gz': '/path/to/build'
      'public/some_tests.zip': '/path/to/tests'

  extra:
    # Build tasks may put their artifacts anywhere but there are common
    # resources that test tasks need to do their job correctly so we
    # need to provide an easy way to lookup the correct aritfact path.
    locations:
      build: 'public/name_i_made_up.tar.gz'
      tests: 'public/some_tests.zip' or test_packages: 'public/test_packages.json'
```

#### Templates properties

  - repository: Target HG repository (ex:
    https://hg.mozilla.org/mozilla-central)

  - revision: Target HG revision for gecko

  - owner: Email address of the committer

### Test Tasks

By convention test tasks are stored in `tasks/tests/` the location of
each particular type of build is specified in `job_flags.yml` (and more
locations in the future)


#### Template properties

  - repository: Target HG repository (ex:
    https://hg.mozilla.org/mozilla-central)

  - revision: Target HG revision for gecko

  - owner: Email address of the committer

  - build_url: Location of the build

  - tests_url: Location of the tests.zip package

  - chunk: Current chunk

  - total_chunks: Total number of chunks

## Developing

Running commands via mach is the best way to invoke commands testing
works a little differently (I have not figured out how to invoke
python-test without running install steps first)


```sh
mach python-test tests/
```

## Examples:

Requires [taskcluster-cli](https://github.com/taskcluster/taskcluster-cli).

```sh
mach taskcluster-trygraph --message 'try: -b do -p all' \
 --head-rev=33c0181c4a25 \
 --head-repository=http://hg.mozilla.org/mozilla-central \
 --owner=jlal@mozilla.com | taskcluster run-graph
```

Creating only a build task and submitting to taskcluster:

```sh
mach taskcluster-build \
  --head-revision=33c0181c4a25 \
  --head-repository=http://hg.mozilla.org/mozilla-central \
  --owner=user@domain.com tasks/builds/b2g_desktop.yml | taskcluster run-task --verbose
```

```sh
mach taskcluster-tests --task-id=Mcnvz7wUR_SEMhmWb7cGdQ  \
  --owner=user@domain.com tasks/tests/b2g_mochitest.yml | taskcluster run-task --verbose
```
