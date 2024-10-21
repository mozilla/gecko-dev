Written April 2018

In-App Update Process
=====================

This document details how Firefox checks for and installs updates. The
focus is how it does this in the Windows Operating System. This was
written in preparation for creating a Background Update Agent, and
therefore does not cover that update mechanism.

Definitions
-----------

MAR file
''''''''

An archive format containing update data. They are signed to prevent
being tampered with.

Partial MAR file
''''''''''''''''

A MAR file containing a binary diff between the installed and updated
versions of Firefox. This is the preferred update mechanism because the
file sizes are much smaller.

Complete MAR file
'''''''''''''''''

A MAR file containing a full copy of the application. This is typically
used if updating from a partial MAR has failed or if the user is
updating from such an old version that no partial MAR exists to upgrade
directly from that version to the current version.

Update directory
''''''''''''''''

Directory where update files are saved. Location is platform-specific,
but can be determined with this code:

.. code::

   Services.dirsvc.get("UpdRootD", Ci.nsIFile);

Update status file
''''''''''''''''''

A file written to and read from, in part, to communicate between Firefox
and the updater. The file will contain a short string indicating the
current status. If that status is "failed", it will also contain an
error code. A list of recognized status strings can be found
`here <https://searchfox.org/mozilla-central/rev/7ccb618f45a1398e31a086a009f87c8fd3a790b6/toolkit/mozapps/update/nsIUpdateService.idl#177-190>`__.
The error codes used for failure status can be found
`here <https://searchfox.org/mozilla-central/rev/3265b390bd5d08a5be520253ef71835bcb715f27/toolkit/mozapps/update/common/updatererrors.h>`__.

Documentation
-------------

- `MAR files <https://wiki.mozilla.org/Software_Update:MAR>`__
- `Update
  pings <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/update-ping.html>`__

Detailed Update Steps
---------------------

1.  An update check is initiated shortly after Firefox launches. First,
    an update XML document is retrieved by an internal XHR request. The
    XML is saved in the `update directory <#update-directory>`__. Some
    very basic sanity checks are performed to ensure that this file is
    valid and describes an update that should be applied.

    1. The URL used to download the update XML requires some variable
       substitution and communicates to the server information about the
       system and the currently installed version of Firefox:
       ``https://aus5.mozilla.org/update/6/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%(noBug1296630v1)(nowebsense)/%SYSTEM_CAPABILITIES%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml``
       On my system, this becomes:
       ``https://aus5.mozilla.org/update/6/Firefox/61.0a1/20180419100148/WINNT_x86_64-msvc-x64/en-US/nightly/Windows_NT 10.0.0.0.16299.371 (x64)(noBug1296630v1)(nowebsense)/ISET:SSE4_2,MEM:32676/default/default/update.xml``
    2. Balrog uses this information to determine what XML data to send
       in response. Common criteria used in making this decision
       include:

       1. Is the system capable of using a 64-bit MAR? Does it have
          enough RAM to make it "worth it"?
       2. Does the system support processor features that are required
          for newer versions of Firefox?
       3. Are updates currently being throttled? If so, Balrog may
          indicate to some clients that there is not an update yet. Note
          that manual checks for update will add a "force" query
          parameter which will force Balrog to report any updates,
          regardless of throttling.

    3. Balrog will send an XML document in response. If an update is
       available, it will contain the URL for a `complete
       MAR <#complete-mar-file>`__. If a `partial
       MAR <#partial-mar-file>`__ is available, the URL for that will be
       contained as well.

2.  If an update is available, Firefox downloads the update MAR file.

    1. Currently, downloads are performed using
       `nsIIncrementalDownloader`, but ideally we would use a less-special
       restartable downloader (`Bug
       1348087 <https://bugzilla.mozilla.org/show_bug.cgi?id=1348087>`__)
    2. Files are stored in the "update directory" (see Definitions,
       above)
    3. If Firefox is closed before download completes, the download
       resumes the next time Firefox launches.
    4. During this time, the update status file will contain
       "downloading"

3.  When the download completes, the MAR file stays in the `update
    directory <#update-directory>`__. The signature/file integrity is
    not yet verified.

    1. The action taken now differs depending on whether update staging
       is enabled (controlled by the pref:
       ``app.update.staging.enabled``). Staging is currently enabled by
       default in all cases.
    2. If staging is enabled:

       1. Firefox runs the updater, which copies the install files into a
          directory within the installation and updates them
       2. If privilege elevation will be needed and the Mozilla
          Maintenance Service is available, the update status file will
          now contain "applied-service"
       3. Otherwise, the update status file will now contain "applied"

    3. If staging is disabled:

       1. If privilege elevation will not be needed to install the
          update, the update status file will now contain "pending"
       2. If privilege elevation will be needed and the Mozilla
          Maintenance Service is available, the update status file will
          now contain "pending-service"
       3. If privilege elevation will be needed and the Mozilla
          Maintenance Service is not available, the update status file
          will now contain either "pending" (on Windows) or "pending-elevate" (on Mac).

4.  Update ping sent with reason="ready".
5.  Firefox waits to be closed so that the update can be installed.

    1. If the user keeps the browser open for more than 4 days, a green
       arrow will be displayed on the menu icon, prompting the user to
       restart to update.
    2. If the user keeps the browser open for 8 days, a doorhanger will
       prompt for a restart.
    3. (Note: These are the delays for the release channel, the prefs
       that control this have different values on other channels. On
       Nightly, for example, the green arrow has no delay and the
       doorhanger delay is 12 hours)

6.  When Firefox next launches, it reads the update status file very
    early in startup. Upon discovering one of the "pending" statuses, it
    launches the updater and exits. This updater process will be running
    without privilege elevation.
7.  The updater reads the update status file and, if the status was a
    "pending" status, it changes the status to "applying".
8.  If the status read was "pending-service" or "applied-service", the
    updater contacts the Mozilla Maintenance Service and requests that a
    privileged updater be started.

    1. This process is retried in the event of failure, but the updater
       will continue if multiple attempts all result in failure
    2. The original updater waits for the privileged updater process to
       exit

9.  If the status read was "pending-elevate" or "applied", or if the
    updater was unable to get a privileged updater from the Mozilla
    Maintenance Service, a new process is started with elevated
    privileges. This causes a UAC prompt to display. The original
    updater waits for the privileged updater to exit.

    1. If the UAC prompt was declined, the updater writes an error state and exits.
    2. When Firefox starts, it returns the status to a "pending" state, and then it returns to step 5
    3. If this process repeats 2 times, we prompt the user to download
       the Firefox installer in order to update (but Firefox returns to
       step 5 nevertheless)

10. The update is now attempted. If the status was one of the "pending"
    statuses, this will involve patching or overwriting installation
    files with data from the MAR file. If the status was one of the
    "applied" statuses, this will involve overwriting files in the
    installation directory with the patched (staged) files in the update
    directory.

    1. If a privileged updater was not needed, the original updater
       process attempts to install the update.
    2. If a privileged updater was started, it attempts to install the
       update. Afterwards it exits and the original, unprivileged
       updater process resumes

11. The updater writes the status to the update status file (either
    "succeeded" or "failed"), starts Firefox, and exits.
12. Firefox starts and reads the update status file.

    1. On update success:

       1. An update ping (with reason="success") is sent
       2. Firefox reads the XML file out of the `update
          directory <#update-directory>`__ and, if it indicates that a
          particular URL should be shown after that update, it does so.

    2. On update failure, Firefox checks whether this was a `partial
       MAR <#partial-mar-file>`__ or a `complete
       MAR <#complete-mar-file>`__ by checking the "selected" attribute
       in the XML.

       1. If it was a partial MAR, Firefox downloads the complete MAR
          and retries the update (i.e. goes back to step 2, but
          downloads a different MAR)
       2. If it was a complete MAR, Firefox notifies the user of the
          failure and gives them a link to the current version.

Operating System Related Notes
------------------------------

The purpose of this document is to describe the mechanism for update
under the Window Operating System. These are some additional notes to
describe how this process differs in other operating systems. This
section is currently known to be incomplete.

- The Mozilla Maintenance Service only exists in Windows.
- On Mac, Firefox can launch an elevated updater process directly.
- On macOS, we write pending-elevate when we need elevation. At startup
  with this status, we do not actually start the updater, we instead ask
  the user if it's okay to update and, if they agree, switch to the pending
  status. If the user is an administrator, they shouldn't need to elevate more
  than once since the process fixes the permissions on the installation directory
  so that Firefox can be updated with administrator privileges.
