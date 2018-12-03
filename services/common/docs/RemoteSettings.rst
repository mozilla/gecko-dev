.. _services/remotesettings:

===============
Remote Settings
===============

The `remote-settings.js <https://dxr.mozilla.org/mozilla-central/source/services/settings/remote-settings.js>`_ module offers the ability to fetch remote settings that are kept in sync with Mozilla servers.


Usage
=====

The ``get()`` method returns the list of entries for a specific key. Each entry can have arbitrary attributes, and can only be modified on the server.

.. code-block:: js

    const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js", {});

    const data = await RemoteSettings("a-key").get();

    /*
      data == [
        {label: "Yahoo",  enabled: true,  weight: 10, id: "d0782d8d", last_modified: 1522764475905},
        {label: "Google", enabled: true,  weight: 20, id: "8883955f", last_modified: 1521539068414},
        {label: "Ecosia", enabled: false, weight: 5,  id: "337c865d", last_modified: 1520527480321},
      ]
    */

    for(const entry of data) {
      // Do something with entry...
      // await InternalAPI.load(entry.id, entry.label, entry.weight);
    });

.. note::
    The data updates are managed internally, and ``.get()`` only returns the local data.
    The data is pulled from the server only if this collection has no local data yet and no JSON dump
    could be found (see :ref:`services/initial-data` below).

.. note::
    The ``id`` and ``last_modified`` (timestamp) attributes are assigned by the server.

Options
-------

The list can optionally be filtered or ordered:

.. code-block:: js

    const subset = await RemoteSettings("a-key").get({
      filters: {
        "property": "value"
      },
      order: "-weight"
    });

Events
------

The ``on()`` function registers handlers to be triggered when events occur.

The ``sync`` event allows to be notified when the remote settings are changed on the server side. Your handler is given an ``event`` object that contains a ``data`` attribute that has information about the changes:

- ``current``: current list of entries (after changes were applied);
- ``created``, ``updated``, ``deleted``: list of entries that were created/updated/deleted respectively.

.. code-block:: js

    RemoteSettings("a-key").on("sync", event => {
      const { data: { current } } = event;
      for(const entry of current) {
        // Do something with entry...
        // await InternalAPI.reload(entry.id, entry.label, entry.weight);
      }
    });

.. note::

    Currently, the synchronization of remote settings is triggered by its own timer every 24H (see the preference ``services.settings.poll_interval`` ).

File attachments
----------------

When an entry has a file attached to it, it has an ``attachment`` attribute, which contains the file related information (url, hash, size, mimetype, etc.). Remote files are not downloaded automatically.

.. code-block:: js

    const data = await RemoteSettings("a-key").get();

    data.filter(d => d.attachment)
        .forEach(async ({ attachment: { url, filename, size } }) => {
          if (size < OS.freeDiskSpace) {
            // Planned feature, see Bug 1501214
            await downloadLocally(url, filename);
          }
        });

.. _services/initial-data:

Initial data
------------

It is possible to package a dump of the server records that will be loaded into the local database when no synchronization has happened yet.

The JSON dump will serve as the default dataset for ``.get()``, instead of doing a round-trip to pull the latest data. It will also reduce the amount of data to be downloaded on the first synchronization.

#. Place the JSON dump of the server records in the ``services/settings/dumps/main/`` folder
#. Add the filename to the ``FINAL_TARGET_FILES`` list in ``services/settings/dumps/main/moz.build``

Now, when ``RemoteSettings("some-key").get()`` is called from an empty profile, the ``some-key.json`` file is going to be loaded before the results are returned.

.. note::

    JSON dumps are not shipped on Android to minimize the installer size.

Targets and A/B testing
=======================

In order to deliver settings to subsets of the population, you can set targets on entries (platform, language, channel, version range, preferences values, samples, etc.) when editing records on the server.

From the client API standpoint, this is completely transparent: the ``.get()`` method — as well as the event data — will always filter the entries on which the target matches.

.. note::

    The remote settings targets follow the same approach as the :ref:`Normandy recipe client <components/normandy>` (ie. JEXL filter expressions),


Uptake Telemetry
================

Some :ref:`uptake telemetry <telemetry/collection/uptake>` is collected in order to monitor how remote settings are propagated.

It is submitted to a single :ref:`keyed histogram <histogram-type-keyed>` whose id is ``UPTAKE_REMOTE_CONTENT_RESULT_1`` and the keys are prefixed with ``main/`` (eg. ``main/a-key`` in the above example).


Create new remote settings
==========================

Staff members can create new kinds of remote settings, following `this documentation <https://remote-settings.readthedocs.io/en/latest/getting-started.html>`_.

It basically consists in:

#. Choosing a key (eg. ``search-providers``)
#. Assigning collaborators to editors and reviewers groups
#. (*optional*) Define a JSONSchema to validate entries
#. (*optional*) Allow attachments on entries

And once done:

#. Create, modify or delete entries and let reviewers approve the changes
#. Wait for Firefox to pick-up the changes for your settings key


Advanced Options
================

``filterFunc``: custom filtering function
-----------------------------------------

By default, the entries returned by ``.get()`` are filtered based on the JEXL expression result from the ``filter_expression`` field. The ``filterFunc`` option allows to execute a custom filter (async) function, that should return the record (modified or not) if kept or a falsy value if filtered out.

.. code-block:: javascript

    RemoteSettings("a-collection", {
      filterFunc: (record, environment) => {
        const { enabled, ...entry } = record;
        return enabled ? entry : null;
      }
    });


Debugging and testing
=====================

Trigger a synchronization manually
----------------------------------

The synchronization of every known remote settings clients can be triggered manually with ``pollChanges()``:

.. code-block:: js

    await RemoteSettings.pollChanges()

The synchronization of a single client can be forced with the ``.sync()`` method:

.. code-block:: js

    await RemoteSettings("a-key").sync();

.. important::

    The above methods are only relevant during development or debugging and should never be called in production code.


Manipulate local data
---------------------

A handle on the local collection can be obtained with ``openCollection()``.

.. code-block:: js

    const collection = await RemoteSettings("a-key").openCollection();

And records can be created manually (as if they were synchronized from the server):

.. code-block:: js

    const record = await collection.create({
      domain: "website.com",
      usernameSelector: "#login-account",
      passwordSelector: "#pass-signin",
    }, { synced: true });

In order to bypass the potential target filtering of ``RemoteSettings("key").get()``, the low-level listing of records can be obtained with ``collection.list()``:

.. code-block:: js

    const subset = await collection.list({
      filters: {
        "property": "value"
      }
    });

The local data can be flushed with ``clear()``:

.. code-block:: js

    await collection.clear()

For further documentation in collection API, checkout the `kinto.js library <https://kintojs.readthedocs.io/>`_, which is in charge of the IndexedDB interactions behind-the-scenes.


Inspect local data
------------------

The internal IndexedDB of Remote Settings can be accessed via the Storage Inspector in the `browser toolbox <https://developer.mozilla.org/en-US/docs/Tools/Browser_Toolbox>`_.

For example, the local data of the ``"key"`` collection can be accessed in the ``remote-settings`` database at *Browser Toolbox* > *Storage* > *IndexedDB* > *chrome*, in the ``records`` store.


Remote Settings Dev Tools
-------------------------

The Remote Settings Dev Tools extension provides some tooling to inspect synchronization statuses, to change the remote server or to switch to *preview* mode in order to sign-off pending changes. `More information on the dedicated repository <https://github.com/mozilla/remote-settings-devtools>`_.


About blocklists
----------------

Addons, certificates, plugins, and GFX blocklists were the first use-cases of remote settings, and thus have some specificities.

For example, they leverage advanced customization options (bucket, content-signature certificate, target filtering etc.), and in order to be able to inspect and manipulate their data, the client instances must first be explicitly initialized.

.. code-block:: js

    const BlocklistClients = ChromeUtils.import("resource://services-common/blocklist-clients.js", {});

    BlocklistClients.initialize();

Then, in order to access a specific client instance, the bucket must be specified:

.. code-block:: js

    const collection = await RemoteSettings("addons", { bucketName: "blocklists" }).openCollection();

And in the storage inspector, the IndexedDB internal store will be prefixed with ``blocklists`` instead of ``main`` (eg. ``blocklists/addons``).

