.. _webextension-telemetry:

==============================
WebExtension API for Telemetry
==============================

Use the ``browser.telemetry`` API to send telemetry data to the Mozilla Telemetry service. Restricted to Mozilla privileged webextensions.

Types
-----

``ScalarType``
~~~~~~~~~~~~~~

Type of scalar: 'count' for numeric values, 'string' for string values, 'boolean' for boolean values. Maps to ``nsITelemetry.SCALAR_TYPE_*``.

``ScalarData``
~~~~~~~~~~~~~~

Represents registration data for a Telemetry scalar.

Properties:

* ``kind`` - See ScalarType_.
* ``keyed`` - *(optional, boolean)* True if this is a keyed scalar. Defaults to ``false``.
* ``record_on_release`` - *(optional, boolean)* True if this data should be recorded on release. Defaults to ``false``.
* ``expired`` - *(optional, boolean)* True if this scalar entry is expired. Operations on an expired scalar don't error (operations on an undefined scalar do), but the operations are no-ops. No data will be recorded. Defaults to ``false``.

``EventData``
~~~~~~~~~~~~~

Represents registration data for a Telemetry event.

Properties:

* ``methods`` - *(array)* List of methods for this event entry.
* ``objects`` - *(array)* List of objects for this event entry.
* ``extra_keys`` - *(array)* List of allowed extra keys for this event entry.
* ``record_on_release`` - *(optional, boolean)* True if this data should be recorded on release. Defaults to ``false``.
* ``expired`` - *(optional, boolean)* True if this event entry is expired. Recording an expired event doesn't error (operations on undefined events do). No data will be recorded. Defaults to ``false``.

Functions
---------

``submitPing``
~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.submitPing(type, message, options);

Submits a custom ping to the Telemetry backend. See :ref:`submitting-customping`.

* ``type`` - *(string)* The type of the ping.
* ``message`` - *(object)* The data payload for the ping.
* ``options`` - *(optional, object)* Options object.

  * ``addClientId`` - *(optional, boolean)* True if the ping should contain the client id and profile group id. Defaults to ``false``.
  * ``addEnvironment`` - *(optional, boolean)* True if the ping should contain the environment data. Defaults to ``false``.
  * ``overrideEnvironment`` - *(optional, object)* Set to override the environment data. Default: not set.
  * ``usePingSender`` - *(optional, boolean)* If true, send the ping using the PingSender. Defaults to ``false``.


``canUpload``
~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.canUpload();

Checks if Telemetry upload is enabled.

``scalarAdd``
~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.scalarAdd(name, value);

As of Firefox 134, this method is a no-op. See Bug 1930196.

Instead, use :doc:`Glean APIs <../../glean/user/glean_for_legacy_scalars>` to record scalars.

``scalarSet``
~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.scalarSet(name, value);

As of Firefox 134, this method is a no-op. See Bug 1930196.

Instead, use :doc:`Glean APIs <../../glean/user/glean_for_legacy_scalars>` to record scalars.

``scalarSetMaximum``
~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.scalarSetMaximum(name, value);

As of Firefox 134, this method is a no-op. See Bug 1930196.

Instead, use :doc:`Glean APIs <../../glean/user/glean_for_legacy_scalars>` to record scalars.

``recordEvent``
~~~~~~~~~~~~~~~

As of Firefox 132, this method is a no-op. See Bug 1894533.

Instead, use :doc:`Glean APIs <../../glean/user/glean_for_legacy_events>` to record events.

``registerScalars``
~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  browser.telemetry.registerScalars(category, data);

As of Firefox 134, this method is a no-op. See Bug 1930196.

Instead, use :doc:`Glean APIs <../../glean/user/glean_for_legacy_scalars>` to record scalars.

``registerEvents``
~~~~~~~~~~~~~~~~~~

As of Firefox 132, this method is a no-op. See Bug 1894533.

Instead, use :doc:`Glean event definitions <../../glean/user/glean_for_legacy_events>` for your extension's events.

``setEventRecordingEnabled``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As of Firefox 133, this method is a no-op. Event recording is enabled by default unconditionally. See Bug 1920562.

Instead, use :doc:`Glean event definitions <../../glean/user/glean_for_legacy_events>` for your extension's events.
