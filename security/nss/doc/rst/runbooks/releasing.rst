.. _mozilla_projects_nss_runbooks_releasing:

Releasing NSS
=============

.. container::

   Stages of the NSS Release Cycle:

   * Normal development. This runs from the day after a Firefox merge until 2 weeks before the next Firefox merge. During this time, the version of NSS in mozilla-central and on NSS's development branch are kept in sync by `Updatebot <https://github.com/mozilla-services/updatebot>`_.
   * Freezing for release. This starts 2 weeks before the next Firefox merge. During this time, mozilla-central tracks a release branch. Commits can still land on NSS's development branch but they won't be uplifted to mozilla-central.

Freezing a version for release
------------------------------

In the week prior to a NSS release, the version in mozilla-unified
will be frozen. This is to ensure that new NSS versions have
adequate testing in Firefox Nightly before making their way to Beta
and Release.

The NSS Release owner will:

1. Make sure your local repo is up to date with ``hg pull`` and ``hg checkout default``.
2. Make a branch for this NSS release. ``hg branch NSS_3_XXX_BRANCH``
3. Tag a beta for this NSS release. ``hg tag NSS_3_XXX_BETA1``
4. Inspect the outgoing changes with ``hg outgoing`` and verify they are correct.
5. Push this branch and tag to the NSS repository. ``hg push --new-branch``
6. Wait for the changes to sync to Github (~15 minutes).
7. Manually uplift this version into mozilla-unified by running ``./mach vendor security/nss/moz.yaml -r NSS_3_XXX_BETA1`` in mozilla-unified.

.. warning::

   It may be that issues are uncovered by users running Firefox Nightly.
   If so, the appropriate changes should be made to this branch and to the development branch, then a new beta tagged and uplifted.

.. warning::

   After this point, further submissions by Updatebot SHOULD be ignored to ensure that the frozen branch is not overwritten by
   further changes to the development branch.

Releasing NSS into Firefox
--------------------------

The NSS Release Owner will:

1. Make sure you're on the appropriate branch (``hg checkout NSS_3_XXX_BRANCH``).
2. Update the NSS version numbers: ``python3 automation/release/nss-release-helper.py remove_beta``
3. Commit the change: ``hg commit -m "Set version numbers to 3.XXX final``
4. Add a release note named ``nss_3_XXX.rst`` to ``doc/rst/releases`` and update ``index.rst`` in the release branch.
5. Commit the release notes: ``hg commit -m "Release notes for NSS 3.XXX"``.
6. Tag the release version: ``hg tag NSS_3_XXX_RTM``
7. Push the changes to the NSS repository. ``hg push``
8. Switch the default branch and graft the release notes onto this branch: ``hg graft -r {DOCS_COMMIT}``.
9. Manually uplift this version by running ``./mach vendor security/nss/moz.yaml -r NSS_3_XXX_RTM`` in mozilla-unified.

.. warning::

   ./mach vendor does not currently update the root CA telemetry. This must be done manually.


Releasing NSS to downstream
---------------------------

You will need the ``gcloud`` tool installed from https://cloud.google.com/sdk/docs/install.

1. Create the release archives with ``python automation/release/nss-release-helper.py create_nss_release_archive 3.XXX NSS_3_XXX_RTM ../stage``
2. Announce the release on `dev-tech-crypto <https://groups.google.com/a/mozilla.org/g/dev-tech-crypto>`_.

Preparing for the next release
------------------------------

 1. File a new bug blocking the `nss-uplift bug <https://bugzilla.mozilla.org/show_bug.cgi?id=nss-uplift>`_ by cloning the current release bug.
 2. Assign the next release owner in the rotation.
 3. Update the `NSS Release Calendar <https://calendar.google.com/calendar/embed?src=mozilla.com_2gnk73saaledse6q8n93b1m2u4%40group.calendar.google.com&ctz=Europe%2FLondon>`_ using the dates from https://whattrainisitnow.com/
 4. Update the release tracker in the team meeting notes (internal only).
 5. Update NSS to the next version: ``python3 automation/release/nss-release-helper.py set_version_to_minor_release 3 XXX+1``.
 6. ``hg commit -m "Set version numbers to 3.{XXX+1} Beta"`` and push this commit.
 7. Approve any waiting commits from Updatebot.

Please now copy the checklist below and fill it out in the NSS release bug and close it:

::

    [ ] - NSS XXX has been released into mozilla-central for Firefox XXX.
    [ ] - NSS release binaries can be found at https://ftp.mozilla.org/pub/nss/releases/
    [ ] - The release has been announced on dev-tech-crypto.
    [ ] - The nss version has been updated on the default branch
    [ ] - The next release bug has been filed.
    [ ] - The release calendar has been updated.

Updating NSPR
-------------

NSPR releases are infrequent, but require changing the NSPR version is listed in ``automation/release/nspr-version.txt``