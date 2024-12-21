================================
How Rust Engines are implemented
================================

There are 2 main components to engines implemented in Rust

The bridged-engine
==================

Because Rust engines still need to work with the existing Sync infrastructure,
there's the concept of a `bridged-engine <https://searchfox.org/mozilla-central/source/services/sync/modules/bridged_engine.js>`_.
In short, this is just a shim between the existing
`Sync Service <https://searchfox.org/mozilla-central/source/services/sync/modules/service.js>`_
and the Rust code.

The bridge
==========

`"Golden Gate" <https://searchfox.org/mozilla-central/source/services/sync/golden_gate>`_
was previously used to help bridge any Rust implemented Sync engines with desktop,
but most of that logic has been removed. The integration of `UniFFI <https://github.com/mozilla/uniffi-rs>`_-ed components
made the Golden Gate bridge code obsolete. Currently Golden Gate contains the
logging logic for the components and the bridged engines exist in application
services within the respective sync components. For instance, these are bridged
engines for `tabs <https://github.com/mozilla/application-services/blob/main/components/tabs/src/sync/bridge.rs>`_ and
`webext-storage <https://github.com/mozilla/application-services/blob/main/components/webext-storage/src/sync/bridge.rs>`_.
