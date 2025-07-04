# `PinCell`

This library defines the `PinCell` type, a pinning variant of the standard
library's `RefCell`.

It is not safe to "pin project" through a `RefCell` - getting a pinned
reference to something inside the `RefCell` when you have a pinned
refernece to the `RefCell` - because `RefCell` is too powerful.

A `PinCell` is slightly less powerful than `RefCell`: unlike a `RefCell`,
one cannot get a mutable reference into a `PinCell`, only a pinned mutable
reference (`Pin<&mut T>`). This makes pin projection safe, allowing you
to use interior mutability with the knowledge that `T` will never actually
be moved out of the `RefCell` that wraps it.


## License

<sup>
Licensed under <a href="LICENSE-MIT">MIT license</a>.
</sup>
