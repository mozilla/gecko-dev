---
myst:
  enable_extensions: ["colon_fence"]
---

# Remote Settings Client

The API for the Remote Settings can be found in the Mozilla Rust components [Kotlin API Reference](https://mozilla.github.io/application-services/kotlin/kotlin-components-docs/mozilla.appservices.remotesettings/index.html) and [Swift API Reference](https://mozilla.github.io/application-services/swift/Classes/RemoteSettings.html).

## Prerequisites

That {doc}`viaduct` must be initialized during application startup.

## Async

The Remote Settings API is synchronous, which means calling it directly will block the current
thread.  To deal with this, all current consumers wrap the API in order to make it async.  For
details on this wrapping, see the consumer code itself.

## Importing items

:::{tab-set-code}

```kotlin
import mozilla.appservices.remotesettings.RemoteSettingsClient
import mozilla.appservices.remotesettings.RemoteSettingsConfig2
import mozilla.appservices.remotesettings.RemoteSettingsException
import mozilla.appservices.remotesettings.RemoteSettingsServer
import mozilla.appservices.remotesettings.RemoteSettingsService
```


```swift
import MozillaAppServices
```
:::

## Application-level setup

Applications should create an app-wide `RemoteSettingsService`.  This manages which
remote settings server to make requests to, syncing data with that server, etc.

`RemoteSettingsService` instances are created using a config.  The name is because there is an
older/semi-deprecated `RemoteSettingsConfig` class.  The DISCO team plans to rename this class and
remove the `2` once we move consumers over to the new API.

:::{tab-set-code}

```kotlin
val config = RemoteSettingsConfig2(
   // Remote Settings server to connect to.  Other options are:
   //  * RemoteSettingsServer.Stage()
   //  * RemoteSettingsServer.Dev()
   //  * RemoteSettingsServer.Custom(url)
   server = RemoteSettingsServer.Prod(),
   storageDir = pathToDir,
   // optional field to fetch from a non-standard bucket
   bucketName = if (usePreviewBucket) { "main-preview" } else { "main" }
)

val appWideRemoteSettingsService = RemoteSettingsService(config)
```

```swift
let config = RemoteSettingsConfig2(
   // Remote Settings server to connect to.  Other options are:
   //  * RemoteSettingsServer.stage
   //  * RemoteSettingsServer.dev
   //  * RemoteSettingsServer.custom(url: url)
   server = RemoteSettingsServer.prod,
   storageDir = pathToDir,
   // optional field to fetch from a non-standard bucket
   bucketName = if usePreviewBucket { "main-preview" } else { "main" }
)

let appWideRemoteSettingsService = RemoteSettingsService(config: config)
```
:::

## Creating Remote Settings clients

`RemoteSettingsService` instances can be used to create new `RemoteSettingsClient` instances that
fetch remote settings records for a particular collection.

:::{tab-set-code}

```kotlin
val remoteSettingsClient = appWideRemoteSettingsService.makeClient("my-collection")
```

```swift
let remoteSettingsClient = appWideRemoteSettingsService.makeClient(collection: "my-collection")
```
:::


## Getting records

`RemoteSettingsClient` instances can be used to fetch remote settings records.  Records have some standard attributes
(`id`, `lastModified`, etc) and also have the `fields` attribute which stores all other JSON data
serialized as a string.

`getRecords` does not make a network request, instead it returns the last synced data with the
server.  This makes it safe to call in early startup where starting up a new network request is not
desirable.  However, this means that it returns a nullable value, which must be checked.


:::{tab-set-code}

```kotlin
fun processRecords(remoteSettingsClient: RemoteSettingsClient) {
    val records = remoteSettingsClient.getRecords()
    if (records != null) {
        for (record in records) {
            processRecord(record.id, deserialize(record.field))
        }
    }
}
```

```swift
func processRecords(remoteSettingsClient: RemoteSettingsClient) {
    let records = remoteSettingsClient.getRecords()
    if let records = records {
        for record in records {
            processRecord(id: record.id, recordData: deserialize(record.field))
        }
    }
}
```
:::

`getRecordsMap` works similarly, but it returns a map with the record ID as the key.  Again,
this value is nullable since the client may not have synced any data yet.

:::{tab-set-code}

```kotlin
fun valueOfFeature(remoteSettingsClient: RemoteSettingsClient): Boolean {
    val records = remoteSettingsClient.getRecordsMap()
    if (records != null) {
        return deserializeFeatureValue(records["featureName"].field)
    } else {
        return DEFAULT_FEATURE_VALUE
    }
}
```

```swift
func valueOfFeature(remoteSettingsClient: RemoteSettingsClient): Bool {
    let records = remoteSettingsClient.getRecordsMap()
    if let records = records {
        return deserializeFeatureValue(recordData: records["featureName"].field)
    } else {
        return DEFAULT_FEATURE_VALUE
    }
}
```
:::

Both `getRecords` and `getRecordsMap` input an optional `syncIfEmpty` parameter.  Pass
`syncIfEmpty=true` to sync records with the server if they haven't been synced before. Even with
this parameter, you should still check for null, which will be returned if the network request
fails.  `syncIfEmpty` should be used with caution, since there can be a delay in fetching the
setting.  For example, it could delay UI updates.

## Getting attachment data

`RemoteSettingsRecord` instances have an optional attachment field.  If present, you can download
the attachment data as a byte array using `RemoteSettingsClient.getAttachment`.  This will make a
network request unless the attachment data is cached.


:::{tab-set-code}

```kotlin
    val records = remoteSettingsClient.getRecords()
    if (records.size > 0 && records[0].attachment != null) {
        val attachmentData: ByteArray = remoteSettingsClient.getAttachment(records[0].attachment.location)
        // do something with the attachment data
    }
}
```

```swift
    let records = remoteSettingsClient.getRecords()
    if (records.count > 0 && records[0].attachment != nil) {
        val attachmentData: Data = remoteSettingsClient.getAttachment(location: records[0].attachment.location)
        // do something with the attachment data
    }
```
:::

## Syncing with the server

Use `RemoteSettingsService.sync()` to synchronize remote settings data with the server.  This will
fetch remote settings data for all clients created with the `RemoteSettingsService` that are still
alive.  This synchronization can take a significant amount of time and should probably be run in a
worker queue.

## Exception handling

The Remote Settings component defines the following error hierarchy:

- **RemoteSettingsError**: Base error
    - **RemoteSettingsError.Network(reason: string)**: Network error while making a request
    - **RemoteSettingsError.Backoff(seconds: int)**: The server requested a request backoff of at least [seconds]
    - **RemoteSettingsError.Other(reason: string)**: Catch-all for other remote settings errors

How this works depends on the language:

:::{tab-set-code}

```kotlin
fun remoteSettingsPeriodicSync() {
    // On Kotlin, errors are sealed/nested classes.
    // "Error" is replaced with "Exception" for consistency with other exceptions.
    try {
        appWideRemoteSettingsService.sync()
    } catch (e: RemoteSettingsException.Network) {
        // Try again after 5 minutes
        Log.w("Network error when syncing Remote Settings: ${e.reason}")
        scheduleRemoteSettingsSyncAfter(300)
    } catch (e: RemoteSettingsException.Backoff) {
        Log.w("Backoff error when syncing Remote Settings")
        scheduleRemoteSettingsSyncAfter(e.seconds)
    } catch (e: RemoteSettingsException.Other) {
        // There's no reason to think another sync will work.
        // Sync again using the normal schedule
        Log.w("Unexpected error when syncing Remote Settings: ${e.reason}")
    }
}
```

```swift
func remoteSettingsPeriodicSync() {
    // On Swift errors are members of the base error enum.
    do {
        appWideRemoteSettingsService.sync()
    } catch RemoteSettingsError.Network(let reason) {
        // Try again after 5 minutes
        print("Network error when syncing Remote Settings: \(reason)")
        scheduleRemoteSettingsSyncAfter(seconds: 300)
    } catch RemoteSettingsError.Backoff(let seconds) {
        print("Backoff error when syncing Remote Settings")
        scheduleRemoteSettingsSyncAfter(seconds: seconds)
    } catch RemoteSettingsError.Other(let reason) {
        // There's no reason to think another sync will work.
        // Sync again using the normal schedule
        print("Unexpected error when syncing Remote Settings: \(reason)")
    }
}
```
:::

`RemoteSettingsClient.getRecords` and `RemoteSettingsClient.getRecordsMap` never throw.  If they
encounter an error, they will record it using internal metrics/error reports then return `null`.
The reason for this is that code that calls those methods will certainly handle exceptions in the
same way as `null` and this avoids duplicating that code.

## Preventing nulls with scheduled downloads

The Remote Settings module has a system in place where we download Remote Settings collections on a
regular basis and store the data inside the library itself.  This data is used as a fallback
whenever `getRecords` or `getRecordsMap` would return `null`.  This can simplified consumer, since
it doesn't need an extra branch to handle missing data. This also can reduce network traffic, since
we only need to fetch new records if they've been updated since the last download.

If you would like your collection to be downloaded on this schedule, please contact the DISCO team
and we can set it up.
