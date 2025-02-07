=============
Update Verify
=============
Update verify is test that runs before each Firefox Release (excluding Nightlies) are shipped. Its main purpose is to ensure that users who receive the release through an update MAR (`Mozilla ARchive <https://wiki.mozilla.org/Software_Update:MAR>`__) end up in the same place as a fresh install would get them. This helps us to ensure that `partial MARs <https://firefox-source-docs.mozilla.org/taskcluster/partials.html>`__ work in future updates, and that code signatures are valid regardless of how a user arrived at a new version.

Note that the object under test here is the update MAR files that are prepared as part of a Firefox release -- and *nothing* else. Although certain other parts of the update system are currently used as part of these tests, the following things are specifically *not* considered under test:

* The state of update rules in `Balrog <https://mozilla-balrog.readthedocs.io/en/latest/index.html>`__
* The `updater` binary itself
* The :ref:`Application Update <Application Update>` component

Both Balrog and the Linux updater binary are currently used as part of running update verify tests, but there are no guarantees they will continue to be in the future.

------------
How it works
------------
At a high level, update verify simulates what happens when an update MAR is applied on a user machine. The result of that is compared against the full installer for the same version. If there are any differences (aside from a `small set of expected and known OK ones <https://searchfox.org/mozilla-central/source/tools/update-verify/release/compare-directories.py#25-99>`__), the test fails.

This test is run for all older builds (back to the `last watershed update <https://searchfox.org/mozilla-central/source/taskcluster/kinds/release-update-verify-config/kind.yml#53-59>`__ on all platforms. This means that we apply the same MARs over and over again, to different older versions of Firefox.

All tests are performed on Linux, with the `updater` from the older version's Linux package. For example, when testing 128.0 -> 129.0 mac updates, we will apply a 129.0 MAR to a 128.0 mac build with the 128.0 Linux updater binary.

With these details in mind, this is what the process looks like for testing a 128.0 -> 129.0 macOS en-US complete MAR:

* Download the 128.0 en-US Linux tarball and unpack it
* Download the 128.0 en-US DMG and unpack it into the `source` directory
* Download the 129.0 en-US DMG and unpack it into the `target` directory
* Download the 129.0 en-US macOS complete MAR
* Run the `updater` binary from the unpacked Linux build to apply it to the `source` directory
* Diff the `source` and `target` directories
* Compare the result against `expected differences <https://searchfox.org/mozilla-central/source/tools/update-verify/release/compare-directories.py#25-99>`__ to determine pass or fail
