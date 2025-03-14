.. _fenix-contributor-guide:

Building Firefox for Android
============================

As a first step, you need to set up your development environment using the instruction :ref:`here <firefox_for_android>`.

Before building, set the paths to your Java installation and Android SDK

**macOS**

.. code-block:: shell

    export JAVA_HOME=$HOME/.mozbuild/jdk/jdk-<latest-version>/Contents/Home
    export ANDROID_HOME=$HOME/.mozbuild/android-sdk-<os_name>

**non-macOS**

.. code-block:: shell

    export JAVA_HOME=$HOME/.mozbuild/jdk/jdk-<latest-version>
    export ANDROID_HOME=$HOME/.mozbuild/android-sdk-<os_name>


Build Fenix using command line
------------------------------

From the root mozilla-central directory, build Fenix:

.. code-block:: shell

    ./mach gradle clean fenix:assembleDebug

You can then find the generated debug apks in objdir under
``gradle/build/mobile/android/fenix/outputs/apk/fenix/debug``

To sign your release builds with your debug key automatically, add the following to `<proj-root>/local.properties`:

.. code-block:: shell

    autosignReleaseWithDebugKey


Run Fenix or other Android projects using command line
---------------------------------------------------------
.. _run_fenix_from_commandline:

You can run the following command to launch an emulator and install and run Fenix:

.. code-block:: shell

    ./mach run --app=fenix

Run Fenix tests
-------------------

You can run tests via all the normal routes from within Android Studio:

- For individual test files, click the little green play button at the top
- For a module/component:

   - Right click in project explorer → run all tests
   - Select from gradle tasks window

If you see the error "Test events were not received", check your top level folder - this happens if you try and run tests in Android Components from ``mozilla-unified/mobile/android/fenix/``.
To build tests for Android Components you need to be using the ``build.gradle`` in ``mozilla-unified/mobile/android/android-components/``.

Alternatively, you can run tests from command line using:

    - ``./mach test <file-name>`` to run all tests in the file
    - ``./mach test <directory-name>`` to run all tests in the directory
    - ``./mach test {fenix,focus,ac,android-components,geckoview}`` to run all tests in the specific project

If after running tests on your Android device, you can no longer long press, this is because the connected Android tests mess around with your phone’s accessibility settings.
They set the long press delay to 3 seconds, which is an uncomfortably long time.
To fix this, go to Settings → Accessibility → Touch and hold delay, and reset this to default or short (depends on manufacturer).

Lint
-------------------

You can run the following commands to verify that your code is formatted correctly:

    - ``./mach lint -l android-fenix`` to lint changes made in fenix directory
    - ``./mach lint -l android-focus`` to lint changes made in focus directory
    - ``./mach lint -l android-ac`` to lint changes made in android-components directory

You can find more linters by running ``./mach lint --list``
You can pass an extra argument ``--fix`` to autofix certain types of reported issues.

Preset Try
-------------------

It is advisable to run your tests before submitting your patch. You can do this using Mozilla’s ``try`` server.
The following commands will ensure that all the required tests are run based on the changes made:

    - ``./mach try --preset fenix`` - will run Fenix test suites
    - ``./mach try --preset firefox-android`` - will run AC and Fenix test suites
    - ``./mach try --preset android-geckoview`` - will run GeckoView test suites

Failures on ``try`` will show up with the test name highlighted in orange. Select the test to find out more.
Intermittent failures occasionally occur due to issues with the test harness. Retriggering the test is a good way to confirm it is an intermittent failure and not due to the patch.
Usually there will also be a bug number with a portion of the stack trace as well for documented intermittent failures.

Speed Up the CI
-------------------

Currently, the CI builds GeckoView even if your commit doesn't impact it.

If you know your changes don't impact GeckoView, you can try using the following option: ``--use-existing-tasks`` or ``-E``. For example:

``./mach try --preset firefox-android -E``

This will try to reuse a GeckoView build from a previous CI job, and thus reduce the CI time.

Other Links:
-------------------

.. toctree::
   :maxdepth: 1

   Understanding Artifact Builds <https://firefox-source-docs.mozilla.org/contributing/build/artifact_builds.html>
   Pushing to Try <https://firefox-source-docs.mozilla.org/tools/try/index.html>
   Submitting a Patch <https://firefox-source-docs.mozilla.org/contributing/how_to_submit_a_patch.html>
   Landing a Patch <https://moz-conduit.readthedocs.io/en/latest/lando-user.html>
