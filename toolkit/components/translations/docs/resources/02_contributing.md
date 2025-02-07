# Contributing to Translations

The following content goes more in-depth than the [Overview](./01_overview.md) section
to provide helpful information regarding contributing to Translations.

- [Source Code](#source-code)
- [Architecture](#architecture)
    - [JSActors](#jsactors)
- [Remote Settings](#remote-settings)
    - [Admin Dashboards](#admin-dashboards)
    - [Prod Admin Access](#prod-admin-access)
    - [Pulling From Different Sources](#pulling-from-different-sources)
    - [Versioning](#versioning)
      - [Non-Breaking Changes](#non-breaking-changes)
      - [Breaking Changes](#breaking-changes)
- [Language Identification](#language-identification)
- [End-to-end Tests](#end-to-end-tests)
    - [End-to-end Test Configuration](#end-to-end-test-configuration)
    - [Running End-to-end Tests Locally](#running-end-to-end-tests-locally)
- [Performance Tests](#performance-tests)
    - [Running Performance Tests Locally](#running-performance-tests-locally)
    - [Comparing Translations Performance in CI](#comparing-translations-performance-in-ci)
    - [Temporarily Increasing Sample Size](#temporarily-increasing-sample-size)
    - [Adding New Performance Tests](#adding-new-performance-tests)
---
## Source Code

The primary source code for Translations in Firefox lives in the following directories:

> * **[browser/components/translations]**
> * **[toolkit/components/translations]**

The primary code for model training and inference lives in GitHub in the following repository:

> * **[mozilla/translations]**

---
## Architecture

### JSActors

Translations functionality is divided into different classes based on which access privileges are needed.
Generally, this split is between [Parent] and [Child] versions of [JSWindowActors].

The [Parent] actors have access to privileged content and is responsible for things like downloading models
from [Remote Settings](#remote-settings), modifying privileged UI components etc.

The [Child] actors are responsible for interacting with content on the page itself, requesting content from
the [Parent] actors, and creating [Workers] to carry out tasks.

---
## Remote Settings

The machine-learning models and [WASM] binaries are all hosted in Remote Settings and are downloaded/cached when needed.

### Admin Dashboards

In order to get access to Translations content in the Remote Settings admin dashboards, you will need to request
access in the Remote Settings component on [Bugzilla].

Once you have access to Translations content in Remote Settings, you will be able to view it in the admin dashboards:

**Dev**<br>
> [https://settings.dev.mozaws.net/v0/admin](https://settings.dev.mozaws.net/v1/admin)

**Stage**<br>
> [https://remote-settings.allizom.org/v0/admin](https://settings-writer.stage.mozaws.net/v1/admin)

### Prod Admin Access

In order to access the prod admin dashboard, you must also have access to a VPN that is authorized to view the dashboard.
To gain access to the VPN, follow [Step 3] on this page in the Remote Settings documentation.

**Prod**<br>
> [https://remote-settings.mozilla.org/v1/admin](https://settings-writer.prod.mozaws.net/v1/admin)


### Pulling From Different Sources

When you are running Firefox, you can choose to pull data from **Dev**, **Stage**, or **Prod** by downloading and installing
the latest [remote-settings-devtools] Firefox extension.

### Versioning

Translations uses semantic versioning for all of its records via the **`version`** property.

#### Non-breaking Changes

Translations code will always retrieve the maximum compatible version of each record from Remote Settings.
If two records exist with different versions, (e.g. **`1.0`** and **`1.1`**) then only the version **`1.1`** record
will be considered.

This allows us to update and ship new versions of models that are compatible with the current source code and wasm runtimes
in both backward-compatible and forward-compatible ways. These can be released through remote settings independent of the
[Firefox Release Schedule].

#### Breaking Changes

Breaking changes for Translations are a bit more tricky. These are changes that make older-version records
incompatible with the current Firefox source code and/or [WASM] runtimes.

While a breaking change will result in a change of the semver number (e.g. **`1.1 ⟶ 2.0`**), this alone is not
sufficient. Since Translations always attempts to use the maximum compatible version, only bumping this number
would result in older versions of Firefox attempting to use a newer-version record that is no longer compatible with the
Firefox source code or [WASM] runtimes.

To handle these changes, Translations utilizes Remote Settings [Filter Expressions] to make certain records
available to only particular releases of Firefox. This will allow Translations to make different sets of Remote Settings records available to different versions
of Firefox.

```{admonition} Example

Let's say that Firefox 108 through Firefox 120 is compatible with translations model records in the **`1.*`** major-version range, however Firefox 121 and onward is compatible with only model records in the **`2.*`** major-version range.

This will allow us to mark the **`1.*`** major-version records with the following filter expression:

**`
"filter_expression": "env.version|versionCompare('108.a0') >= 0 && env.version|versionCompare('121.a0') < 0"
`**

This means that these records will only be available in Firefox versions greater than or equal to 108, and less than 121.

Similarly, we will be able to mark all of the **`2.*`** major-version records with this filter expression:

**`
"filter_expression": "env.version|versionCompare('121.a0') >= 0"
`**

This means that these records will only be available in Firefox versions greater than or equal to Firefox 121.

```

Tying breaking changes to releases in this way frees up Translations to make changes as large as entirely
switching one third-party library for another in the compiled source code, while allowing older versions of Firefox to continue utilizing the old library and allowing newer versions of Firefox to utilize the new library.

---
## Language Identification

Translations currently uses the [CLD2] language detector.

We have previously experimented with using the [fastText] language detector, but we opted to use [CLD2] due to complications with [fastText] [WASM] runtime performance. The benefit of the [CLD2] language detector is that it already exists in the Firefox source tree. In the future, we would still like to explore moving to a more modern language detector such as [CLD3], or perhaps something else.

---
## End-to-end Tests

A true [Remote Settings](#remote-settings) network connection is not available when running Firefox tests locally or in CI. As such, we cannot serve the WASM binary or the translation models to the Translations infrastructure the exact same way that we do in production.

Most of our integration tests in Firefox are written using a mocked, local instance of Remote Settings with a mocked translator that simply capitalizes text and appends the intended language tags to the end of the translated output. This works out nicely because it helps our UI tests be both quick and deterministic. However, it is important that we also test the full end-to-end connections, running our models within the inference engine.

This section covers the configurations and setup for running end-to-end Translations tests both locally and in CI.

### End-to-End Test Configuration

In order to provide genuine WASM and translation-model artifacts to end-to-end tests in CI, we simulate a local instance of [Remote Settings](#remote-settings) that [pulls from the file system](https://searchfox.org/mozilla-central/rev/d5f93d53d7d005bd303925bc166163f158142cfd/toolkit/components/translations/tests/browser/shared-head.js#860-925). In particular, this Remote Settings instance will look for files within a directory specified by the `MOZ_FETCHES_DIR` environment variable, which is a standard variable defined in Firefox CI for all fetch tasks.

In order to expose the correct artifacts to `MOZ_FETCHES_DIR` in CI, we specify [translations-fetch.yml] which gets run by Taskcluster any time Translations tests are pushed to CI. This file contains a list of URLs at which the artifacts will be downloaded and added to `MOZ_FETCHES_DIR`. They must match the specified size and hash in order to pass the fetch task.

### Running End-to-end Tests Locally

The `MOZ_FETCHES_DIR` environment variable is automatically set for pushes to CI, however it will not be set in your local environment by default. Running an end-to-end Translations test locally will result in a failure unless the proper artifacts are downloaded to a directory that is exported as `MOZ_FETCHES_DIR`.

For convenience, we provide a script that will automatically download the artifacts specified in [translations-fetch.yml]:

* [toolkit/components/translations/tests/scripts/download-translations-artifacts.py](https://searchfox.org/mozilla-central/source/toolkit/components/translations/tests/scripts/download-translations-artifacts.py)

You will then need to export that temporary directory as `MOZ_FETCHES_DIR` prior to running end-to-end Translations tests.

Once your `MOZ_FETCHES_DIR` is properly exported, you can run end-to-end tests the same way you would any other Translations mochitest, e.g.

```
./mach test browser_translations_e2e_full_page_translate_without_lexical_shortlist.js
```

---
## Performance Tests

Translations performance tests run very similarly to other Translatiosn end-to-end tests. The [configuration](#end-to-end-test-configuration) using `MOZ_FETCHES_DIR` is exactly the same.

### Running Performance Tests Locally

To run Translations performance tests locally, you will need to follow the same [steps](#running-end-to-end-tests-locally) that you would to run Translations end-to-end tests locally. The only difference is in the way the test is invoked, e.g.

```
./mach perftest browser/components/translations/tests/browser/browser_translations_perf_es_en.js
```

### Comparing Translations Performance in CI

Translations performance tests allow us to make changes and compare a [kernel density estimation] of their results using the [perf.compare] website.

To push a new run of our performance tests to the Try servers, you will run the following command:

```
./mach try perf --single-run --full --artifact
```

Once this pulls up the fuzzy finder, you can filter on `-tr8ns`, which should pull up our Translations performance tests for each operating system.

You will need to do this once for your base revision, and once for your modified revision. Note that if your base revision is simply the tip of central with no other commits, then you can omit the `--single-run` flag and it should automatically push up two revisions to try: one without your changes and one with your changes.

Once you have the two revision hashes from the Try servers, you can simply add them to the [perf.compare] website to view the results once they are ready. Ensure that the category to compare is set to `mozperftest`.

#### Temporarily Increasing Sample Size

Our performance tests run whenever code is merged to autoland, to help detect regressions or improvements to the performance of the code. However, the test may only run a limited number of times so as not to waste resources in automation.

If you are trying to make subtle changes, or perhaps you just want a higher confidence in the statistical significance of your changes, you can increase the run count in your base revision and modified revision by modifying the [runCount](https://searchfox.org/mozilla-central/rev/d5f93d53d7d005bd303925bc166163f158142cfd/browser/components/translations/tests/browser/browser_translations_perf_es_en.js#62-76) in the test. Note that you may also have to increase the test timeout, depending on the `runCount`.

This is why it may be useful to push up the base revision and modified revision separately using the `--single-run` flag, rather than letting the base revision simply be the tip of central.

### Adding New Performance Tests

In order to add a new performance test, you may need to add new language-model artifacts to [translations-fetch.yml] described in the [configuration](#end-to-end-test-configuration) section.

You will also need to add a new test page and [register](https://searchfox.org/mozilla-central/rev/d5f93d53d7d005bd303925bc166163f158142cfd/browser/components/translations/tests/browser/head.js#420-434) its language, word count, and token count with the TranslationsBencher. For your convenience, we provide a script that will analyze the HTML of your test page, ensuring that it has a valid language tag, then providing you with the word count and token count.

* [toolkit/components/translations/tests/scripts/translations-perf-data.py](https://searchfox.org/mozilla-central/source/toolkit/components/translations/tests/scripts/translations-perf-data.py)




<!-- Hyperlinks -->
[browser/components/translations]: https://searchfox.org/mozilla-central/search?q=&path=browser%2Fcomponents%2Ftranslations&case=false&regexp=false
[Bugzilla]: https://bugzilla.mozilla.org/enter_bug.cgi?product=Cloud%20Services&component=Server%3A%20Remote%20Settings
[Child]: https://searchfox.org/mozilla-central/search?q=TranslationsChild
[CLD2]: https://github.com/CLD2Owners/cld2
[CLD3]: https://github.com/google/cld3
[Download and Install]: https://emscripten.org/docs/getting_started/downloads.html#download-and-install
[emscripten (2.0.3)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#203-09102020
[emscripten (2.0.18)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#2018-04232021
[emscripten (3.1.35)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#3135---040323
[Environments]: https://remote-settings.readthedocs.io/en/latest/getting-started.html#environments
[eval()]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval
[fastText]: https://fasttext.cc/
[Filter Expressions]: https://remote-settings.readthedocs.io/en/latest/target-filters.html#filter-expressions
[Firefox Release Schedule]: https://wiki.mozilla.org/Release_Management/Calendar
[generate functions]: https://emscripten.org/docs/api_reference/emscripten.h.html?highlight=dynamic_execution#functions
[Getting Set Up To Work On The Firefox Codebase]: https://firefox-source-docs.mozilla.org/setup/index.html
[importScripts()]: https://developer.mozilla.org/en-US/docs/Web/API/WorkerGlobalScope/importScripts
[JSWindowActors]: https://firefox-source-docs.mozilla.org/dom/ipc/jsactors.html#jswindowactor
[kernel density estimation]: https://en.wikipedia.org/wiki/Kernel_density_estimation
[minify]: https://github.com/tdewolff/minify
[mozilla/translations]: https://github.com/mozilla/translations
[Parent]: https://searchfox.org/mozilla-central/search?q=TranslationsParent
[perf.compare]: https://perf.compare
[Step 3]: https://remote-settings.readthedocs.io/en/latest/getting-started.html#create-a-new-official-type-of-remote-settings
[remote-settings-devtools]: https://github.com/mozilla-extensions/remote-settings-devtools/releases
[Remote Settings]: https://remote-settings.readthedocs.io/en/latest/
[toolkit/components/translations]: https://searchfox.org/mozilla-central/search?q=&path=toolkit%2Fcomponents%2Ftranslations&case=false&regexp=false
[translations-fetch.yml]: https://searchfox.org/mozilla-central/source/taskcluster/kinds/fetch/translations-fetch.yml
[WASM]: https://webassembly.org/
[Workers]: https://searchfox.org/mozilla-central/search?q=%2Ftranslations.*worker&path=&case=false&regexp=true
