no-browser-refs-in-toolkit
==========================

Rejects for using any code from ``browser/`` (Desktop Firefox) inside
``toolkit``, as ``toolkit`` is for code shared with other Gecko consumers
like Firefox on Android, Thunderbird, etc.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    "chrome://browser/content/browser.xhtml"
    "moz-src:///browser/components/search/SearchUIUtils.sys.mjs"
    "moz-src://foo/browser/components/search/Foo.sys.mjs"
    "resource:///modules/BrowserWindowTracker.sys.mjs"
    "browser/browser.ftl"

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    "chrome://global/content/aboutAbout.html"
    "moz-src:///toolkit/components/search/SearchService.sys.mjs"
    "resource://gre/modules/AppConstants.sys.mjs"
    "toolkit/global/aboutFoo.ftl"
