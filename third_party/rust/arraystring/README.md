# ArrayString

Fixed capacity stack based generic string

Since rust doesn't have constant generics yet `typenum` is used to allow for generic arrays (U1 to U255)

Can't outgrow initial capacity (defined at compile time), always occupies `capacity` `+ 1` bytes of memory

*Doesn't allocate memory on the heap and never panics in release (all panic branches are stripped at compile time - except `Index`/`IndexMut` traits, since they are supposed to)*

* [Documentation](https://docs.rs/arraystring/0.3.0/arraystring)

## Why

Data is generally bounded, you don't want a phone number with 30 characters, nor a username with 100. You probably don't even support it in your database.

Why pay the cost of heap allocations of strings with unlimited capacity if you have limited boundaries?

Stack based strings are generally faster to create, clone and append to than heap based strings (custom allocators and thread-locals may help with heap based ones).

But that becomes less true as you increase the array size, `CacheString` occuppies a full cache line, 255 bytes is the maximum we accept - `MaxString` (bigger will just wrap) and it's probably already slower than heap based strings of that size (like in `std::string::String`)

There are other stack based strings out there, they generally can have "unlimited" capacity (heap allocate), but the stack based size is defined by the library implementor, we go through a different route by implementing a string based in a generic array.

Array based strings always occupies the full space in memory, so they may use more memory (in the stack) than dynamic strings.

## Features

 **default:** `std`

 - `std` enabled by default, enables `std` compatibility - `impl Error` trait for errors (remove it to be `#[no_std]` compatible)
 - `serde-traits` enables serde traits integration (`Serialize`/`Deserialize`)

     Opperates like `String`, but truncates it if it's bigger than capacity

 - `diesel-traits` enables diesel traits integration (`Insertable`/`Queryable`)

     Opperates like `String`, but truncates it if it's bigger than capacity

 - `logs` enables internal logging

     You will probably only need this if you are debugging this library

 ## Examples

```rust
use arraystring::{Error, ArrayString, typenum::U5, typenum::U20};

type Username = ArrayString<U20>;
type Role = ArrayString<U5>;

#[derive(Debug)]
pub struct User {
    pub username: Username,
    pub role: Role,
}

fn main() -> Result<(), Error> {
    let user = User {
        username: Username::try_from_str("user")?,
        role: Role::try_from_str("admin")?
    };
    println!("{:?}", user);

    Ok(())
}
```

 ## Comparisons

*These benchmarks ran while I streamed video and used my computer (with* **non-disclosed specs**) *as usual, so don't take the actual times too seriously, just focus on the comparison*

```my_custom_benchmark
small-string  (23 bytes)      clone                  4.837 ns
small-string  (23 bytes)      try_from_str          14.777 ns
small-string  (23 bytes)      from_str_truncate     11.360 ns
small-string  (23 bytes)      from_str_unchecked    11.291 ns
small-string  (23 bytes)      try_push_str           1.162 ns
small-string  (23 bytes)      push_str               3.490 ns
small-string  (23 bytes)      push_str_unchecked     1.098 ns
-------------------------------------------------------------
cache-string  (63 bytes)      clone                 10.170 ns
cache-string  (63 bytes)      try_from_str          25.579 ns
cache-string  (63 bytes)      from_str_truncate     16.977 ns
cache-string  (63 bytes)      from_str_unchecked    17.201 ns
cache-string  (63 bytes)      try_push_str           1.160 ns
cache-string  (63 bytes)      push_str               3.486 ns
cache-string  (63 bytes)      push_str_unchecked     1.115 ns
-------------------------------------------------------------
max-string   (255 bytes)      clone                147.410 ns
max-string   (255 bytes)      try_from_str         157.340 ns
max-string   (255 bytes)      from_str_truncate    158.000 ns
max-string   (255 bytes)      from_str_unchecked   158.420 ns
max-string   (255 bytes)      try_push_str           1.167 ns
max-string   (255 bytes)      push_str               4.337 ns
max-string   (255 bytes)      push_str_unchecked     1.103 ns
-------------------------------------------------------------
string (19 bytes)             clone                 33.295 ns
string (19 bytes)             from                  32.512 ns
string (19 bytes)             push str              28.128 ns
-------------------------------------------------------------
inlinable-string (30 bytes)   clone                 16.751 ns
inlinable-string (30 bytes)   from_str              29.310 ns
inlinable-string (30 bytes)   push_str               2.865 ns
-------------------------------------------------------------
smallstring crate (20 bytes)  clone                 60.988 ns
smallstring crate (20 bytes)  from_str              50.233 ns
```

## Licenses

[MIT](master/license/MIT) and [Apache-2.0](master/license/APACHE)
