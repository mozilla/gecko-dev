/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A nested declarations rule.
//! https://drafts.csswg.org/css-nesting-1/#nested-declarations-rule

use crate::properties::PropertyDeclarationBlock;
use crate::shared_lock::{
    DeepCloneWithLock, Locked, SharedRwLock, SharedRwLockReadGuard, ToCssWithGuard,
};
use crate::str::CssStringWriter;
use cssparser::SourceLocation;
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps, MallocUnconditionalShallowSizeOf};
use servo_arc::Arc;

/// A nested declarations rule.
#[derive(Clone, Debug, ToShmem)]
pub struct NestedDeclarationsRule {
    /// The declarations.
    pub block: Arc<Locked<PropertyDeclarationBlock>>,
    /// The source position this rule was found at.
    pub source_location: SourceLocation,
}

impl NestedDeclarationsRule {
    /// Measure heap usage.
    pub fn size_of(&self, guard: &SharedRwLockReadGuard, ops: &mut MallocSizeOfOps) -> usize {
        self.block.unconditional_shallow_size_of(ops) + self.block.read_with(guard).size_of(ops)
    }
}

impl DeepCloneWithLock for NestedDeclarationsRule {
    fn deep_clone_with_lock(&self, lock: &SharedRwLock, guard: &SharedRwLockReadGuard) -> Self {
        Self {
            block: Arc::new(lock.wrap(self.block.read_with(&guard).clone())),
            source_location: self.source_location.clone(),
        }
    }
}

impl ToCssWithGuard for NestedDeclarationsRule {
    fn to_css(
        &self,
        guard: &SharedRwLockReadGuard,
        dest: &mut CssStringWriter,
    ) -> std::fmt::Result {
        self.block.read_with(guard).to_css(dest)
    }
}
