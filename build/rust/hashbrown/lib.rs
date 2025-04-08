/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
pub use hashbrown::*;

// `hashbrown` 0.15.0 moved `DefaultHashBuilder` from the `hash_map` module to the root, so let's
// "undo" that.
pub mod hash_map {
    pub use hashbrown::hash_map::*;
    pub use hashbrown::DefaultHashBuilder;
}
