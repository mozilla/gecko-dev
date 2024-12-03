=======================
Session Store Utilities
=======================

-----------------
SessionStoreUtils
-----------------

`SessionStoreUtils.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/SessionStoreUtils.webidl>`__ and its implementation `SessionStoreUtils.cpp <https://searchfox.org/mozilla-central/source/toolkit/components/sessionstore/SessionStoreUtils.cpp>`__ contain several of the key functions and types for implementing session restore. In particular the functions that extracts or restores states from/to a document.

-------------
nsFrameLoader
-------------

`nsFrameLoader.cpp <https://searchfox.org/mozilla-central/source/dom/base/nsFrameLoader.webidl>`__ and `nsFrameLoader.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/FrameLoader.webidl>`__ defines important functionality for session store data collection that enables state flushing. Flushing is required because of the buffering controlled by ``browser.sessionstore.interval``, since there are occasions where waiting for the collection to buffer isn't desired, e.g closing a tab, duplicating a tab, etc. ``Promise<undefined> FrameLoader.requestTabStateFlush()`` initiates a flush, which will perform the steps described in :ref:`collecting-data`, but without buffering. Once the collection is finished, the promise resolves. ``requestTabStateFlush`` can be called either from JavaScript or C++. ``void nsFrameLoader::RequestFinalTabStateFlush()`` on the other hand is called by the platform when a tab is closing, and tries to ensure that whatever data has been collected is sent to the session store. When this has finished, the observer service will notify listeners of the ``"browser-shutdown-tabstate-updated"`` topic.

The main difference between ``RequestTabStateFlush`` and ``RequestFinalTabStateFlush`` is that the latter is very much a best effort functionality, and data loss cannot be guaranteed to be avoided. On the other hand, using ``RequestTabStateFlush`` and awaiting the promise might take an relatively long time, which might make that not suitable in all situations.

-----------
Preferences
-----------

There's a handful of preferences that can be used to tweak or toggle session restore functionality:

* ``browser.sessionstore.disable_platform_collection``
    This disables platform data collection entirely, and enable Fission incompatible data collection.
* ``browser.sessionstore.dom_storage_limit``
    Maximum number of bytes of DOMSessionStorage data collected per origin.
* ``browser.sessionstore.dom_form_limit``
    Maximum number of characters of form field data collected per field.
* ``browser.sessionstore.dom_form_max_limit``
    Maximum number of characters of form data collected per origin.
* ``browser.sessionstore.interval``
    Minimal interval between two save operations in milliseconds (while the user is active).
* ``browser.sessionstore.debug.no_auto_updates``
    Essentially setting the collection interval to infinity, causing session store updates to never occur automatically.

The two most important preferences are ``browser.sessionstore.interval`` and ``browser.sessionstore.debug.no_auto_updates`` since they're useful to write session restore tests that are deterministic (non-intermittent), by limiting the interval or entirely turning off session data collection. This is also an instance where ``FrameLoader.requestTabStateFlush`` is useful.
