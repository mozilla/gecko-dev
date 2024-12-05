JS Loader APIs
==============

Gecko provides multiple ways to load/evaluate JS files from JS files,
in addition to standard ways such as the dynamic ``import()`` and the worker's
``importScripts``.

Synchronous Classic Script Load
-------------------------------

``Services.scriptloader.loadSubScript`` can be used for synchronously loading
given classic script in the given global.

The script is evaluated in the 2nd parameter's global, and the loaded script's
global variables are defined into the given object.

.. code:: JavaScript

    Services.scriptloader.loadSubScript(
      "chrome://browser/content/browser.js", this
    );

See `mozIJSSubScriptLoader.idl <https://searchfox.org/mozilla-central/source/js/xpconnect/idl/mozIJSSubScriptLoader.idl>`_ for more details

Asynchronous Classic Script Compile
-----------------------------------

``ChromeUtils.compileScript`` can be used for asynchronously compile given
classic script, and execute in given globals.

.. code:: JavaScript

    async function f() {
      const script = await ChromeUtils.compileScript(
        "resource://test/some_script.js"
      );

      const result = script.executeInGlobal(targetGlobal1);

      // The script can be executed against multiple globals.
      const result2 = script.executeInGlobal(targetGlobal2);
    }

See `ChromeUtils.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/ChromeUtils.webidl>`_ and `PrecompiledScript.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/PrecompiledScript.webidl>`_ for more details.

Synchronous Module Import
-------------------------

``ChromeUtils.importESModule`` and ``ChromeUtils.defineESModuleGetters`` can be used for importing ECMAScript modules into the current global.

The last parameter of those API controls where to import the module.
Passing ``{ global: "current" }`` option makes it to import the module into the current global.

.. code:: JavaScript

    const { Utils } =
      ChromeUtils.importESModule("resource://gre/modules/Utils.sys.mjs", {
        global: "current",
      });

    Utils.hello();

    const lazy = {}
    ChromeUtils.defineESModuleGetters(lazy, {
      Utils2: "resource://gre/modules/Utils2.sys.mjs",
    }, {
      global: "current",
    });

    function f() {
      lazy.Utils2.hello();
    }

See :ref:`System Modules <System Modules>` for more details about those API.
