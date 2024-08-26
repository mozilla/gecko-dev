/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A [`@position-try`][position-try] rule for Anchor Positioning.
//!
//! [position-try]: https://drafts.csswg.org/css-anchor-position-1/#fallback-rule

use std::fmt::Write;

use crate::properties::PropertyDeclarationBlock;
use crate::shared_lock::{
    DeepCloneWithLock, Locked, SharedRwLock, SharedRwLockReadGuard, ToCssWithGuard,
};
use crate::str::CssStringWriter;
use crate::values::DashedIdent;
use cssparser::SourceLocation;
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps, MallocUnconditionalShallowSizeOf};
use servo_arc::Arc;
use style_traits::{CssWriter, ToCss};

/// A position-try rule.
#[derive(Clone, Debug, ToShmem)]
pub struct PositionTryRule {
    /// Name of this position-try rule.
    pub name: DashedIdent,
    /// The declaration block this position-try rule contains.
    pub block: Arc<Locked<PropertyDeclarationBlock>>,
    /// The source position this rule was found at.
    pub source_location: SourceLocation,
}

impl PositionTryRule {
    /// Measure heap usage.
    #[cfg(feature = "gecko")]
    pub fn size_of(&self, guard: &SharedRwLockReadGuard, ops: &mut MallocSizeOfOps) -> usize {
        self.block.unconditional_shallow_size_of(ops) + self.block.read_with(guard).size_of(ops)
    }
}

impl DeepCloneWithLock for PositionTryRule {
    fn deep_clone_with_lock(
        &self,
        lock: &SharedRwLock,
        guard: &SharedRwLockReadGuard,
    ) -> Self {
        PositionTryRule {
            name: self.name.clone(),
            block: Arc::new(lock.wrap(self.block.read_with(&guard).clone())),
            source_location: self.source_location.clone(),
        }
    }
}

impl ToCssWithGuard for PositionTryRule {
    fn to_css(
        &self,
        guard: &SharedRwLockReadGuard,
        dest: &mut CssStringWriter,
    ) -> std::fmt::Result {
        dest.write_str("@position-try ")?;
        self.name.to_css(&mut CssWriter::new(dest))?;
        dest.write_str(" { ")?;
        let declarations = self.block.read_with(guard);
        declarations.to_css(dest)?;
        if !declarations.is_empty() {
            dest.write_char(' ')?;
        }
        dest.write_char('}')
    }
}
