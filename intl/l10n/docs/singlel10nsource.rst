==============================================
Single localization source for multiple builds
==============================================

Firefox uses one localization source for all of its build versions.

There's a number of upsides to this:

* Localizers maintain a single source of truth. They can translate Nightly,
  while updating Beta, Developer Edition or even Release and ESR.
* It extends the window of time localizers can work on strings before deadlines.
* Uplifting string changes has less of an impact on the localization toolchain,
  and their impact can be evaluated case by case.

So the problem at hand is to have one localization source
and use that to build multiple versions of Firefox. The goal is for that
localization to be as complete as possible for each version.

The process to tackle these follows these steps:

* Create resource to localize, `firefox-l10n-source`_.

  * Review updates to that resource in a *quarantine*.
  * Expose a known good state of that resource to localizers.

* Get content localized in Pontoon.
* Write localizations back to `firefox-l10n`_.
* Get localizations into the builds.

firefox-l10n-source repository
==============================

`firefox-l10n-source`_ acts as a unified source string repository for all
shipping Firefox versions (nightly, beta, release, ESR, etc.).
The repository consists of two branches, ``main`` and ``update``.

The ``main`` branch contains all final ``en-US`` strings. Pontoon syncs from
this branch exposing any new strings committed to this branch to Localizers.

The ``update`` branch acts as a quarantine. Scheduled GitHub actions are used
to regularly extract new messages from the Firefox source code in ``gecko-dev``
and add them to the ``update`` branch. Changes in the ``update`` branch are
merged into ``main`` after the Localization Team review.

.. note::

   The concept behind the quarantine in the process above is to
   protect localizers from churn on strings that have technical
   problems. Examples like that could be missing localization notes
   or copy that should be improved.


firefox-l10n repository
=======================

`firefox-l10n`_ acts as the source of truth of all localized strings. Once
the translation of a string is completed in Pontoon, the content is stored
in the associated sub-directory for each locale. These strings are then used
during build to create builds and langpacks for all shipping Firefox locales.

.. _firefox-l10n-source: https://github.com/mozilla-l10n/firefox-l10n-source
.. _firefox-l10n: https://github.com/mozilla-l10n/firefox-l10n
