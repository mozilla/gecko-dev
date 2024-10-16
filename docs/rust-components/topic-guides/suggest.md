---
myst:
  enable_extensions: ["colon_fence"]
---

# Suggest

The API for the `SuggestStore` can be found in the [MozillaComponents Kotlin documentation](https://mozilla.github.io/application-services/kotlin/kotlin-components-docs/mozilla.appservices.suggest/-suggest-store/index.html).

```{note}
   Make sure you initialize {doc}`viaduct` for this component.
```

```{warning}
   The `SuggestStore` is synchronous, which means it needs to be wrapped in the asynchronous primitive of the target language you are using.
```


## Setting up the store

You need to import one or more of the following primitives to work with the `SuggestStore` (these come from the generated `suggest.kt` file, produced by `uniffi`):

:::{tab-set-code}

```kotlin
import mozilla.appservices.remotesettings.RemoteSettingsServer
import mozilla.appservices.suggest.SuggestApiException
import mozilla.appservices.suggest.SuggestIngestionConstraints
import mozilla.appservices.suggest.SuggestStore
import mozilla.appservices.suggest.SuggestStoreBuilder
import mozilla.appservices.suggest.Suggestion
import mozilla.appservices.suggest.SuggestionQuery
```

```swift
import MozillaAppServices
```
:::

Create a `SuggestStore` as a singleton. You do this via the `SuggestStoreBuilder`, which returns a `SuggestStore`. No I/O or network requests are performed during construction, which makes this safe to do at any point in the application startup:

:::{tab-set-code}
```kotlin
internal val store: SuggestStore = {
    SuggestStoreBuilder()
        .dataPath(context.getDatabasePath(DATABASE_NAME).absolutePath)
        .remoteSettingsServer(remoteSettingsServer)
        .build()
```

```swift
let store: SuggestStore = {
    let storeBuilder = SuggestStoreBuilder()
    storeBuilder.dataPath(context.getDatabasePath(DATABASE_NAME).absolutePath)
    storeBuilder.remoteSettingsServer(remoteSettingsServer)
    return storeBuilder.build()
}
```
:::

* You need to set the `dataPath`, which is the path (the SQLite location) where you store your suggestions.
* The `remoteSettingsServer` is only needed if you want to set the server to anything else but `prod`. If so, you pass a `RemoteSettingsServer` object.

## Ingesting suggestions

Ingesting suggestions happens in two different ways: On startup, and then, periodically, in the background.

* `SuggestIngestionConstraints` is used to control what gets ingested.
* Use the `providers` field to limit ingestion by provider type.
* Use the `providerConstraints` field to add additional constraints, currently this is only used for exposure suggestions.

### On Start Up

Ingest with `SuggestIngestionConstraints(emptyOnly=true)` shortly after each startup. This ensures we have something in the DB on the first run and also after upgrades where we often will clear the DB to start from scratch.

:::{tab-set-code}
```kotlin
store.value.ingest(SuggestIngestionConstraints(
    emptyOnly = true,
    providers = listOf(SuggestionProvider.AMP_MOBILE, SuggestionProvider.WIKIPEDIA, SuggestionProvider.WEATHER)
))
```

```swift
store.value.ingest(SuggestIngestionConstraints(
    emptyOnly: true,
    providers: [.AMP_MOBILE, .WIKIPEDIA, .WEATHER]
))
```
:::

### Periodically

Ingest with `SuggestIngestionConstraints(emptyOnly=false)` on a regular schedule (like once a day).

:::{tab-set-code}
```kotlin
store.value.ingest(SuggestIngestionConstraints(emptyOnly = false))
```

```swift
store.value.ingest(SuggestIngestionConstraints(emptyOnly: false))
```
:::

## Querying Suggestions

Call `SuggestStore::query` to fetch suggestions for the suggest bar. The `providers` parameter should be the same value that got passed to `ingest()`.

:::{tab-set-code}
```kotlin
store.value.query(
    SuggestionQuery(
        keyword = text,
        providers = listOf(SuggestionProvider.AMP_MOBILE, SuggestionProvider.WIKIPEDIA, SuggestionProvider.WEATHER),
        limit = MAX_NUM_OF_FIREFOX_SUGGESTIONS,
    ),
)
```

```swift
store.value.query(
    SuggestionQuery(
        keyword: text,
        providers: [.AMP_MOBILE, .WIKIPEDIA, .WEATHER],
        limit: MAX_NUM_OF_FIREFOX_SUGGESTIONS
    )
)
```
:::

## Interrupt querying

Call `SuggestStore::Interrupt` with `InterruptKind::Read` to interrupt any in-progress queries when the user cancels a query and before running the next query.

:::{tab-set-code}
```kotlin
store.value.interrupt()
```

```swift
store.value.interrupt()
```
:::

## Shutdown the store

On shutdown, call `SuggestStore::Interrupt` with `InterruptKind::ReadWrite` to interrupt any in-progress ingestion and queries.

:::{tab-set-code}
```kotlin
store.value.interrupt()
```

```swift
store.value.interrupt()
```
:::
