Telemetry
=========

This document describes search telemetry recorded by Toolkit such as search
service telemetry and telemetry related to fetching search suggestions.

This document only covers Legacy telemetry, not Glean telemetry.
Glean metrics are self-documenting and can be looked up in the Glean dictionary.

Other important search-related telemetry is recorded by Firefox and is
documented in :doc:`/browser/search/telemetry` in the Firefox documentation.

Legacy Telemetry
----------------

Scalars
-------

browser.searchinit.secure_opensearch_engine_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the number of secure (i.e., using https) OpenSearch search
  engines a given user has installed.

browser.searchinit.insecure_opensearch_engine_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the number of insecure (i.e., using http) OpenSearch search
  engines a given user has installed.

browser.searchinit.secure_opensearch_update_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the number of OpenSearch search engines with secure updates
  enabled (i.e., using https) a given user has installed.

browser.searchinit.insecure_opensearch_update_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the number of OpenSearch search engines with insecure updates
  enabled (i.e., using http) a given user has installed.

Keyed Scalars
-------------

browser.searchinit.engine_invalid_webextension
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  NOTE: This telemetry is no longer reported to legacy Telemetry. See changelog
  below.

  Records the WebExtension ID of a search engine where the saved search engine
  settings do not match the WebExtension.

  The keys are the WebExtension IDs. The values are integers:

  1. Associated WebExtension is not installed.
  2. Associated WebExtension is disabled.
  3. The submission URL of the associated WebExtension is different to that of the saved settings.

  Changelog
    Firefox 134
      Legacy ``browser.searchinit.engine_invalid_webextension`` telemetry
      mirrored to Glean. (See bug 1927093)

    Firefox 139
      Legacy ``browser.searchinit.engine_invalid_webextension`` telemetry
      removed completely. (See bug 1958170)

Histograms
----------

SEARCH_SUGGESTIONS_LATENCY_MS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  This histogram records the latency in milliseconds of fetches to the
  suggestions endpoints of search engines, or in other words, the time from
  Firefox's request to a suggestions endpoint to the time Firefox receives a
  response. It is a keyed exponential histogram with 50 buckets and values
  between 0 and 30000 (0s and 30s). Keys in this histogram are search engine IDs
  for built-in search engines and 'other' for non-built-in search engines.

Default Search Engine
~~~~~~~~~~~~~~~~~~~~~
Telemetry for the user's default search engine is currently reported via two
systems:

  1. Legacy telemetry:
     `Fields are reported within the legacy telemetry environment <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/data/environment.html#defaultsearchengine>`__
  2. Glean:
     `Fields are documented in the Glean dictionary <https://dictionary.telemetry.mozilla.org/apps/firefox_desktop?search=search.engine>`__.

Glean Telemetry
---------------
`These search service fields are documented via Glean dictionary <https://dictionary.telemetry.mozilla.org/apps/firefox_desktop?search=tags%3A%22Firefox%20%3A%3A%20Search%22>`__.

search.service.startup_time
~~~~~~~~~~~~~~~~~~~~~~~~~~~

  The time duration it takes for the search service to start up.

search.service.initializationStatus
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  A labeled counter for the type of initialization statuses that can occur on
  start up. Labels include: ``failedSettings``, ``failedFetchEngines``,
  ``failedLoadEngines``, ``failedLoadSettingsAddonManager``, ``settingsCorrupt``,
  ``success``.

  A counter for initialization successes on start up.

search.suggestions.*
~~~~~~~~~~~~~~~~~~~~

  Labeled counters to count the number of suggestion requests sent from app-
  provided search engines. There are three separate counters for the number of
  successful, aborted and failed requests. Aborted requests can happen when
  users type faster than the search engine responds and failed requests when
  there is an HTTP or network error.
