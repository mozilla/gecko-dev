.. role:: bash(code)
   :language: bash

.. role:: js(code)
   :language: javascript

.. role:: python(code)
   :language: python

===========================================
How Migrations Are Run on l10n Repositories
===========================================

Once a patch including new FTL strings and a migration recipe lands in
mozilla-central, the Localization Team will perform a series of actions to migrate
strings for all 100+ localizations:

 - New Fluent strings land in `mozilla-central`, together with a migration
   recipe.
 - New strings are added to an `update` branch of `firefox-l10n-source`_,
   a unified repository including strings for all shipping versions of Firefox,
   and used as a buffer before exposing strings to localizers.
 - Migration recipes are run against all l10n subfolders, migrating strings
   from old to new files, and storing them in VCS.
 - New en-US strings are merged into the `main` branch of `firefox-l10n-source`
   that syncs with localization tools, exposing strings to all localizers.

Migration recipes could be run multiple times within a release cycle if more
patches containing migrations land after the first.

Migration recipes are periodically removed from `mozilla-central`. This clean-up
process will typically leave recipes from the most recent 2 or 3 cycles. Older
recipes are stored in `this repository`__ .

.. tip::

  A script to run migrations on all l10n repositories is available in `this
  repository`__, automating part of the steps described for manual testing, and
  it could be adapted to local testing.

  __ https://github.com/flodolo/fluent-migrations
.. _firefox-l10n-source: https://github.com/mozilla-l10n/firefox-l10n-source/
