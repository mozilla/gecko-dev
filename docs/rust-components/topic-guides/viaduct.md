# Viaduct

`Viaduct` initialization is required for all platforms and for multiple components. The [README](https://github.com/mozilla/application-services/blob/main/components/viaduct/README.md) explains the component in more detail.

Firefox Desktop, Firefox Android, and Firefox iOS all already initialize viaduct.  You only need to
take action if you want to use the Rust components on a new application.

There are 3 different options to use `viaduct`:

* Any `libxul` based can ignore initialization, since it's handled by `libxul`.
* Using the reqwest backend, which uses the `reqwest` library and a `reqwest`-managed thread.
* Implementing the C FFI like `libxul` does (https://searchfox.org/mozilla-central/source/toolkit/components/viaduct).
