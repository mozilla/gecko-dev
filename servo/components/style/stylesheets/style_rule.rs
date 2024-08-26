/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A style rule.

use crate::properties::PropertyDeclarationBlock;
use crate::selector_parser::SelectorImpl;
use crate::shared_lock::{
    DeepCloneWithLock, Locked, SharedRwLock, SharedRwLockReadGuard, ToCssWithGuard,
};
use crate::str::CssStringWriter;
use crate::stylesheets::{CssRules, style_or_page_rule_to_css};
use cssparser::SourceLocation;
#[cfg(feature = "gecko")]
use malloc_size_of::{
    MallocSizeOf, MallocSizeOfOps, MallocUnconditionalShallowSizeOf, MallocUnconditionalSizeOf,
};
use selectors::SelectorList;
use servo_arc::Arc;
use std::fmt::{self, Write};

/// A style rule, with selectors and declarations.
#[derive(Debug, ToShmem)]
pub struct StyleRule {
    /// The list of selectors in this rule.
    pub selectors: SelectorList<SelectorImpl>,
    /// The declaration block with the properties it contains.
    pub block: Arc<Locked<PropertyDeclarationBlock>>,
    /// The nested rules to this style rule. Only non-`None` when nesting is enabled.
    pub rules: Option<Arc<Locked<CssRules>>>,
    /// The location in the sheet where it was found.
    pub source_location: SourceLocation,
}

impl DeepCloneWithLock for StyleRule {
    /// Deep clones this StyleRule.
    fn deep_clone_with_lock(
        &self,
        lock: &SharedRwLock,
        guard: &SharedRwLockReadGuard,
    ) -> StyleRule {
        StyleRule {
            selectors: self.selectors.clone(),
            block: Arc::new(lock.wrap(self.block.read_with(guard).clone())),
            rules: self.rules.as_ref().map(|rules| {
                let rules = rules.read_with(guard);
                Arc::new(lock.wrap(rules.deep_clone_with_lock(lock, guard)))
            }),
            source_location: self.source_location.clone(),
        }
    }
}

impl StyleRule {
    /// Measure heap usage.
    #[cfg(feature = "gecko")]
    pub fn size_of(&self, guard: &SharedRwLockReadGuard, ops: &mut MallocSizeOfOps) -> usize {
        let mut n = 0;
        n += self.selectors.unconditional_size_of(ops);
        n += self.block.unconditional_shallow_size_of(ops) +
            self.block.read_with(guard).size_of(ops);
        if let Some(ref rules) = self.rules {
            n += rules.unconditional_shallow_size_of(ops) +
                rules.read_with(guard).size_of(guard, ops)
        }
        n
    }
}

impl ToCssWithGuard for StyleRule {
    /// https://drafts.csswg.org/cssom/#serialize-a-css-rule CSSStyleRule
    fn to_css(&self, guard: &SharedRwLockReadGuard, dest: &mut CssStringWriter) -> fmt::Result {
        use cssparser::ToCss;
        self.selectors.to_css(dest)?;
        dest.write_char(' ')?;
        style_or_page_rule_to_css(self.rules.as_ref(), &self.block, guard, dest)
    }
}
