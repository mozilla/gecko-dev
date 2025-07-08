Node Licenses
=============

This linter verifies that dependencies included by ``package.json`` files have
`accepted licenses <https://www.mozilla.org/en-US/MPL/license-policy/>`_.  Mozilla
employees can also consult the
`Licensing & Contributor Agreements Runbook <https://mozilla-hub.atlassian.net/l/cp/bgfp6Be7>`_
for more details.

**This linter currently only works for tools that are not incorporated into the
production code.**

Raised Node License Issues
--------------------------

If the linter raises an issue with a license, the license should be checked against
the Runbook, and if necessary, consult with the Legal team to ensure it is
acceptable.

Dependencies with unaccepted licenses must not be committed into the
repository. If this linter fails it will cause your changes to be backed out.

New licenses that have been accepted by Legal may be added to the
``accepted-test-licenses`` list in :searchfox:`node-licenses.yml <tools/lint/node-licenses.yml>`.

There is also a specific section in the configuration file ``known-packages`` where
a package may be specified if Legal has accepted the use of that package but is
not willing to allow the license generally.

Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter node-licenses <file paths>

Configuration
-------------

This linter is currently enabled on specific directories, as listed in the
:searchfox:`configuration file <tools/lint/node-licenses.yml>`.

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/node-licenses.yml>`
* :searchfox:`Source <tools/lint/node-licenses/__init__.py>`
