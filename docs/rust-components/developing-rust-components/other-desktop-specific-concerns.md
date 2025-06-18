# Other desktop-specific concerns

## Releasing resources

Firefox Desktop does not allow "late writes" -- i.e. writes that happen during shutdown.
This is a particular concern to any component that holds an SQLite database.
If they are garbage collected at shutdown with the database still open, then SQLite will write to disk when it closes its connection, triggering a late write error.

The typical way to avoid this is to have a `shutdown` method, which closes all resources.
The JS code then adds a `profile-before-change` shutdown blocker, which calls `shutdown()`.
See the `ContentRelevancyManager.sys.mjs` for an example.

## 64-bit integers

The JS number type can only store 53 bit integer values safely.
If you have a `u64` or `i64` Rust value, make sure to only use values between [Number.MIN_SAFE_INTEGER](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/MIN_SAFE_INTEGER) and [Number.MAX_SAFE_INTEGER](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/MAX_SAFE_INTEGER).
Values outside the range will result in JavaScript exceptions when they're attempted to be passed/returned across the FFI.

## Callback/trait interfaces and reference cycles

Callback interfaces should avoid storing UniFFI objects, since this can create a reference cycle that will never be broken.
For example, component `A` stores reference to a callback interface `B` and `B` stores a reference to `A`.
In this case there's no way for the JavaScript garbage collector to detect the cycle, since it goes through FFI code..
This also applies to trait interfaces that have the `with_foreign` attribute.

There are several ways avoid this, listed in order of preference:

 * Prefer callback interfaces that store no properties at all.
 * Avoid properties that store Rust objects.
 * Use a [WeakRef](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/WeakRef) to store Rust objects.
 * As a last-resort, use callback interfaces that store Rust objects but make sure you have a system to manually break the reference cycles.

Note: Reference cycles can also be formed in Rust-only code, using `Arc`s so watch out for those too.
However, cycles that go across the FFI using callback interfaces are particularly hard to spot.

## Implementing trait interfaces in Rust

JavaScript callback interfaces can sometimes be avoided if UniFFI [trait interfaces](https://mozilla.github.io/uniffi-rs/latest/types/interfaces.html#exposing-traits-as-interfaces) are used.
These work similarly to callback interfaces, but can be implemented in either JavaScript or Rust.

Firefox Desktop has some support for using Rust code, for example [Rust code can access XPCOM components](https://firefox-source-docs.mozilla.org/writing-rust-code/xpcom.html.
You may be able to leverage this to avoid JavaScript altogether and implementing the trait in Firefox Desktop Rust code instead.
