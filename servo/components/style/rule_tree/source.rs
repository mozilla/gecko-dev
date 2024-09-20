/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#![forbid(unsafe_code)]

use crate::properties::PropertyDeclarationBlock;
use crate::shared_lock::{Locked, SharedRwLockReadGuard};
use servo_arc::Arc;
use std::io::Write;
use std::ptr;

/// A style source for the rule node. It is a declaration block that may come from either a style
/// rule or a standalone block like animations / transitions / smil / preshints / style attr...
///
/// Keeping the style rule around would provide more debugability, but also causes more
/// pointer-chasing in the common code-path, which is undesired. If needed, we could keep it around
/// in debug builds or something along those lines.
#[derive(Clone, Debug)]
pub struct StyleSource(Arc<Locked<PropertyDeclarationBlock>>);

impl PartialEq for StyleSource {
    fn eq(&self, other: &Self) -> bool {
        Arc::ptr_eq(&self.0, &other.0)
    }
}

impl StyleSource {
    #[inline]
    pub(super) fn key(&self) -> ptr::NonNull<()> {
        self.0.raw_ptr()
    }

    /// Creates a StyleSource from a PropertyDeclarationBlock.
    #[inline]
    pub fn from_declarations(decls: Arc<Locked<PropertyDeclarationBlock>>) -> Self {
        Self(decls)
    }

    pub(super) fn dump<W: Write>(&self, guard: &SharedRwLockReadGuard, writer: &mut W) {
        let _ = write!(writer, "  -> {:?}", self.read(guard).declarations());
    }

    /// Read the style source guard, and obtain thus read access to the
    /// underlying property declaration block.
    #[inline]
    pub fn read<'a>(&'a self, guard: &'a SharedRwLockReadGuard) -> &'a PropertyDeclarationBlock {
        self.0.read_with(guard)
    }

    /// Returns the declaration block if applicable, otherwise None.
    #[inline]
    pub fn get(&self) -> &Arc<Locked<PropertyDeclarationBlock>> {
        &self.0
    }
}
