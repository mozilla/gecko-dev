Dot Mozilla Reference
=====================

This linter is to help ensure that new code that adds references to ".mozilla" does not do so without ensuring that X Desktop Group (XDG) configuration path handling is taken care of.

The `XDG Base Directory Specification <https://specifications.freedesktop.org/basedir-spec/0.8/>`_
provides a set of environment variables as well as directories that
applications should follow to store their data: configurations, states, caches etc.

Firefox originally used ``$HOME/.mozilla`` but after Firefox 143 (bug 259356), it will prefer the environment variables over the legacy location.

If you are adding a path that would reference ``.mozilla``, then you should:

 * Contact the `OS Integration team`

 * Add handling like e.g. in :searchfox:`crash reporter client application <../rev/f30ba9df8307c48346ac1038be981595bd585603/toolkit/crashreporter/client/app/src/config.rs#511-532>`

 * Add an exclusion to the relevant section in :searchfox:`tools/lint/dot-mozilla-reference.yml <tools/lint/dot-mozilla-reference.yml>`

 If you are adding code that uses ``.mozilla`` for something other than referencing the data path, then you may:

 * Add an exclusion to the relevant section in :searchfox:`tools/lint/dot-mozilla-reference.yml <tools/lint/dot-mozilla-reference.yml>`

Run Locally
-----------

The mozlint integration of codespell can be run using mach:

.. parsed-literal::

    $ mach lint --linter dot-mozilla-reference <file paths>


Configuration
-------------

This linter is enabled on the whole code base. See the description above for details on handling exclusions.

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/dot-mozilla-reference.yml>`
