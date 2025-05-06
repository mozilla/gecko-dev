.. _crashes_crashmanager:

=============
Crash Manager
=============

The **Crash Manager** is a service and interface for managing crash
data within the Gecko application.

From JavaScript, the service can be accessed via::

   let crashManager = Services.crashmanager;

That will give you an instance of ``CrashManager`` from ``CrashManager.sys.mjs``.
From there, you can access and manipulate crash data.

The crash manager stores statistical information about crashes as well as
detailed information for both browser and content crashes. The crash manager
automatically detects new browser crashes at startup by scanning for
:ref:`Crash Events`. Content process crash information on the other hand is
provided externally.

Crash Pings
===========

The Crash Manager is responsible for sending crash pings when a crash occurs
or when a crash event is found. Crash pings are sent using
`Telemetry pings <../../telemetry/data/crash-ping.html>`__.

Glean
-----
Crash pings have been migrated to use `Glean pings <../../glean/index.html>`__, however
the Telemetry pings will still be sent until we are confident that the Glean
implementation is robust. The Glean `crash` ping can be found
`here <https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/crash>`__.

See `bug 1784069 <https://bugzilla.mozilla.org/show_bug.cgi?id=1784069>`_ for details.


Other Documents
===============

.. toctree::
   :maxdepth: 1

   crash-events
