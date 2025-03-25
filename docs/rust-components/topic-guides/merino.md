---
myst:
  enable_extensions: ["colon_fence"]
---

# Curated Recommendations Client

Fetches personalized content recommendations from the Merino Service. [Merino Curated Recommendations API Docs](https://merino.services.mozilla.com/docs#/default/curated_content_api_v1_curated_recommendations_post)

The API for the `CuratedRecommendationsClient` can be found in the Mozilla Rust components [Kotlin API Reference](https://mozilla.github.io/application-services/kotlin/kotlin-components-docs/mozilla.appservices.merino/index.html) and [Swift API Reference](https://mozilla.github.io/application-services/swift/Classes/CuratedRecommendationsClient.html).


## Prerequisites

Ensure that {doc}`viaduct` is initialized during application startup, as it is used for making network requests.

## Async

The Curated Recommendations API is synchronous, meaning calling it directly will block the current thread. To mitigate this, consumers should wrap the API in an async implementation.

## Importing the Client

:::{tab-set-code}

```kotlin
import mozilla.appservices.merino.curatedrecommendations.CuratedRecommendationsClient
import mozilla.appservices.merino.curatedrecommendations.CuratedRecommendationsRequest
import mozilla.appservices.merino.curatedrecommendations.CuratedRecommendationsResponse
import mozilla.appservices.merino.curatedrecommendations.CuratedRecommendationsError
```


```swift
import MozillaAppServices
```
:::

## Initializing the Curated Recommendations Client

The `CuratedRecommendationsClient` requires a `userAgentHeader` and optionally accepts a `baseHost` for customizing the target environment. By default, it uses the production host.

:::{tab-set-code}
```kotlin

val client = CuratedRecommendationsClient(
    baseHost = "https://merino.services.mozilla.com",
    userAgentHeader = "Mozilla/5.0"
)

```

```swift

let client = CuratedRecommendationsClient(
    baseHost: "https://merino.services.mozilla.com",
    userAgentHeader: "Mozilla/5.0"
)

```
:::


## Fetching Curated Recommendations

The `getCuratedRecommendations()` method fetches recommendations based on the provided request parameters.

:::{tab-set-code}
```kotlin

val request = CuratedRecommendationsRequest(
    locale = Locale.EN_US,
    region = "US",
    count = 4,
    topics = listOf("business"),
    feeds = listOf("sections")
)

try {
    val response: CuratedRecommendationsResponse = client.getCuratedRecommendations(request)
    println("Received recommendations: $response")
} catch (e: CuratedRecommendationsError) {
    println("Error fetching recommendations: ${e.message}")
}

```

```swift

let request = CuratedRecommendationsRequest(
    locale: Locale.en-US,
    region: "US",
    count: 4,
    topics: ["business"],
    feeds: ["sections"]
)

do {
    let response = try client.getCuratedRecommendations(request: request)
    print("Received recommendations: \(response)")
} catch {
    print("Error fetching recommendations: \(error)")
}

```
:::

## Data Models

### Curated Recommendations Request Model

The `CuratedRecommendationsRequest` model defines the parameters required to request curated recommendations.

### Request Fields
| **Field** | **Type** | **Description** |
|-----------|---------|----------------|
| `locale` | `string` | The Firefox installed locale, e.g., `en`, `en-US`, `de-DE`. Determines the language of recommendations. |
| `region` | `string (optional)` | _(Optional)_ The country-level region, e.g., `US` or `IE`. Helps return more relevant recommendations. If not provided, it is extracted from `locale` if it contains two parts (e.g., `en-US`). |
| `count` | `integer (optional)` | _(Optional)_ The maximum number of recommendations to return. Defaults to `100`. |
| `topics` | `array<string> (optional)` | _(Optional)_ A list of preferred [curated topics](https://mozilla-hub.atlassian.net/wiki/x/LQDaMg). |
| `feeds` | `array<string> (optional)` | _(Optional)_ A list of additional data feeds. Accepted values: `"need_to_know"`, `"fakespot"`, and `"sections"`. |
| `sections` | `array<object> (optional)` | _(Optional)_ A list of section settings that the user follows or has blocked. |
| `experimentName` | `string (optional)` | _(Optional)_ The Nimbus New Tab experiment name that the user is enrolled in. Used to run backend experiments independently of Firefox releases. |
| `experimentBranch` | `string (optional)` | _(Optional)_ The branch name of the Nimbus experiment that the user is in. |
| `enableInterestPicker` | `boolean (optional, default: false)` | _(Optional, defaults to `false`)_ If `true`, the API response will include an `interestPicker` object with sections for interest bubbles. |



### Curated Recommendations Response Model

The `CuratedRecommendationsResponse` model defines the response format containing recommendations.

### Response Fields
| **Field** | **Type** | **Description** |
|-----------|---------|----------------|
| `recommendedAt` | `integer` | The timestamp (in milliseconds) indicating when the recommendations were generated. |
| `data` | `array<object>` | A list of curated recommendation items. |
| `feeds` | `object (optional)` | _(Optional)_ A structured set of multiple curated recommendation lists. |
| `interestPicker` | `object (optional)` | _(Optional)_ Returned if `enableInterestPicker` is `true` in the request. Specifies the display order (`receivedFeedRank`) and a list of sections (referenced by `sectionId`) for interest bubbles. The text in these bubbles should match the corresponding section title. |


## Error Handling

The Curated Recommendations component defines the following error hierarchy:
- **`CuratedRecommendationsApiError`**: Base error
    - **`Network(reason: string)`**: Network error while making a request.
    - **`Other(code: integer (optional), reason: string)`**: Generic error containing an HTTP status code and message.


### Handling Errors in Kotlin and Swift

:::{tab-set-code}

```kotlin
fun fetchCuratedRecommendations() {
    try {
        val response = client.getCuratedRecommendations(request)
    } catch (e: CuratedRecommendationsError.Network) {
        // Log and retry after 5 minutes
        Log.w("Network error when fetching Curated Recommendations: ${e.reason}")
        scheduleRetry(300)
    } catch (e: CuratedRecommendationsError.Other) {
        when (e.code) {
            400 -> Log.e("Bad Request: ${e.reason}")
            422 -> Log.e("Validation Error: ${e.reason}")
            in 500..599 -> Log.e("Server Error: ${e.reason}")
            else -> Log.e("Unexpected Error: ${e.reason}")
        }
    }
}

```

```swift
func fetchCuratedRecommendations() {
    do {
        let response = try client.getCuratedRecommendations(request)
    } catch CuratedRecommendationsError.Network(let reason) {
        // Log and retry after 5 minutes
        print("Network error when fetching Curated Recommendations: \(reason)")
        scheduleRetry(seconds: 300)
    } catch CuratedRecommendationsError.Other(let code, let reason) {
        switch code {
        case 400:
            print("Bad Request: \(reason)")
        case 422:
            print("Validation Error: \(reason)")
        case 500...599:
            print("Server Error: \(reason)")
        default:
            print("Unexpected Error: \(reason)")
        }
    }
}

```
:::
