---
myst:
  enable_extensions: ["colon_fence"]
---

# Relevancy

The `relevancy` component tracks the user's interests locally, without sharing any data over the network. The component currently supports building an interest vector based on the URLs they visit.

## Prerequisites

That {doc}`viaduct` must be initialized during application startup.

## Async

The Relevancy API is synchronous, which means calling it directly will block the current
thread.  To deal with this, all current consumers wrap the API in order to make it async.  For
details on this wrapping, see the consumer code itself.

On JS, this wrapping is handled automatically by UniFFI.  See
https://searchfox.org/mozilla-central/source/toolkit/components/uniffi-bindgen-gecko-js/config.toml
for details on which functions/methods are wrapped to be async.

## Setting up the store

To use the `RelevancyStore` in either Kotlin or Swift, you need to import the relevant classes and data types from the `MozillaAppServices` library.

:::{tab-set-code}
```kotlin
import mozilla.appservices.relevancy.RelevancyStore
import mozilla.appservices.relevancy.InterestVector

val store = RelevancyStore(dbPath)
```

```swift
import MozillaAppServices

let store = RelevancyStore(dbPath: "path/to/database")
```

```js
ChromeUtils.defineESModuleGetters(lazy, {
  RelevancyStore: "resource://gre/modules/RustSuggest.sys.mjs",
});

const store = RelevancyStore.init(dbPath);
```
:::


* `dbPath`: This is the path to the SQLite database where the relevancy data is stored. The initialization is non-blocking, and the database is opened lazily.

## Ingesting relevancy data

To build the user's interest vector, call the `ingest` function with a list of URLs ranked by frequency. This method downloads the interest data, classifies the user's top URLs, and builds the interest vector. This process may take time and should only be called from a worker thread.

### Example usage of `ingest`:

:::{tab-set-code}
```kotlin
val topUrlsByFrequency = listOf("https://example.com", "https://another-example.com")
val interestVector = store.ingest(topUrlsByFrequency)
```

```swift
let topUrlsByFrequency = ["https://example.com", "https://another-example.com"]
let interestVector = store.ingest(topUrlsByFrequency)
```

```js
const topUrlsByFrequency = ["https://example.com", "https://another-example.com"];
const interestVector = await store.ingest(topUrlsByFrequency);
```
:::

* `topUrlsByFrequency`: A list of URLs ranked by how often and recently the user has visited them. This data is used to build the user's interest vector.
* The `ingest` function returns an `InterestVector`, which contains the user's interest levels for different tracked categories.
