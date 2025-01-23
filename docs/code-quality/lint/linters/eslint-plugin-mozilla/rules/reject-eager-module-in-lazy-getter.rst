reject-eager-module-in-lazy-getter
==================================

Rejects defining a lazy getter for module that's known to be loaded early in the
startup process and it is not necessary to lazy load it.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
    const { AppConstants } = ChromeUtils.importESModule(
      "resource://gre/modules/AppConstants.sys.mjs"
    );
