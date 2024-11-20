/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A [`@scope`][scope] rule.
//!
//! [scope]: https://drafts.csswg.org/css-cascade-6/#scoped-styles

use crate::applicable_declarations::ScopeProximity;
use crate::dom::TElement;
use crate::parser::ParserContext;
use crate::selector_parser::{SelectorImpl, SelectorParser};
use crate::shared_lock::{
    DeepCloneWithLock, Locked, SharedRwLock, SharedRwLockReadGuard, ToCssWithGuard,
};
use crate::str::CssStringWriter;
use crate::stylesheets::CssRules;
use crate::simple_buckets_map::SimpleBucketsMap;
use cssparser::{Parser, SourceLocation, ToCss};
#[cfg(feature = "gecko")]
use malloc_size_of::{
    MallocSizeOfOps, MallocUnconditionalShallowSizeOf, MallocUnconditionalSizeOf,
};
use selectors::context::{MatchingContext, QuirksMode};
use selectors::matching::matches_selector;
use selectors::parser::{Component, ParseRelative, Selector, SelectorList};
use selectors::OpaqueElement;
use servo_arc::Arc;
use std::fmt::{self, Write};
use style_traits::{CssWriter, ParseError};

/// A scoped rule.
#[derive(Debug, ToShmem)]
pub struct ScopeRule {
    /// Bounds at which this rule applies.
    pub bounds: ScopeBounds,
    /// The nested rules inside the block.
    pub rules: Arc<Locked<CssRules>>,
    /// The source position where this rule was found.
    pub source_location: SourceLocation,
}

impl DeepCloneWithLock for ScopeRule {
    fn deep_clone_with_lock(
        &self,
        lock: &SharedRwLock,
        guard: &SharedRwLockReadGuard,
    ) -> Self {
        let rules = self.rules.read_with(guard);
        Self {
            bounds: self.bounds.clone(),
            rules: Arc::new(lock.wrap(rules.deep_clone_with_lock(lock, guard))),
            source_location: self.source_location.clone(),
        }
    }
}

impl ToCssWithGuard for ScopeRule {
    fn to_css(&self, guard: &SharedRwLockReadGuard, dest: &mut CssStringWriter) -> fmt::Result {
        dest.write_str("@scope")?;
        {
            let mut writer = CssWriter::new(dest);
            if let Some(start) = self.bounds.start.as_ref() {
                writer.write_str(" (")?;
                start.to_css(&mut writer)?;
                writer.write_char(')')?;
            }
            if let Some(end) = self.bounds.end.as_ref() {
                writer.write_str(" to (")?;
                end.to_css(&mut writer)?;
                writer.write_char(')')?;
            }
        }
        self.rules.read_with(guard).to_css_block(guard, dest)
    }
}

impl ScopeRule {
    /// Measure heap usage.
    #[cfg(feature = "gecko")]
    pub fn size_of(&self, guard: &SharedRwLockReadGuard, ops: &mut MallocSizeOfOps) -> usize {
        self.rules.unconditional_shallow_size_of(ops) +
            self.rules.read_with(guard).size_of(guard, ops) +
            self.bounds.size_of(ops)
    }
}

/// Bounds of the scope.
#[derive(Debug, Clone, ToShmem)]
pub struct ScopeBounds {
    /// Start of the scope.
    pub start: Option<SelectorList<SelectorImpl>>,
    /// End of the scope.
    pub end: Option<SelectorList<SelectorImpl>>,
}

impl ScopeBounds {
    #[cfg(feature = "gecko")]
    fn size_of(&self, ops: &mut MallocSizeOfOps) -> usize {
        fn bound_size_of(
            bound: &Option<SelectorList<SelectorImpl>>,
            ops: &mut MallocSizeOfOps,
        ) -> usize {
            bound
                .as_ref()
                .map(|list| list.unconditional_size_of(ops))
                .unwrap_or(0)
        }
        bound_size_of(&self.start, ops) + bound_size_of(&self.end, ops)
    }
}

fn parse_scope<'a>(
    context: &ParserContext,
    input: &mut Parser<'a, '_>,
    parse_relative: ParseRelative,
    for_end: bool,
) -> Result<Option<SelectorList<SelectorImpl>>, ParseError<'a>> {
    input
        .try_parse(|input| {
            if for_end {
                // scope-end not existing is valid.
                if input.try_parse(|i| i.expect_ident_matching("to")).is_err() {
                    return Ok(None);
                }
            }
            let parens = input.try_parse(|i| i.expect_parenthesis_block());
            if for_end {
                // `@scope to {}` is NOT valid.
                parens?;
            } else if parens.is_err() {
                // `@scope {}` is valid.
                return Ok(None);
            }
            input.parse_nested_block(|input| {
                if input.is_exhausted() {
                    // `@scope () {}` is valid.
                    return Ok(None);
                }
                let selector_parser = SelectorParser {
                    stylesheet_origin: context.stylesheet_origin,
                    namespaces: &context.namespaces,
                    url_data: context.url_data,
                    for_supports_rule: false,
                };
                let parse_relative = if for_end {
                    ParseRelative::ForScope
                } else {
                    parse_relative
                };
                Ok(Some(SelectorList::parse_disallow_pseudo(
                    &selector_parser,
                    input,
                    parse_relative,
                )?))
            })
        })
}

impl ScopeBounds {
    /// Parse a container condition.
    pub fn parse<'a>(
        context: &ParserContext,
        input: &mut Parser<'a, '_>,
        parse_relative: ParseRelative,
    ) -> Result<Self, ParseError<'a>> {
        let start = parse_scope(context, input, parse_relative, false)?;
        let end = parse_scope(context, input, parse_relative, true)?;
        Ok(Self { start, end })
    }
}

/// Types of implicit scope root.
#[derive(Debug, Copy, Clone, MallocSizeOf)]
pub enum ImplicitScopeRoot {
    /// This implicit scope root is in the light tree.
    InLightTree(OpaqueElement),
    /// This implicit scope root is the document element, regardless of which (light|shadow) tree
    /// the element being matched is. This is the case for e.g. if you specified an implicit scope
    /// within a user stylesheet.
    DocumentElement,
    /// The implicit scope root is in a constructed stylesheet - the scope root the element
    /// under consideration's shadow root (If one exists).
    Constructed,
    /// This implicit scope root is in the shadow tree.
    InShadowTree(OpaqueElement),
    /// This implicit scope root is the shadow host of the stylesheet-containing shadow tree.
    ShadowHost(OpaqueElement),
}

impl ImplicitScopeRoot {
    /// Return true if this matches the shadow host.
    pub fn matches_shadow_host(&self) -> bool {
        match self {
            Self::InLightTree(..) | Self::InShadowTree(..) | Self::DocumentElement => false,
            Self::ShadowHost(..) | Self::Constructed => true,
        }
    }

    /// Return the implicit scope root element.
    pub fn element(&self, current_host: Option<OpaqueElement>) -> ImplicitScopeTarget {
        match self {
            Self::InLightTree(e) | Self::InShadowTree(e) | Self::ShadowHost(e) => {
                ImplicitScopeTarget::Element(*e)
            },
            Self::Constructed | Self::DocumentElement => {
                if matches!(self, Self::Constructed) {
                    if let Some(host) = current_host {
                        return ImplicitScopeTarget::Element(host);
                    }
                }
                ImplicitScopeTarget::DocumentElement
            },
        }
    }
}

/// Target of this implicit scope.
pub enum ImplicitScopeTarget {
    /// Target matches only the specified element.
    Element(OpaqueElement),
    /// Implicit scope whose target is the document element.
    DocumentElement,
}

impl ImplicitScopeTarget {
    /// Check if this element is the implicit scope.
    fn check<E: TElement>(&self, element: E) -> bool {
        match self {
            Self::Element(e) => element.opaque() == *e,
            Self::DocumentElement => element.is_root(),
        }
    }
}

/// Target of this scope.
pub enum ScopeTarget<'a> {
    /// Target matches an element matching the specified selector list.
    Selector(&'a SelectorList<SelectorImpl>),
    /// Target matches an implicit scope target.
    Implicit(ImplicitScopeTarget),
}

impl<'a> ScopeTarget<'a> {
    /// Check if the given element is the scope.
    fn check<E: TElement>(
        &self,
        element: E,
        scope: Option<OpaqueElement>,
        scope_subject_map: &ScopeSubjectMap,
        context: &mut MatchingContext<E::Impl>,
    ) -> bool {
        match self {
            Self::Selector(list) => context.nest_for_scope_condition(scope, |context| {
                if scope_subject_map.early_reject(element, context.quirks_mode()) {
                    return false;
                }
                for selector in list.slice().iter() {
                    if matches_selector(selector, 0, None, &element, context) {
                        return true;
                    }
                }
                false
            }),
            Self::Implicit(t) => t.check(element),
        }
    }
}

/// A scope root candidate.
#[derive(Clone, Copy, Debug)]
pub struct ScopeRootCandidate {
    /// This candidate's scope root.
    pub root: OpaqueElement,
    /// Ancestor hop from the element under consideration to this scope root.
    pub proximity: ScopeProximity,
}

/// Collect potential scope roots for a given element and its scope target.
/// The check may not pass the ceiling, if specified.
pub fn collect_scope_roots<E>(
    element: E,
    ceiling: Option<OpaqueElement>,
    context: &mut MatchingContext<E::Impl>,
    target: &ScopeTarget,
    matches_shadow_host: bool,
    scope_subject_map: &ScopeSubjectMap,
) -> Vec<ScopeRootCandidate>
where
    E: TElement,
{
    let mut result = vec![];
    let mut parent = Some(element);
    let mut proximity = 0usize;
    while let Some(p) = parent {
        if ceiling == Some(p.opaque()) {
            break;
        }
        if target.check(p, ceiling, scope_subject_map, context) {
            result.push(ScopeRootCandidate {
                root: p.opaque(),
                proximity: ScopeProximity::new(proximity),
            });
            // Note that we can't really break here - we need to consider
            // ALL scope roots to figure out whch one didn't end.
        }
        parent = p.parent_element();
        proximity += 1;
        // We we got to the top of the shadow tree - keep going
        // if we may match the shadow host.
        if parent.is_none() && matches_shadow_host {
            parent = p.containing_shadow_host();
        }
    }
    result
}

/// Given the scope-end selector, check if the element is outside of the scope.
/// That is, check if any ancestor to the root matches the scope-end selector.
pub fn element_is_outside_of_scope<E>(
    selector: &Selector<E::Impl>,
    element: E,
    root: OpaqueElement,
    context: &mut MatchingContext<E::Impl>,
    root_may_be_shadow_host: bool,
) -> bool
where
    E: TElement,
{
    let mut parent = Some(element);
    context.nest_for_scope_condition(Some(root), |context| {
        while let Some(p) = parent {
            if matches_selector(selector, 0, None, &p, context) {
                return true;
            }
            if p.opaque() == root {
                // Reached the top, not lying outside of scope.
                break;
            }
            parent = p.parent_element();
            if parent.is_none() && root_may_be_shadow_host {
                if let Some(host) = p.containing_shadow_host() {
                    // Pretty much an edge case where user specified scope-start and -end of :host
                    return host.opaque() == root;
                }
            }
        }
        return false;
    })
}

/// A map containing simple selectors in subjects of scope selectors.
/// This allows fast-rejecting scopes before running the full match.
#[derive(Clone, Debug, Default, MallocSizeOf)]
pub struct ScopeSubjectMap {
    buckets: SimpleBucketsMap<()>,
    any: bool,
}

impl ScopeSubjectMap {
    /// Add the `<scope-start>` of a scope.
    pub fn add_bound_start(&mut self, selectors: &SelectorList<SelectorImpl>, quirks_mode: QuirksMode) {
        if self.add_selector_list(selectors, quirks_mode) {
            self.any = true;
        }
    }

    fn add_selector_list(&mut self, selectors: &SelectorList<SelectorImpl>, quirks_mode: QuirksMode) -> bool {
        let mut is_any = false;
        for selector in selectors.slice().iter() {
            is_any = is_any || self.add_selector(selector, quirks_mode);
        }
        is_any
    }

    fn add_selector(&mut self, selector: &Selector<SelectorImpl>, quirks_mode: QuirksMode) -> bool {
        let mut is_any = true;
        let mut iter = selector.iter();
        while let Some(c) = iter.next() {
            let component_any = match c {
                Component::Class(cls) => {
                    match self.buckets.classes.try_entry(cls.0.clone(), quirks_mode) {
                        Ok(e) => {
                            e.or_insert(());
                            false
                        },
                        Err(_) => true,
                    }
                },
                Component::ID(id) => {
                    match self.buckets.ids.try_entry(id.0.clone(), quirks_mode) {
                        Ok(e) => {
                            e.or_insert(());
                            false
                        },
                        Err(_) => true,
                    }
                },
                Component::LocalName(local_name) => {
                    self.buckets.local_names.insert(local_name.lower_name.clone(), ());
                    false
                },
                Component::Is(ref list) | Component::Where(ref list) => {
                    self.add_selector_list(list, quirks_mode)
                },
                _ => true,
            };

            is_any = is_any && component_any;
        }
        is_any
    }

    /// Shrink the map as much as possible.
    pub fn shrink_if_needed(&mut self) {
        self.buckets.shrink_if_needed();
    }

    /// Clear the map.
    pub fn clear(&mut self) {
        self.buckets.clear();
        self.any = false;
    }

    /// Could a given element possibly be a scope root?
    fn early_reject<E: TElement>(&self, element: E, quirks_mode: QuirksMode) -> bool {
        if self.any {
            return false;
        }

        if let Some(id) = element.id() {
            if self.buckets.ids.get(id, quirks_mode).is_some() {
                return false;
            }
        }

        let mut found = false;
        element.each_class(|cls| {
            if self.buckets.classes.get(cls, quirks_mode).is_some() {
                found = true;
            }
        });
        if found {
            return false;
        }

        if self.buckets.local_names.get(element.local_name()).is_some() {
            return false;
        }

        true
    }
}

/// Determine if this selector list, when used as a scope bound selector, is considered trivial.
pub fn scope_selector_list_is_trivial(list: &SelectorList<SelectorImpl>) -> bool {
    fn scope_selector_is_trivial(selector: &Selector<SelectorImpl>) -> bool {
        // A selector is trivial if:
        // * There is no selector conditional on its siblings and/or descendant to match, and
        // * There is no dependency on sibling relations, and
        // * There's no ID selector in the selector. A more correct approach may be to ensure that
        //   scoping roots of the style sharing candidates and targets have matching IDs, but that
        //   requires re-plumbing what we pass around for scope roots.
        let mut iter = selector.iter();
        loop {
            while let Some(c) = iter.next() {
                match c {
                    Component::ID(_) | Component::Nth(_) | Component::NthOf(_) | Component::Has(_) => return false,
                    Component::Is(ref list) | Component::Where(ref list) | Component::Negation(ref list) =>
                        if !scope_selector_list_is_trivial(list) {
                            return false;
                        }
                    _ => (),
                }
            }

            match iter.next_sequence() {
                Some(c) => if c.is_sibling() {
                    return false;
                },
                None => return true,
            }
        }
    }

    list.slice().iter().all(|s| scope_selector_is_trivial(s))
}
