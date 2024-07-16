/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::{Atom, LocalName, ShrinkIfNeeded};
use crate::selector_map::{MaybeCaseInsensitiveHashMap, PrecomputedHashMap};

/// A map for filtering by easily-discernable features in a selector.
#[derive(Clone, Debug, MallocSizeOf)]
pub struct SimpleBucketsMap<T> {
    pub classes: MaybeCaseInsensitiveHashMap<Atom, T>,
    pub ids: MaybeCaseInsensitiveHashMap<Atom, T>,
    pub local_names: PrecomputedHashMap<LocalName, T>,
}

impl<T> Default for SimpleBucketsMap<T> {
    fn default() -> Self {
        // TODO(dshin): This is a bit annoying - even if these maps would be empty,
        // deriving this trait requires `T: Default`
        // This is a known issue - See https://github.com/rust-lang/rust/issues/26925.
        Self {
            classes: Default::default(),
            ids: Default::default(),
            local_names: Default::default(),
        }
    }
}

impl<T> SimpleBucketsMap<T> {
    /// Clears the map.
    #[inline(always)]
    pub fn clear(&mut self) {
        self.classes.clear();
        self.ids.clear();
        self.local_names.clear();
    }

    /// Shrink the capacity of the map if needed.
    #[inline(always)]
    pub fn shrink_if_needed(&mut self) {
        self.classes.shrink_if_needed();
        self.ids.shrink_if_needed();
        self.local_names.shrink_if_needed();
    }

    /// Returns whether there's nothing in the map.
    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.classes.is_empty() &&
            self.ids.is_empty() &&
            self.local_names.is_empty()
    }
}
