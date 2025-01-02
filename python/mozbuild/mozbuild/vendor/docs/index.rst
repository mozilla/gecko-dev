================================
Vendoring Third Party Components
================================

The firefox source tree vendors many third party dependencies. The build system
provides a normalized way to keep track of:

1. The upstream source license, location and revision

2. (Optionally) The upstream source modification, including

   1. Mozilla-specific patches

   2. Custom update actions, such as excluding some files, moving files around
      etc.

This is done through a descriptive ``moz.yaml`` file added to the third
party sources, and the use of:

.. code-block:: sh

    ./mach vendor [options] ./path/to/moz.yaml

to interact with it.


Template ``moz.yaml`` file
==========================

.. literalinclude:: template.yaml
  :language: YAML

Common Vendoring Operations
===========================


Update to the latest upstream revision:

.. code-block:: sh

   ./mach vendor /path/to/moz.yaml


Check for latest revision, returning no output if it is up-to-date, and a
version identifier if it needs to be updated:

.. code-block:: sh

   ./mach vendor /path/to/moz.yaml --check-for-update

Vendor a specific revision:

.. code-block:: sh

   ./mach vendor /path/to/moz.yaml -r $REVISION --force


In the presence of patches, two steps are needed:

1. Vendor without applying patches (patches are applied *after*
   ``update-actions``) through ``--patch-mode none``

2. Apply patches on updated sources through ``--patch -mode only``

In the absence of patches, a single step is needed, and no extra argument is
required.
