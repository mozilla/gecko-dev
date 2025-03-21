/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! The [`@property`] at-rule.
//!
//! https://drafts.css-houdini.org/css-properties-values-api-1/#at-property-rule

use super::{
    registry::{PropertyRegistration, PropertyRegistrationData},
    syntax::Descriptor,
    value::{
        AllowComputationallyDependent, ComputedValue as ComputedRegisteredValue,
        SpecifiedValue as SpecifiedRegisteredValue,
    },
};
use crate::custom_properties::{Name as CustomPropertyName, SpecifiedValue};
use crate::error_reporting::ContextualParseError;
use crate::parser::{Parse, ParserContext};
use crate::shared_lock::{SharedRwLockReadGuard, ToCssWithGuard};
use crate::str::CssStringWriter;
use crate::values::{computed, serialize_atom_name};
use cssparser::{
    AtRuleParser, BasicParseErrorKind, CowRcStr, DeclarationParser, ParseErrorKind, Parser,
    ParserInput, ParserState, QualifiedRuleParser, RuleBodyItemParser, RuleBodyParser, SourceLocation,
};
#[cfg(feature = "gecko")] use malloc_size_of::{MallocSizeOf, MallocSizeOfOps};
use selectors::parser::SelectorParseErrorKind;
use servo_arc::Arc;
use std::fmt::{self, Write};
use style_traits::{
    CssWriter, ParseError, PropertyInheritsParseError, PropertySyntaxParseError,
    StyleParseErrorKind, ToCss,
};
use to_shmem::{SharedMemoryBuilder, ToShmem};

/// Parse the block inside a `@property` rule.
///
/// Valid `@property` rules result in a registered custom property, as if `registerProperty()` had
/// been called with equivalent parameters.
pub fn parse_property_block<'i, 't>(
    context: &ParserContext,
    input: &mut Parser<'i, 't>,
    name: PropertyRuleName,
    source_location: SourceLocation,
) -> Result<PropertyRegistration, ParseError<'i>> {
    let mut descriptors = PropertyDescriptors::default();
    let mut parser = PropertyRuleParser {
        context,
        descriptors: &mut descriptors,
    };
    let mut iter = RuleBodyParser::new(input, &mut parser);
    let mut syntax_err = None;
    let mut inherits_err = None;
    while let Some(declaration) = iter.next() {
        if !context.error_reporting_enabled() {
            continue;
        }
        if let Err((error, slice)) = declaration {
            let location = error.location;
            let error = match error.kind {
                // If the provided string is not a valid syntax string (if it
                // returns failure when consume a syntax definition is called on
                // it), the descriptor is invalid and must be ignored.
                ParseErrorKind::Custom(StyleParseErrorKind::PropertySyntaxField(_)) => {
                    syntax_err = Some(error.clone());
                    ContextualParseError::UnsupportedValue(slice, error)
                },

                // If the provided string is not a valid inherits string,
                // the descriptor is invalid and must be ignored.
                ParseErrorKind::Custom(StyleParseErrorKind::PropertyInheritsField(_)) => {
                    inherits_err = Some(error.clone());
                    ContextualParseError::UnsupportedValue(slice, error)
                },

                // Unknown descriptors are invalid and ignored, but do not
                // invalidate the @property rule.
                _ => ContextualParseError::UnsupportedPropertyDescriptor(slice, error),
            };
            context.log_css_error(location, error);
        }
    }

    // https://drafts.css-houdini.org/css-properties-values-api-1/#the-syntax-descriptor:
    //
    //     The syntax descriptor is required for the @property rule to be valid; if it’s
    //     missing, the @property rule is invalid.
    let Some(syntax) = descriptors.syntax else {
        return Err(if let Some(err) = syntax_err {
            err
        } else {
            let err = input.new_custom_error(StyleParseErrorKind::PropertySyntaxField(
                PropertySyntaxParseError::NoSyntax,
            ));
            context.log_css_error(
                source_location,
                ContextualParseError::UnsupportedValue("", err.clone()),
            );
            err
        });
    };

    // https://drafts.css-houdini.org/css-properties-values-api-1/#inherits-descriptor:
    //
    //     The inherits descriptor is required for the @property rule to be valid; if it’s
    //     missing, the @property rule is invalid.
    let Some(inherits) = descriptors.inherits else {
        return Err(if let Some(err) = inherits_err {
            err
        } else {
            let err = input.new_custom_error(StyleParseErrorKind::PropertyInheritsField(
                PropertyInheritsParseError::NoInherits,
            ));
            context.log_css_error(
                source_location,
                ContextualParseError::UnsupportedValue("", err.clone()),
            );
            err
        });
    };

    if PropertyRegistration::validate_initial_value(&syntax, descriptors.initial_value.as_deref())
        .is_err()
    {
        return Err(input.new_error(BasicParseErrorKind::AtRuleBodyInvalid));
    }

    Ok(PropertyRegistration {
        name,
        data: PropertyRegistrationData {
            syntax,
            inherits,
            initial_value: descriptors.initial_value,
        },
        url_data: context.url_data.clone(),
        source_location,
    })
}

struct PropertyRuleParser<'a, 'b: 'a> {
    context: &'a ParserContext<'b>,
    descriptors: &'a mut PropertyDescriptors,
}

/// Default methods reject all at rules.
impl<'a, 'b, 'i> AtRuleParser<'i> for PropertyRuleParser<'a, 'b> {
    type Prelude = ();
    type AtRule = ();
    type Error = StyleParseErrorKind<'i>;
}

impl<'a, 'b, 'i> QualifiedRuleParser<'i> for PropertyRuleParser<'a, 'b> {
    type Prelude = ();
    type QualifiedRule = ();
    type Error = StyleParseErrorKind<'i>;
}

impl<'a, 'b, 'i> RuleBodyItemParser<'i, (), StyleParseErrorKind<'i>>
    for PropertyRuleParser<'a, 'b>
{
    fn parse_qualified(&self) -> bool {
        false
    }
    fn parse_declarations(&self) -> bool {
        true
    }
}

macro_rules! property_descriptors {
    (
        $( #[$doc: meta] $name: tt $ident: ident: $ty: ty, )*
    ) => {
        /// Data inside a `@property` rule.
        ///
        /// <https://drafts.css-houdini.org/css-properties-values-api-1/#at-property-rule>
        #[derive(Clone, Debug, Default, PartialEq)]
        struct PropertyDescriptors {
            $(
                #[$doc]
                $ident: Option<$ty>,
            )*
        }

        impl PropertyRegistration {
            fn decl_to_css(&self, dest: &mut CssStringWriter) -> fmt::Result {
                $(
                    let $ident = Option::<&$ty>::from(&self.data.$ident);
                    if let Some(ref value) = $ident {
                        dest.write_str(concat!($name, ": "))?;
                        value.to_css(&mut CssWriter::new(dest))?;
                        dest.write_str("; ")?;
                    }
                )*
                Ok(())
            }
        }

        impl<'a, 'b, 'i> DeclarationParser<'i> for PropertyRuleParser<'a, 'b> {
            type Declaration = ();
            type Error = StyleParseErrorKind<'i>;

            fn parse_value<'t>(
                &mut self,
                name: CowRcStr<'i>,
                input: &mut Parser<'i, 't>,
                _declaration_start: &ParserState,
            ) -> Result<(), ParseError<'i>> {
                match_ignore_ascii_case! { &*name,
                    $(
                        $name => {
                            // DeclarationParser also calls parse_entirely so we’d normally not need
                            // to, but in this case we do because we set the value as a side effect
                            // rather than returning it.
                            let value = input.parse_entirely(|i| Parse::parse(self.context, i))?;
                            self.descriptors.$ident = Some(value)
                        },
                    )*
                    _ => return Err(input.new_custom_error(SelectorParseErrorKind::UnexpectedIdent(name.clone()))),
                }
                Ok(())
            }
        }
    }
}

property_descriptors! {
    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#the-syntax-descriptor>
    "syntax" syntax: Descriptor,

    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#inherits-descriptor>
    "inherits" inherits: Inherits,

    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#initial-value-descriptor>
    "initial-value" initial_value: InitialValue,
}

/// Errors that can happen when registering a property.
#[allow(missing_docs)]
pub enum PropertyRegistrationError {
    NoInitialValue,
    InvalidInitialValue,
    InitialValueNotComputationallyIndependent,
}

impl PropertyRegistration {
    /// Measure heap usage.
    #[cfg(feature = "gecko")]
    pub fn size_of(&self, _: &SharedRwLockReadGuard, ops: &mut MallocSizeOfOps) -> usize {
        MallocSizeOf::size_of(self, ops)
    }

    /// Computes the value of the computationally independent initial value.
    pub fn compute_initial_value(
        &self,
        computed_context: &computed::Context,
    ) -> Result<ComputedRegisteredValue, ()> {
        let Some(ref initial) = self.data.initial_value else {
            return Err(());
        };

        if self.data.syntax.is_universal() {
            return Ok(ComputedRegisteredValue::universal(Arc::clone(initial)));
        }

        let mut input = ParserInput::new(initial.css_text());
        let mut input = Parser::new(&mut input);
        input.skip_whitespace();

        match SpecifiedRegisteredValue::compute(
            &mut input,
            &self.data,
            &self.url_data,
            computed_context,
            AllowComputationallyDependent::No,
        ) {
            Ok(computed) => Ok(computed),
            Err(_) => Err(()),
        }
    }

    /// Performs syntax validation as per the initial value descriptor.
    /// https://drafts.css-houdini.org/css-properties-values-api-1/#initial-value-descriptor
    pub fn validate_initial_value(
        syntax: &Descriptor,
        initial_value: Option<&SpecifiedValue>,
    ) -> Result<(), PropertyRegistrationError> {
        use crate::properties::CSSWideKeyword;
        // If the value of the syntax descriptor is the universal syntax definition, then the
        // initial-value descriptor is optional. If omitted, the initial value of the property is
        // the guaranteed-invalid value.
        if syntax.is_universal() && initial_value.is_none() {
            return Ok(());
        }

        // Otherwise, if the value of the syntax descriptor is not the universal syntax definition,
        // the following conditions must be met for the @property rule to be valid:

        // The initial-value descriptor must be present.
        let Some(initial) = initial_value else {
            return Err(PropertyRegistrationError::NoInitialValue);
        };

        // A value that references the environment or other variables is not computationally
        // independent.
        if initial.has_references() {
            return Err(PropertyRegistrationError::InitialValueNotComputationallyIndependent);
        }

        let mut input = ParserInput::new(initial.css_text());
        let mut input = Parser::new(&mut input);
        input.skip_whitespace();

        // The initial-value cannot include CSS-wide keywords.
        if input.try_parse(CSSWideKeyword::parse).is_ok() {
            return Err(PropertyRegistrationError::InitialValueNotComputationallyIndependent);
        }

        match SpecifiedRegisteredValue::parse(
            &mut input,
            syntax,
            &initial.url_data,
            AllowComputationallyDependent::No,
        ) {
            Ok(_) => {},
            Err(_) => return Err(PropertyRegistrationError::InvalidInitialValue),
        }

        Ok(())
    }
}

impl ToCssWithGuard for PropertyRegistration {
    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#serialize-a-csspropertyrule>
    fn to_css(&self, _guard: &SharedRwLockReadGuard, dest: &mut CssStringWriter) -> fmt::Result {
        dest.write_str("@property ")?;
        self.name.to_css(&mut CssWriter::new(dest))?;
        dest.write_str(" { ")?;
        self.decl_to_css(dest)?;
        dest.write_char('}')
    }
}

impl ToShmem for PropertyRegistration {
    fn to_shmem(&self, _builder: &mut SharedMemoryBuilder) -> to_shmem::Result<Self> {
        Err(String::from(
            "ToShmem failed for PropertyRule: cannot handle @property rules",
        ))
    }
}

/// A custom property name wrapper that includes the `--` prefix in its serialization
#[derive(Clone, Debug, PartialEq, MallocSizeOf)]
pub struct PropertyRuleName(pub CustomPropertyName);

impl ToCss for PropertyRuleName {
    fn to_css<W: Write>(&self, dest: &mut CssWriter<W>) -> fmt::Result {
        dest.write_str("--")?;
        serialize_atom_name(&self.0, dest)
    }
}

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#inherits-descriptor>
#[derive(Clone, Debug, MallocSizeOf, PartialEq, ToCss)]
pub enum Inherits {
    /// `true` value for the `inherits` descriptor
    True,
    /// `false` value for the `inherits` descriptor
    False,
}

impl Parse for Inherits {
    fn parse<'i, 't>(
        _context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self, ParseError<'i>> {
        // FIXME(bug 1927012): Remove `return` from try_match_ident_ignore_ascii_case so the closure
        // can be removed.
        let result: Result<Inherits, ParseError> = (|| {
            try_match_ident_ignore_ascii_case! { input,
                "true" => Ok(Inherits::True),
                "false" => Ok(Inherits::False),
            }
        })();
        if let Err(err) = result {
            Err(ParseError {
                kind: ParseErrorKind::Custom(StyleParseErrorKind::PropertyInheritsField(
                    PropertyInheritsParseError::InvalidInherits,
                )),
                location: err.location,
            })
        } else {
            result
        }
    }
}

/// Specifies the initial value of the custom property registration represented by the @property
/// rule, controlling the property’s initial value.
///
/// The SpecifiedValue is wrapped in an Arc to avoid copying when using it.
pub type InitialValue = Arc<SpecifiedValue>;

impl Parse for InitialValue {
    fn parse<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self, ParseError<'i>> {
        input.skip_whitespace();
        Ok(Arc::new(SpecifiedValue::parse(input, &context.url_data)?))
    }
}
