.. _collecting-data:

###############
Collecting Data
###############

When collecting data to be stored across sessions, two special cases need to be considered:

* Data collected in the parent process
* Data collected across the content processes

The reasons for these special cases mainly arise from where data is held. Session History and Session Storage are located in the parent process, and e.g form data or scroll position are found in the document in the content process where that document was created.

==========================
Handling Document Changes
==========================

-----------------
Detecting Changes
-----------------

Due to cross-process site isolation every tab is potentially split across a set of processes, with a document tree fragment in each process. The following diagram illustrates how changes in documents across different sites are detected and processed within Gecko's session store. It shows the hierarchical relationship between the content processes and the parent process, detailing the flow of information from detecting changes in individual documents to handling these changes at the session store level.

.. _Session Store Change Detection diagram:

.. Mermaid is a bit broken in Firefox, see https://github.com/mermaid-js/mermaid/issues/5785. This is the reason for all flowcharts having wrappingWidth set to 400.
.. mermaid::

  ---
  title: Session Store Change Detection
  config:
    flowchart:
      wrappingWidth: 400
      curve: linear
  ---
  flowchart BT
    classDef crossorigin stroke-dasharray:5 5;
    subgraph Gecko
      direction TB
      subgraph P[Parent process]
        direction BT
        SSP[SessionStoreParent] --> BSS[BrowserSessionStore]
        SH[SessionHistory] --> SSP
        SS[SessionStorage] --> SSP
      end

      subgraph C1[Content process]
        direction TB
        subgraph D1[Document tree]
            direction TB
            a1[a.com] --> b1[b.com] & c1[c.com]
            b1[b.com] --> a11[a.com] & b12[b.com]
        end

        SSC1[SessionStoreChild] <-- "fa:fa-clock-o" --> SSCL1[SessionStoreChangeListener]
        SSCL1 <--> D1
      end

      subgraph C2[Content process]
        direction TB
        subgraph D2[Document tree]
            direction TB
            a2[a.com] --> b2[b.com] & c2[c.com]
            b2[b.com] --> a22[a.com] & b22[b.com]
        end

        SSC2[SessionStoreChild] <-- "fa:fa-clock-o" -->  SSCL2[SessionStoreChangeListener]
        SSCL2 <--> D2
      end

      subgraph C3[Content process]
        direction TB
        subgraph D3[Document tree]
            direction TB
            a3[a.com] --> b3[b.com] & c3[c.com]
            b3[b.com] --> a32[a.com] & b32[b.com]
        end

        SSC3[SessionStoreChild] <-- "fa:fa-clock-o" -->  SSCL3[SessionStoreChangeListener]
        SSCL3 <--> D3
      end
      P <-- a.com ---> C1
      P <-- b.com ---> C2
      P <-- c.com ---> C3

      class a2,a22,a3,a32 crossorigin;
      class b1,b12,b3,b32 crossorigin;
      class c1,c2 crossorigin;
    end
    Gecko --> Embedder

Due to the potentially distributed nature of a site, storing session store data is performed in the parent process. Collecting data from a tree of documents to store across sessions is the responsibility of the participating content processes. Because of this the format of data collected in a content process is both partial and incremental in nature, and the parent process needs to handle them as such. The data is also a snapshot of the current state, and to not perform collection too often content processes need to buffer data by collecting periodically.

The steps for collecting data are roughly as follows:

#. Every document tree fragment has a listener that detects any changes to a document contained in that fragment.
#. Every change listener has a timer which is either set or not.
#. When a change is detected either a timer is set, or else one is scheduled.
#. A note is made in which document the change happened.
#. When the timer fires it becomes unset and every document marked as having changes is inspected, and the changed data is collected.
#. Then for each document, an increment is sent to update the session store in the parent with.

These document tree fragments are managed in Gecko by a `PBrowser <https://searchfox.org/mozilla-central/source/dom/ipc/PBrowser.ipdl>`__ IPDL actor, which in turn is managed by the `PContent <https://searchfox.org/mozilla-central/source/dom/ipc/PContent.ipdl>`__ IPDL actor. These actors facilitate communication between the content processes containing documents and the parent process running the UI.

To encapsulate the framework of session store data collection, the operations for incrementally transmitting data to put in the session store is handled by the `PSessionStore <https://searchfox.org/mozilla-central/source/toolkit/components/sessionstore/PSessionStore.ipdl>`__ IPDL actor, which is managed by the PBrowser IPDL actor.

The C++ class `SessionStoreChangeListener <https://searchfox.org/mozilla-central/source/toolkit/components/sessionstore/SessionStoreChangeListener.h>`__ registers event handlers that listen to changes in the document tree and manages the timer. The timer enables buffering changes to make sure that the cross-process communication is throttled as well as only collecting when a change has happened. The buffering period can be controlled by the pref ``browser.sessionstore.interval``. The class also keeps track of documents and collects data from them, as well as sending the collected data to the parent process using PSessionStore.

----------------------
Incremental Collection
----------------------

As noted in the previous section and from the :ref:`session store change detection diagram <Session Store Change Detection diagram>`, change will arrive in the parent process incrementally in fragments. Because of this the tree, mapping a set of changes to a tree of browsing contexts, needs to be built in that fashion; incrementally and in fragments.

In a situation like the following:

.. Mermaid is a bit broken in Firefox, see https://github.com/mermaid-js/mermaid/issues/5785. This is the reason for all flowcharts having wrappingWidth set to 400.
.. mermaid::

  ---
  title: Session Store Incremental Update
  config:
  flowchart:
    wrappingWidth: 400
    curve: linear
  ---
  flowchart BT
    subgraph Gecko
      direction BT
      subgraph P[Parent process]
        direction BT
        BSS[BrowserSessionStore]
        SH[SessionHistory] --> BSS
        SS[SessionStorage] --> BSS
      end

      subgraph C1[Content process]
        direction TB

        subgraph D1[Document tree]
            direction TB
            a11[a.com] --> b1[b.com] & a12[a.com]
            b1[b.com] --> a13[a.com] & c1[c.com]
        end
      end

      subgraph C2[Content process]
        direction TB

        subgraph D2[Document tree]
            direction TB
            a21[a.com] --> b2[b.com] & a22[a.com]
            b2[b.com] --> a23[a.com] & c2[c.com]
        end
      end

      subgraph C3[Content process]
        direction TB

        subgraph D3[Document tree]
            direction TB
            a31[a.com] --> b3[b.com] & a32[a.com]
            b3[b.com] --> a33[a.com] & c3[c.com]
        end
      end

      C1 & C2 & C3 --> P

      classDef crossorigin stroke-dasharray:5 5;
      class a21,a22,a23,a31,a32,a33 crossorigin;
      class b1,b3 crossorigin;
      class c1,c2 crossorigin;
    end

if a user would scroll c.com and then after some time write some text in b.com, the sequence of the change data structure that should be created would be:

.. Mermaid is a bit broken in Firefox, see https://github.com/mermaid-js/mermaid/issues/5785. This is the reason for all flowcharts having wrappingWidth set to 400.
.. mermaid::

  ---
  title: Incremental Update Data
  config:
    flowchart:
      wrappingWidth: 400
      curve: linear
  ---
  flowchart LR

    subgraph D0[No Data]
      direction TB
      a11["{}"] --> b1["{}"] & a12["{}"]
      b1 --> a13["{}"] & c1["{}"]
    end

    subgraph D1[Scroll Data]
      direction TB
      a21["{}"] --> b2["{}"] & a22["{}"]
      b2 --> a23["{}"] & c2["{scroll: 42}"]
    end

    subgraph D2[Form Data]
      direction TB
      a31["{}"] --> b3["{id: 'some text'}"] & a32["{}"]
      b3 --> a33["{}"] & c3["{scroll: 42}"]
    end

    D0 --> D1 --> D2

This would then be merged on top of the embedder's session store data, possibly adding or changing the current state, including removing nodes. This is achieved by the embedder implementing the ``nsISessionStoreFunctions.idl`` interface.

------------------------
Disabling site isolation
------------------------

In the case where site isolation is disabled the :ref:`session store change detection diagram <Session Store Change Detection diagram>` collapses to the following:

.. _Collapsed Session Store Change Detection diagram:

.. Mermaid is a bit broken in Firefox, see https://github.com/mermaid-js/mermaid/issues/5785. This is the reason for all flowcharts having wrappingWidth set to 400.
.. mermaid::

  ---
  title: Session Store Change Detection
  config:
    flowchart:
      wrappingWidth: 400
      curve: linear
  ---
  flowchart BT
    subgraph Gecko
      direction TB
      subgraph P[Parent process]
        direction BT
        SSP[SessionStoreParent] --> BSS[BrowserSessionStore]
        SH[SessionHistory] --> SSP
        SS[SessionStorage] --> SSP
      end

      subgraph C1[Content process]
        direction TB
        subgraph D1[Document tree]
            direction TB
            a1[a.com] --> b1[b.com] & c1[c.com]
            b1[b.com] --> a11[a.com] & b12[b.com]
        end

        SSC1[SessionStoreChild] <-- "fa:fa-clock-o" --> SSCL1[SessionStoreChangeListener]
        SSCL1 <--> D1
      end

      subgraph C2[Content process]
        direction TB
        subgraph D2[Document tree]
          direction TB
          a2[example.com]
        end

        SSC2[SessionStoreChild] <-- "fa:fa-clock-o" -->  SSCL2[SessionStoreChangeListener]
        SSCL2 <--> D2
      end

      subgraph C3[Content process]
        direction TB
        subgraph D3[Document tree]
          direction TB
          a3[example.org]
        end

        SSC3[SessionStoreChild] <-- "fa:fa-clock-o" -->  SSCL3[SessionStoreChangeListener]
        SSCL3 <--> D3
      end

      P <-- a.com ---> C1
      P <-- "<empty>" ---> C2
      P <-- "<empty>" ---> C3
    end
    Gecko --> Embedder

but nothing else actually changes in the way that session store data is collected.

=====================================
Collecting Data in the Parent Process
=====================================

--------------------------
Collecting Session Storage
--------------------------

Session Storage is accessed through the `BackgroundSessionStorageManager <https://searchfox.org/mozilla-central/source/dom/storage/SessionStorageManager.h>`__ and by calling ``BackgroundSessionStorageManager::GetData``.

--------------------------
Collecting Session History
--------------------------

Session history is collected by calling the ``SessionHistory.collectFromParent`` function in the `Session History module <https://searchfox.org/mozilla-central/source/toolkit/modules/sessionstore/SessionHistory.sys.mjs>`__. This is the responsibility of the embedder to collect and is not pushed to the embedder.

.. _interacting-with-sessionstore:

=========================================
Interacting With Session Store Collection
=========================================

To integrate with session store collection an embedder needs to implement the `nsISessionStoreFunctions <https://searchfox.org/mozilla-central/source/toolkit/components/sessionstore/nsISessionStoreFunctions.idl>`__ interface.

.. code-block:: webidl

  interface nsISessionStoreFunctions : nsISupports {
    void UpdateSessionStore(
      in Element aBrowser, in BrowsingContext aBrowsingContext,
      in jsval aPermanentKey, in uint32_t aEpoch, in boolean aCollectSHistory,
      in jsval aData);

    void UpdateSessionStoreForStorage(
      in Element aBrowser, in BrowsingContext aBrowsingContext,
      in jsval aPermanentKey, in uint32_t aEpoch, in jsval aData);
  };

Collected changes will be sent to the embedder through one of two function calls, depending on the type of collected data.

``UpdateSessionStore`` is called for data collected in the content processes with the following arguments:

* ``in Element aBrowser`` is deprecated and always ``null``
* ``in BrowsingContext aBrowsingContext`` is the root browsing context of the sub-tree where data was collected.
* ``in jsval aPermanentKey`` is the current browser's permanent key. It's completely opaque, but unique for the browser.
* ``in uint32_t aEpoch`` is the current epoch of the session store. Setting the epoch is done via ``nsIFrameLoader.requestEpochUpdate``. After requesting a new epoch, the following calls to ``UpdateSessionStore`` will have that epoch. This can, for example, be used to filter out unwanted updates by requesting a new epoch, and after that ignore all calls to ``UpdateSessionStore`` that has a different epoch.
* ``in boolean aCollectSHistory`` if collecting all of session history is needed.
* ``in jsval aData`` is the data collected.

``UpdateSessionStoreForStorage`` differs only in that it doesn't get called with ``aCollectSHistory``, and in how the data in ``aData`` is structured. This function will be called when session storage has been collected.

Exactly how these arguments are to be used is very much up to the embedder to decide.

-------------------------------
Structure of the collected data
-------------------------------

Session store data comes in three flavors:

* Data collected from the document
* Data collected from session storage
* Data collected from session history

The actual format of the data stored is not relevant insofar that its use is basically just to be complete enough to be able to restore the state of a session.

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Structure of data pushed by ``UpdateSessionStore``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In `SessionStoreUtils.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/SessionStoreUtils.webidl>`__ the structure of collected data is defined as:

.. code-block:: webidl

  dictionary CollectedData
  {
    ByteString scroll;
    record<DOMString, CollectedFormDataValue> id;
    record<DOMString, CollectedFormDataValue> xpath;
    DOMString innerHTML;
    ByteString url;
    // children contains CollectedData instances
    sequence<object?> children;
  };

  // object contains either a CollectedFileListValue or a CollectedNonMultipleSelectValue or Sequence<DOMString>
  // or a CollectedCustomElementValue
  typedef (DOMString or boolean or object) CollectedFormDataValue;

  dictionary CollectedFileListValue
  {
    DOMString type = "file";
    required sequence<DOMString> fileList;
  };

  dictionary CollectedNonMultipleSelectValue
  {
    required long selectedIndex;
    required DOMString value;
  };

  dictionary CollectedCustomElementValue
  {
    (File or USVString or FormData)? value = null;
    (File or USVString or FormData)? state = null;
  };

The dictionary ``CollectedData`` includes scroll position as well as form data fields, but scroll position is collected separately. Positions are stored as string ``"x,y"`` of a coordinate, and the scroll data builds a tree of data for the document tree. This means that scroll data can have the following form:

.. code-block:: json

  {
    "scroll": {
      "scroll": "0,132",
      "children": [
        {
          "scroll": "0,87"
        }
      ]
    }
  }

for a document scrolled to ``0,132`` containing an iframe scrolled to ``0,87``.

The collected form data builds up a similar tree of data for the document tree as the scroll data. The data collected is from the different form elements and data from an editable document. The different form elements are either identified by their ``id`` attribute, if they have one, or an xpath expression pointing to them. These are stored in records in the properties ``id`` and ``xpath``. Editable documents are stored in the property ``innerHTML``. This means that form data can have the following form:

.. code-block:: json

  {
    "formdata": {
      "url": "http://example.org/sessionstore.html",
      "id": {
        "input": "lorem ipsum"
      },
      "children": [
        {
          "url": "http://example.org/sessionstoreframe.html",
          "id": {
            "input": "dolor sit amet"
          },
          "xpath": {
            "/xhtml:html/xhtml:body/xhtml:select": {
              "selectedIndex": 1,
              "value": "2"
            }
          }
        }
      ]
    }
  }

``FormData`` and ``File`` are the same data as their web exposed counterparts in `FormData.webidl <https://searchfox.org/mozilla-central/source/dom/webidl/FormData.webidl>`__ and `File.webidl <https://searchfox.org/mozilla-central/source/dom/webidl/File.webidl>`__.

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Structure of data pushed by ``UpdateSessionStoreForStorage``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The data stored has the structure of a record of partitioned URIs mapping to a key/value record.

.. code-block:: json

  {
    "http://example.com": {
      "test": "lorem ipsum"
    },
    "https://example.org^partitionKey=%28http%2Cexample.com%29": {
      "test": "dolor sit amet"
    }
  }

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Structure of data retrieved by ``SessionHistory.collectFromParent``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: json

  {
    "entries": [
      {
        "url": "about:home",
        "title": "New Tab",
        "cacheKey": 0,
        "ID": 4,
        "docshellUUID": "{8d0d8d8f-7732-4d91-b146-f4e7baefd518}",
        "resultPrincipalURI": null,
        "principalToInherit_base64": "{\"0\":{\"0\":\"moz-nullprincipal:{c2bf9cd7-8940-4097-9dd5-2f65e5b50c78}\"}}",
        "hasUserInteraction": true,
        "triggeringPrincipal_base64": "{\"3\":{}}",
        "docIdentifier": 5,
        "persist": true
      },
      {
        "url": "http://elg.no/",
        "title": "http://elg.no/",
        "cacheKey": 0,
        "ID": 17,
        "docshellUUID": "{8d0d8d8f-7732-4d91-b146-f4e7baefd518}",
        "resultPrincipalURI": null,
        "hasUserInteraction": false,
        "triggeringPrincipal_base64": "{\"3\":{}}",
        "docIdentifier": 19,
        "persist": true
      }
    ],
    "requestedIndex": 0,
    "index": 2,
    "fromIdx": -1
  }
