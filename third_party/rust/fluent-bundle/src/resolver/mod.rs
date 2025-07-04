//! The `resolver` module contains the definitions and implementations for the internal
//! `ResolveValue` and `WriteValue` traits. The former converts AST nodes to a
//! [`FluentValue`], and the latter converts them to a string that is written to an
//! implementor of the [`std::fmt::Write`] trait.

pub mod errors;
mod expression;
mod inline_expression;
mod pattern;
mod scope;

pub use errors::ResolverError;
pub use scope::Scope;

use std::borrow::Borrow;
use std::fmt;

use crate::memoizer::MemoizerKind;
use crate::resource::FluentResource;
use crate::types::FluentValue;

/// Resolves an AST node to a [`FluentValue`].
pub(crate) trait ResolveValue<'bundle> {
    /// Resolves an AST node to a [`FluentValue`].
    fn resolve<'ast, 'args, 'errors, R, M>(
        &'ast self,
        scope: &mut Scope<'bundle, 'ast, 'args, 'errors, R, M>,
    ) -> FluentValue<'bundle>
    where
        R: Borrow<FluentResource>,
        M: MemoizerKind;
}

/// Resolves an AST node to a string that is written to source `W`.
pub(crate) trait WriteValue<'bundle> {
    /// Resolves an AST node to a string that is written to source `W`.
    fn write<'ast, 'args, 'errors, W, R, M>(
        &'ast self,
        w: &mut W,
        scope: &mut Scope<'bundle, 'ast, 'args, 'errors, R, M>,
    ) -> fmt::Result
    where
        W: fmt::Write,
        R: Borrow<FluentResource>,
        M: MemoizerKind;

    /// Writes error information to `W`. This can be used to add FTL errors inline
    /// to a message.
    fn write_error<W>(&self, _w: &mut W) -> fmt::Result
    where
        W: fmt::Write;
}
