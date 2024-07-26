# Developing with a local Glean build

FOG uses a release version of Glean, as published on [crates.io][cratesio-glean].

For local development and try runs you can replace this Glean implementation with a local or remote version.

1. To tell `mach` where to find your Glean, patch the top-level {searchfox}`Cargo.toml`
   to point at your Glean fork:

    ```toml
    [patch.crates-io]
    glean = { git = "https://github.com/myfork/glean", branch = "my-feature-branch" }
    glean-core = { git = "https://github.com/myfork/glean", branch = "my-feature-branch" }
    ```

    Both crates need to be specified to ensure they are in sync.

    You can specify the exact code to use by `branch`, `tag` or `rev` (Git commit).
    See the [cargo documentation for details][cargo-doc].

    You can also use a path dependency, for a truly local Glean:

    ```toml
    [patch.crates-io]
    glean = { path = "../glean/glean-core/rlb" }
    glean-core = { path = "../glean/glean-core" }
    ```

2. If the version of the Glean SDK in the patched repository is not
   [semver]-compatible with the versions required by the workspaces within mozilla-central,
   trying to update the Cargo lockfile at this time will result in an error like

   ```
   warning: Patch `glean vMM.m.p (/path/to/glean/glean-core/rlb)` was not used in the crate graph.
   ```

   In this case, change the `glean` crate version in both {searchfox}`Cargo.toml`
   and {searchfox}`gfx/wr/Cargo.toml` to match the version in your `glean` repo:

   ```toml
   glean = "=MM.m.p"
   ```

3. Update the Cargo lockfile for the root workspace:

    ```
    cargo update -p glean
    ```

   (You'd think that, if you had to change {searchfox}`gfx/wr/Cargo.toml` in step 2,
   you'd have to `cargo update` in `gfx/wr` too, but you'd be wrong.
   It's not needed, and if you try it'll just error about `crates.io`
   not knowing about your local version of Glean.)

4.  Mozilla's supply-chain management policy requires that third-party software
    (which includes the Glean SDK because it is distributed as though it is third-party)
    be audited and certified as safe.
    Your local Glean SDK is not coming from a source that has been vetted.
    If you try to vendor right now, `./mach vendor rust` may complain something like:

    ```
    Vet error: There are some issues with your policy.audit-as-crates-io entries
    ```

    To allow your local Glean crates to be treated as crates.io-sourced crates for vetting,
    add the following sections to the top of `supply-chain/config.toml`:

    ```toml
    [policy.glean]
    audit-as-crates-io = true

    [policy.glean-core]
    audit-as-crates-io = true
    ```

    **Note:** Do not attempt to check these changes in.
    `@supply-chain-reviewers` may become cross as they `r-` your patch.

5. Vendor the changed crates:

    ```
    ./mach vendor rust
    ```

    **Note:** If you're using a path dependency, `mach vendor rust` doesn't actually change files.
    Instead it pulls the files directly from the location on disk you specify.

6. Finally, build Firefox:

    ```
    ./mach build
    ```

A remote `git` reference works for try runs as well,
but a `path` dependency will not.

Please ensure you do not land a non-release version of Glean.

[cratesio-glean]: https://crates.io/crates/glean
[cargo-doc]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html#specifying-dependencies-from-git-repositories
[semver]: https://semver.org/
