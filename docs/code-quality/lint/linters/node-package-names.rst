Node Package Names
==================

This linter verifies that ``package.json`` files don't include a ``name``
property, unless we are publishing them.

For ``package.json`` files that are only used for defining dependencies or
commands, we should not include a ``name`` property, as that can cause confusion
as to if the package is published or not.

Fixing Node Package Names Errors
--------------------------------

* If the directory/package is an imported third party package, it should be added
  to :searchfox:`ThirdPartyPaths.txt <tools/rewriting/ThirdPartyPaths.txt>`.
* If the directory/package is not going to be published, remove the ``name`` and
  ``version`` properties.
* If the directory/package is going to be published:

    * The name must include the ``@mozilla/`` scope.
    * It must be published under the Mozilla organisation on npmjs.com before landing.
    * Once the package has been published initially, it may be added as an exclusion
      in :searchfox:`node-package-names.yml <tools/lint/node-package-names.yml>`.

Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter node-package-names <file paths>

Configuration
-------------

This linter is enabled on most of the whole code base.

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/node-package-names.yml>`
* :searchfox:`Source <tools/lint/node-package-names/__init__.py>`
