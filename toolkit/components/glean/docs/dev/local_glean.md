# Developing with a local Glean build

FOG uses a release version of Glean, as published on [crates.io][cratesio-glean].

For local development and try runs you can replace this Glean implementation with a local or remote version.

Using a path to a local checkout of <https://github.com/mozilla/glean>:

```
./mach glean dev ../path/to/glean
```

Using a remote branch of your Glean fork:

```
./mach glean dev https://github.com/myfork/glean my-feature-branch
```

A remote `git` reference works for try runs as well,
but a `path` dependency will not.

Please ensure you do not land a non-release version of Glean.

To switch back to a normal build with a release Glean version:

```
./mach glean prod
```

You can also manually reset the changes to `Cargo.toml`, `Cargo.lock` and in `third_party/rust`.

[cratesio-glean]: https://crates.io/crates/glean
[cargo-doc]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html#specifying-dependencies-from-git-repositories
[semver]: https://semver.org/
