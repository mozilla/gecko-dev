# Threading

Many Rust components are implemented with a blocking API - eg, they'll block on a database, network etc.
We can't naively block the JS main thread calling the component, since that could freeze the entire Firefox UI.
We therefore allow every sync callable to configure how it is presented to JS, with an appropriate background thread async mechanism used as necessary.

This is more of an issue for Desktop than Android/iOS because Kotlin/Swift can create worker queues and use those to schedule Rust calls.
JavaScript doesn't have an equivalent -- especially since Web Workers are not currently used in Desktop chrome code.
Functions that block must either be `async` in Rust or configured to be `wrapped-async` on Desktop.

## Sync calls

Sync calls are supported as long as they are non-blocking.
This requirement means that these calls are somewhat rare, but there are use-cases for them.
Here are some examples:

* **Interrupt/shutdown functions**
  These execute quickly and we typically want to run them immediately rather than schedule them in a task queue.
* **Builder object methods**
  These are non-blocking and it's usually more ergonomic to keep these non-async.
* **Non-blocking components**
  Some components, like the context ID component don't perform any network requests, IO, etc. inside the Rust code.
  These components can present a fully sync API.

## Wrapped-async calls

Sync Rust functions/methods/constructors can be configured to be wrapped-async to avoid blocking the main thread.
These functions are scheduled to run on a worker thread in the generated C++ code and the result is returned to JavaScript via a promise.
`toolkit/components/uniffi-bindgen-gecko-js/config.toml` configures which sync functions should be wrapped-async.

## Async calls

Rust async calls can be used from JavaScript as long as they are well-behaved and do not perform blocking operations between `await` points.

The only way to currently handle blocking operations in async functions is via async [trait interface methods](https://mozilla.github.io/uniffi-rs/latest/types/interfaces.html#exposing-traits-as-interfaces).
For example, Rust code can define a `Storage` trait with async save/load methods.
This code can then be implemented in `async` JavaScript functions.
This essentially means that blocking operations are handled by the SpiderMonkey JavaScript engine and the Rust code is non-blocking async code that awaits that.

Alternatively, trait interfaces can be implemented by Desktop Rust code.
This probably means using the [Firefox Rust XPCOM bridge](https://firefox-source-docs.mozilla.org/writing-rust-code/xpcom.html) to implement the callback interfaces.

## Can synchronous calls lock Mutexes?

UniFFI interfaces must be `Send + Sync`, since once they're passed to a foreign language that foreign language is free to move them between threads.
This is typically achieved by using a Mutex to protect the inner data.

In general, sync functions can lock these mutexes if the component does not have wrapped-async functions.
Digging deeper:

* If a component only exports sync functions, then the mutex will always be available so locking it is non-blocking.
* If the component exports wrapped-async calls, then locking the mutex is blocking if the wrapped-async calls also use the mutex.
  In this case, the sync functions may need to wait for the wrapped-async methods to finish and those functions are running blocking code.
* If a component exports async functions, then locking the mutex is non-blocking as long as the async functions are well-behaved.
  The only exception to this rule is if the async functions perform blocking IO without awaiting it or if they hold the mutex between await points.
  Sync functions cannot use async mutexes, so those are not an issue.

## Callback interface methods

A related issue comes up for callback interface methods.
JavaScript code can only be run from the main thread.
This means `async`/`wrapped-async` Rust calls running off the main thread can't make a synchronous callback interface calls.
UniFFI provides a couple ways to work around this.

## Fire-and-forget callback interface methods

Sync callback interface methods are currently always wrapped to be "fire-and-forget".
This means the JavaScript call is scheduled to run asynchronously and no value is returned back to Rust.
This is currently the only way to run synchronous callback interface methods and only works with methods that don't return values.

## Async Rust -> async callback interface methods are ok

Async callback interface methods can be called from async Rust code and don't require any special caveats.
