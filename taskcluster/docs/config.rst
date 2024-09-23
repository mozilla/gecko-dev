Taskcluster Configuration
=========================

Taskcluster requires configuration of many resources to correctly support Firefox CI.
Many of those span multiple projects (branches) instead of riding the trains.

Global Settings
---------------

The data behind configuration of all of these resources is kept in the `fxci-config`_ repository.
The files in this repository are intended to be self-documenting, but one of particular interest is ``projects.yml``, which describes the needs of each project.

Configuration Implementation
----------------------------

Translation of `fxci-config`_ to Taskcluster resources, and updating those resources, is handled by `ci-admin`_.
This is a small Python application with commands to generate the expected configuration, compare the expected to actual configuration, and apply the expected configuration.
Only the ``apply`` subcommand requires elevated privileges.

This tool automatically annotates all managed resources with "DO NOT EDIT", warning users of the administrative UI that changes made through the UI may be reverted.

Changing Configuration
----------------------

To change Taskcluster configuration, make patches to `fxci-config`_, using the Firefox Build System :: Task Configuration Bugzilla component.
The resulting configuration is applied upon landing.

See also the `releng documentation`_.

.. _fxci-config: https://github.com/mozilla-releng/fxci-config
.. _ci-admin: https://github.com/mozilla-releng/fxci-config/tree/main/src/ciadmin
.. _releng documentation: https://docs.mozilla-releng.net/en/latest/how-to/taskcluster/ci_admin.html
