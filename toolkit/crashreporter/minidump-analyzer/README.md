# Testing
To run tests, comment out the `mozilla-central-workspace-hack` dependency, then run `cargo test`.
Otherwise, the dependency won't have the feature we need enabled. This is because the crate is
excluded from the workspace (as we don't want to vendor the dev-dependencies).

The `Cargo.lock` in this directory corresponds to this configuration.
