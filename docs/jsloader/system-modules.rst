System Modules
==============

Gecko uses a variant of the standard ECMAScript module to implement the
browser internals.

Each system module is a per-process singleton, shared among all consumers in
the process.

Shared System Global
--------------------

The shared system global is a privileged global dedicated for the system
modules.

All system modules are imported into the shared system global (except for
modules loaded into the `DevTools distinct system global`_).

See ``mozJSModuleLoader::CreateLoaderGlobal`` in `mozJSModuleLoader.cpp <https://searchfox.org/mozilla-central/source/js/xpconnect/loader/mozJSModuleLoader.cpp>`_ for details about the global and built-in functions.

Defining a Module
-----------------

The system module is written as a subset of the standard ECMAScript module
(see `Limitations`_ below), and symbols can be exported with the standard
``export`` declarations.

The system module uses the ``.sys.mjs`` filename extension.

.. code:: JavaScript

    // Test.sys.mjs

    export const TestUtils = {
      hello() {
        console.log("hello");
      }
    };

    export function TestFunc() {
      console.log("hi");
    }

System modules can use other extensions than ``.sys.mjs``, but in that case
make sure the right ESLint rules are applied to them.

Importing a Module
------------------

Immediate Import
^^^^^^^^^^^^^^^^

Inside all privileged code, system modules can be imported with
``ChromeUtils.importESModule``.
The system module is imported synchronously, and the namespace object is
returned.

.. note::

    At the script or module top-level, if the module is not going to be
    immediately and unconditionally used, please consider using
    ``ChromeUtils.defineESModuleGetters`` below instead, in order to improve
    the browser startup performance and the window open performance.

.. code:: JavaScript

    // Privileged code.

    const { TestUtils } =
      ChromeUtils.importESModule("resource://gre/modules/Test.sys.mjs");

    TestUtils.hello();

Inside system modules, other system modules can be imported with the regular
``import`` declaration and the dynamic ``import()``.

.. code:: JavaScript

    // System module top-level scope.

    import { TestUtils } from "resource://gre/modules/Test.sys.mjs";

    TestUtils.hello();

.. code:: JavaScript

    // A function inside a system module.

    async function f() {
      const { TestUtils } = await import("resource://gre/modules/Test.sys.mjs");
      TestUtils.hello();
    }

.. note::

    The ``import`` declaration and the dynamic ``import()`` can be used only
    from system modules.
    If the system module is imported from regular modules in some random global
    with these ways, the module is imported into that global instead of
    the shared system global, and it becomes a different instance.

Lazy Import
^^^^^^^^^^^

Modules can be lazily imported with ``ChromeUtils.defineESModuleGetters``.
``ChromeUtils.defineESModuleGetters`` receives a target object, and a object
that defines a map from the exported symbol name to the module URI.
Those symbols are defined on the target object as a lazy getter.
The module is imported on the first access, and the getter is replaced with
a data property with the exported symbol's value.

.. note::

    ``ChromeUtils.defineESModuleGetters`` is applicable only to the exported
    symbol, and not to the namespace object.
    See the next section for how to define the lazy getter for the namespace
    object.

The convention for the target object's name is ``lazy``.

.. code:: JavaScript

    // Privileged code.

    const lazy = {}
    ChromeUtils.defineESModuleGetters(lazy, {
      TestUtils: "resource://gre/modules/Test.sys.mjs",
    });

    function f() {
      // Test.sys.mjs is imported on the first access.
      lazy.TestUtils.hello();
    }

In order to import multiple symbols from the same module, add the corresponding
property with the symbol name and the module URI for each.

.. code:: JavaScript

    // Privileged code.

    const lazy = {}
    ChromeUtils.defineESModuleGetters(lazy, {
      TestUtils: "resource://gre/modules/Test.sys.mjs",
      TestFunc: "resource://gre/modules/Test.sys.mjs",
    });

See `ChromeUtils.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/ChromeUtils.webidl>`_ for more details.

Using the Namespace Object
--------------------------

The namespace object returned by the ``ChromeUtils.importESModule`` call
can also be directly used.

.. code:: JavaScript

    // Privileged code.

    const TestNS =
      ChromeUtils.importESModule("resource://gre/modules/Test.sys.mjs");

    TestNS.TestUtils.hello();

This is almost same as the following normal ``import`` declaration.

.. code:: JavaScript

    // System module top-level scope.

    import * as TestNS from "resource://gre/modules/Test.sys.mjs";

    TestNS.TestUtils.hello();

or the dynamic import without the destructuring assignment.

.. code:: JavaScript

    async function f() {
      const TestNS = await import("resource://gre/modules/Test.sys.mjs");
      TestNS.TestUtils.hello();
    }


``ChromeUtils.defineESModuleGetters`` does not support directly using
the namespace object.
Possible workaround is to use ``ChromeUtils.defineLazyGetter`` with
``ChromeUtils.importESModule``.

.. code:: JavaScript

    const lazy = {}
    ChromeUtils.defineLazyGetter(lazy, "TestNS", () =>
      ChromeUtils.importESModule("resource://gre/modules/Test.sys.mjs"));

    function f() {
      // Test.sys.mjs is imported on the first access.
      lazy.TestNS.TestUtils.hello();
    }


Importing from Unprivileged Testing Code
----------------------------------------

In unprivileged testing code such as mochitest plain,
``ChromeUtils.importESModule`` is available as
``SpecialPowers.ChromeUtils.importESModule``.

.. code:: JavaScript

    // Mochitest-plain testcase.

    const { TestUtils } =
      SpecialPowers.ChromeUtils.importESModule(
        "resource://gre/modules/Test.sys.mjs"
      );

Importing from C++ Code
-----------------------

C++ code can import ES modules with ``do_ImportESModule`` function.
The exported object should follow the specified XPCOM interface.

.. code:: c++

    nsCOMPtr<nsIUtils> utils = do_ImportESModule(
      "resource://gre/modules/Test.sys.mjs", "Utils");

See `nsImportModule.h <https://searchfox.org/mozilla-central/source/js/xpconnect/loader/nsImportModule.h>`_ for more details.

Lifetime
--------

The shared system global has the almost same lifetime as the process, and the
system modules are never unloaded until the end of the shared system global's
lifetime.

If a module need to be dynamically updated with the same URI, for example with
privileged extensions getting updated, they can add query string to distinguish
different versions.

Lifetime of the Global Variables
--------------------------------

Unlike the classic scripts, the ECMAScript's module's global variables are not
properties of any objects.

If the all strong references to the document goes away, the objects held by
the module global variables are ready to be GCed.  This means, the module global
variables don't have the same lifetime as the module itself.

In privileged scripts, there can be multiple usage of weak-references and
similar things, such as XPCOM ``nsISupportsWeakReference``, or
the window-less ``browser`` element and its content document.

If those objects needs to be kept alive longer, for example, if they need to
have the same lifetime as the module itself, there should be another strong
reference to them.

Possible options for those objects are the following:

  * Export the variable that holds the object
  * Store the object into the exported object's property
  * Close over the variable from the function that's reachable from the exported objects
  * Do not use weak reference

Utility Functions
-----------------

``Cu.isESmoduleLoaded`` is a function to query whether the module is already
imported to the shared system global.

.. code:: JavaScript

    if (Cu.isESmoduleLoaded("resource://gre/modules/Test.sys.mjs")) {
      // ...
    }

``Cu.loadedESModules`` returns a list of URLs of the already-imported modules.
This is only for startup testing purpose, and this shouldn't be used in
the production code.

.. code:: JavaScript

    for (const uri of Cu.loadedESModules) {
      // ...
    }

If ``browser.startup.record`` preference is set to ``true`` at the point of
importing modules, ``Cu.getModuleImportStack`` returns the call stack of the
module import.
This is only for the debugging purpose.

.. code:: JavaScript

    Services.prefs.setBoolPref("browser.startup.record", true);

    const { TestUtils } =
      ChromeUtils.importESModule("resource://gre/modules/Test.sys.mjs");

    console.log(
      Cu.getModuleImportStack("resource://gre/modules/Test.sys.mjs"));

See `xpccomponents.idl <https://searchfox.org/mozilla-central/source/js/xpconnect/idl/xpccomponents.idl>`_ for more details.

Limitations
-----------

Top-level ``await`` is not supported in the system module, due to the
requirement for synchronous loading.

DevTools Distinct System Global
-------------------------------

DevTools-related system modules can be imported into a separate dedicate global,
which is used when debugging the browser.

The target global can be controlled by the ``global`` property of the 2nd
parameter of ``ChromeUtils.importESModule``, or the 3rd parameter of
``ChromeUtils.defineESModuleGetters``.

The ``global`` property defaults to ``"shared"``, which is the shared system
global.
Passing ``"devtools"`` imports the module in the DevTools distinct system
global.

.. code:: JavaScript

    const { TestUtils } =
      ChromeUtils.importESModule("resource://gre/modules/Test.sys.mjs", {
        global: "devtools",
      });

    TestUtils.hello();

.. code:: JavaScript

    const lazy = {}
    ChromeUtils.defineESModuleGetters(lazy, {
      TestUtils: "resource://gre/modules/Test.sys.mjs",
    }, {
      global: "devtools",
    });

If the system module file is shared between both cases, ``"contextual"`` can be
used.  The module is imported into the DevTools distinct system global if the
current global is the DevTools distinct system global.  Otherwise the module
is imported into the shared system global.

See ``ImportESModuleTargetGlobal`` in `ChromeUtils.webidl <https://searchfox.org/mozilla-central/source/dom/chrome-webidl/ChromeUtils.webidl>`_ for more details.

Integration with JSActors
-------------------------

:ref:`JSActors <JSActors>` are implemented with system modules.

See the :ref:`JSActors <JSActors>` document for more details.

Integration with XPCOM Components
---------------------------------

:ref:`XPCOM Components <Defining XPCOM Components>` can be implemented with
system modules, by passing ``esModule`` option.

See the :ref:`XPCOM Components <Defining XPCOM Components>` document for more
details.

Importing into Current Global
-----------------------------

``ChromeUtils.importESModule`` can be used also for importing modules into
the current global, by passing ``{ global: "current" }`` option.
In this case the imported module is not a system module.

See the :ref:`JS Loader APIs <JS Loader APIs>` document for more details.

JSM
---

Prior to the ECMAScript-module-based system modules, Firefox codebase had been
using a Mozilla-specific module system called JSM.

The details around the migration is described in `the migration document <https://docs.google.com/document/d/1cpzIK-BdP7u6RJSar-Z955GV--2Rj8V4x2vl34m36Go/edit?usp=sharing>`_.
