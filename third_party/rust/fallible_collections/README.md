Fallible Collections.rs
==============

Implements APIs on Rust collections wich gracefully return a `Result` when an allocation error occurs.
This is inspired a lot by [RFC 2116](https://github.com/rust-lang/rfcs/blob/master/text/2116-alloc-me-maybe.md).

There are APIs for a fallible interface for `Vec`, `Box`, `BTree`, `HashMap`,
and a `TryClone` trait wich is implemented for primitive Rust traits and a fallible format macro.
You can use this with `try_clone_derive` crate wich derive `TryClone` for your own types.

# Getting Started

[`fallible_collections` is available on crates.io](https://crates.io/crates/fallible_collections).
It is recommended to look there for the newest released version, as well as links to the newest builds of the docs.

Add the following dependency to your Cargo manifest:

```toml
[dependencies]
fallible_collections = "0.5"

# or
fallible_collections = { version = "0.5", features = ["std"] }
```

...and see the [docs](https://docs.rs/fallible_collections) for how to use it.

# Example

Exemple of using the `FallibleBox` interface.

```rust
use fallible_collections::FallibleBox;

fn main() {
	// this crate an Ordinary box but return an error on allocation failure
	let mut a = <Box<_> as FallibleBox<_>>::try_new(5).unwrap();
	let mut b = Box::new(5);

	assert_eq!(a, b);
	*a = 3;
	assert_eq!(*a, 3);
}
```

Exemple of using the `FallibleVec` interface.

```rust
use fallible_collections::FallibleVec;

fn main() {
	// this crate an Ordinary Vec<Vec<u8>> but return an error on allocation failure
	let a: Vec<Vec<u8>> = try_vec![try_vec![42; 10].unwrap(); 100].unwrap();
	let b: Vec<Vec<u8>> = vec![vec![42; 10]; 100];
	assert_eq!(a, b);
	assert_eq!(a.try_clone().unwrap(), a);
	...
}
```

## License

Licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.

