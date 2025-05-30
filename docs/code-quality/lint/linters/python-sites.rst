Python Sites
==================

This linter verifies that :searchfox:`python site <python/sites/>` files are
following the best practices.

Redundant Specifications
--------------------------------

If a dependency is specified in the :searchfox:`mach site <python/sites/>`, it
does not need to be specified in any command site (eg: `build.txt`, `common.txt`, etc).

This check warns against these redundant specifications, and can also fix them.

Specification Ordering
--------------------------------

The first line of any site file should start with `require-python:`. All subsequent lines
should be sorted alphabetically. This check ensures that's the case, and can fix the ordering
for you, while still maintaining comment block association.

Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter python-sites <file paths>

Configuration
-------------

This linter is enabled by default, and will run if you make changes to site files.

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/python-sites.yml>`
* :searchfox:`Source <tools/lint/python-sites/__init__.py>`
