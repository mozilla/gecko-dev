========================
How webext storage works
========================

This document describes the implementation of the the `storage.sync` part of the
`WebExtensions Storage APIs
<https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage>`_.
The implementation lives in the `toolkit/components/extensions/storage folder <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage>`_

The app-services component `lives on github <https://github.com/mozilla/application-services/blob/main/components/webext-storage>`_.
There are docs that describe `how to update/vendor this (and all) external rust code <../../../../build/buildsystem/rust.html>`_ you might be interested in.
We use UniFFI to generate JS bindings for the components. More details about UniFFI can be found in `these docs <https://searchfox.org/mozilla-central/source/docs/writing-rust-code/uniffi.md>`_.

To set the scene, let's look at the parts exposed to WebExtensions first; there are lots of
moving part there too.

WebExtension API
################

The WebExtension API is owned by the addons team. The implementation of this API is quite complex
as it involves multiple processes, but for the sake of this document, we can consider the entry-point
into the WebExtension Storage API as being `parent/ext-storage.js <https://searchfox.org/mozilla-central/source/toolkit/components/extensions/parent/ext-storage.js>`_.

Overview of the API
###################

At a high level, this API is quite simple - there are methods to "get/set/remove" extension
storage data. Note that the "external" API exposed to the addon has subtly changed the parameters
for this "internal" API, so there's an extension ID parameter and the JSON data has already been
converted to a string.
The semantics of the API are beyond this doc but are
`documented on MDN <https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/sync>`_.

As you will see in those docs, the API is promise-based, but the rust implementation is fully
synchronous and Rust knows nothing about Javascript promises - so this system converts
the callback-based API to a promise-based one.
