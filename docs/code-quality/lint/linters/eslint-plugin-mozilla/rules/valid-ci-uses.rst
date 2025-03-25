valid-ci-uses
=============

Ensures that interface accesses on ``Ci`` are valid, and property accesses on
``Ci.<interface>`` are also valid.

This rule requires a full build to run, and is not turned on by default. To run
this rule manually, use:

.. code-block:: console

    MOZ_OBJDIR=objdir-ff-opt ./mach eslint --rule="mozilla/valid-ci-uses: error" *

.. note::
    Note that for platform-specific interfaces, you need to manually add the
    interface in question to
    :searchfox:`the rule <tools/lint/eslint/eslint-plugin-mozilla/lib/rules/valid-ci-uses.js>`
    (as a build on any given platform cannot find out about interfaces that are
    not available on that platform).


Examples of incorrect code for this rule:
-----------------------------------------

``nsIFoo`` does not exist.

.. code-block:: js

    Ci.nsIFoo

``UNKNOWN_CONSTANT`` does not exist on nsIURIFixup.

.. code-block:: js

    Ci.nsIURIFixup.UNKNOWN_CONSTANT

Examples of correct code for this rule:
---------------------------------------

``nsIFile`` does exist.

.. code-block:: js

    Ci.nsIFile

``FIXUP_FLAG_NONE`` does exist on nsIURIFixup.

.. code-block:: js

    Ci.nsIURIFixup.FIXUP_FLAG_NONE
