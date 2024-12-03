==============
Restoring Data
==============

Restoring consists of two steps:

1) Restore session history, which is handled in the `Session History module <https://searchfox.org/mozilla-central/source/toolkit/modules/sessionstore/SessionHistory.sys.mjs>`__.
2) Restore the other collected data, which is handled in `SessionStoreUtils.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/SessionStoreUtils.webidl>`__ and `SessionStoreUtils.cpp <https://searchfox.org/mozilla-central/source/toolkit/components/sessionstore/SessionStoreUtils.cpp>`__

The reason for this is that restoring the session history implies navigating to the location of the restored active session history entry, and it's only possible to restore the state of the page after that navigation is complete.

Doing this is accomplished by calling ``SessionHistory.restoreFromParent`` followed by ``SessionStoreUtils.initializeRestore``. ``restoreFromParent`` and ``initializeRestore`` aren't tightly coupled, and will work in separate situations. ``restoreFromParent`` will do just what it says, restore the session history initiated from the parent process and ``initializeRestore`` will load the tab and restore the saved state of a document tree separately. These steps can in turn be made arbitrarily complicated, depending on a lot of different parameters:

* Restoring several windows
* Restoring several tabs
* Restoring the currently selected tab
* ... and so on

but in the end it's just a matter of restoring the session history, reloading the tab and restoring document tree state.

The following (very) simplified sequence diagram illustrates the abstract control flow:

.. mermaid::

    ---
    title: Session Store Restore Steps
    ---
    sequenceDiagram
        box Tab
        participant Parent
        participant Content
        end

        Parent->>Parent: SessionHistory.restoreFromParent
        Parent->>Parent: SessionStoreUtils.initializeRestore

        loop RestoreTabContentObserver::Observe
        Content->>Parent: PWindowGlobal.requestRestoreTabContent
        Parent->>Content: PWindowGlobal.restoreTabContent
        end

Restoring is a load order recursive procedure, and the validity of a restore is checked for every document that has restore data. If a document doesn't match its stored session history entry, restore is skipped for that document.

--------------------------------------------
Calling ``SessionHistory.restoreFromParent``
--------------------------------------------

``SessionHistory.restoreFromParent`` expects the following arguments:

.. code-block:: webidl

  partial interface SessionHistory {
    undefined restoreFromParent(history, tabData);
  };

* ``history`` is the session history object
* ``tabData`` is the tabdata including all history entries.

The format of ``history`` isn't relevant, but it is the idl interface `nsISHistory <https://searchfox.org/mozilla-central/source/docshell/shistory/nsISHistory.idl>`__.

The format of ``tabData`` is an object literal with the corresponding interface:

.. code-block:: webidl

  interface {
    sequence<nsISHEntry> entries;
    long requestedIndex;
    long index;
    long fromIdx;
  }

All this data is available when the embedder receives a call from ``nsISessionStoreFunctions.UpdateSessionStore`` as described in :ref:`interacting-with-sessionstore`.

-----------------------------------------------
Calling ``SessionStoreUtils.initializeRestore``
-----------------------------------------------

``SessionStoreUtils.initializeRestore`` expects the following arguments:

.. code-block:: webidl

  partial interface SessionStoreUtils {
    Promise<undefined> initializeRestore(CanonicalBrowsingContext browsingContext,
                                         nsISessionStoreRestoreData? data);
  };

* ``browsingContext`` is the top browsing context for the tree to restore
* ``data`` is the collected data.

The ``data`` argument is an object described by the idl interface:

.. code-block:: webidl

  interface nsISessionStoreRestoreData : nsISupports {
    // Setters for form data.
    attribute AUTF8String url;
    attribute AString innerHTML;

    // Setters for scroll data.
    attribute ACString scroll;

    // Methods for adding individual form fields which are called as the JS code
    // finds them.
    void addTextField(in boolean aIsXPath, in AString aIdOrXPath,
                      in AString aValue);
    void addCheckbox(in boolean aIsXPath, in AString aIdOrXPath,
                     in boolean aValue);
    void addFileList(in boolean aIsXPath, in AString aIdOrXPath, in AString aType,
                     in Array aFileList);
    void addSingleSelect(in boolean aIsXPath, in AString aIdOrXPath,
                         in unsigned long aSelectedIndex, in AString aValue);
    void addMultipleSelect(in boolean aIsXPath, in AString aIdOrXPath,
                           in Array aValues);
    void addCustomElement(in boolean aIsXPath, in AString aIdOrXPath,
                          in jsval aValue, in jsval aState);
    // Add a child data object to our children list.
    void addChild(in nsISessionStoreRestoreData aChild, in unsigned long aIndex);
  };

All this data is available when the embedder receives a call from ``nsISessionStoreFunctions.UpdateSessionStore`` as described in :ref:`interacting-with-sessionstore`.
