//! #[diplomat::attr] and other attributes

use crate::ast;
use crate::ast::attrs::{AttrInheritContext, DiplomatBackendAttrCfg, StandardAttribute};
use crate::hir::lowering::ErrorStore;
use crate::hir::{
    EnumVariant, LoweringError, Method, Mutability, OpaqueId, ReturnType, SelfType, SuccessType,
    Type, TypeDef, TypeId,
};
use syn::Meta;

pub use crate::ast::attrs::RenameAttr;

/// Diplomat attribute that can be specified on items, methods, and enum variants. These
/// can be used to control the codegen in a particular backend.
///
/// Most of these are specified via `#[diplomat::attr(some cfg here, attrname)]`, where `some cfg here`
/// can be used to pick which backends something applies to.
#[non_exhaustive]
#[derive(Clone, Default, Debug)]
pub struct Attrs {
    /// "disable" this item: do not generate code for it in the backend
    ///
    /// This attribute is always inherited except to variants
    pub disable: bool,
    /// An optional namespace. None is equivalent to the root namespace.
    ///
    /// This attribute is inherited to types (and is not allowed elsewhere)
    pub namespace: Option<String>,
    /// Rename this item/method/variant
    ///
    /// This attribute is inherited except through methods and variants (and is not allowed on variants)
    pub rename: RenameAttr,
    /// Rename this item in the C ABI. This *must* be respected by backends.
    ///
    /// This attribute is inherited except through variants
    pub abi_rename: RenameAttr,
    /// This method is "special": it should generate something other than a regular method on the other side.
    /// This can be something like a constructor, an accessor, a stringifier etc.
    ///
    /// This attribute does not participate in inheritance and must always
    /// be specified on individual methods
    pub special_method: Option<SpecialMethod>,
}

/// Attributes that mark methods as "special"
#[non_exhaustive]
#[derive(Clone, Debug)]
pub enum SpecialMethod {
    /// A constructor.
    ///
    /// Must return Self (or Result<Self> for backends with `fallible_constructors` enabled )
    Constructor,
    /// A named constructor, with optional name. If the name isn't specified, it will be derived
    /// from the method name
    ///
    /// Must return Self (or Result<Self> for backends with `fallible_constructors` enabled )
    NamedConstructor(Option<String>),

    /// A getter, with optional name. If the name isn't specified, it will be derived
    /// from the method name
    ///
    /// Must have no parameters and must return something.
    Getter(Option<String>),
    /// A setter, with optional name. If the name isn't specified, it will be derived
    /// from the method name
    ///
    /// Must have no return type (aside from potentially a `Result<(), _>`) and must have one parameter
    Setter(Option<String>),
    /// A stringifier. Must have no parameters and return a string (writeable)
    Stringifier,
    /// A comparison operator. Currently unsupported
    Comparison,
    /// An iterator (a type that is mutated to produce new values)
    Iterator,
    /// An iterable (a type that can produce an iterator)
    Iterable,
    /// Indexes into the type using an integer
    Indexer,
}

/// For special methods that affect type semantics, whether this type has this method.
///
/// This will likely only contain a subset of special methods, but feel free to add more as needed.
#[derive(Debug, Default)]
#[non_exhaustive]
pub struct SpecialMethodPresence {
    pub comparator: bool,
    /// If it is an iterator, the type it iterates over
    pub iterator: Option<SuccessType>,
    /// If it is an iterable, the iterator type it returns (*not* the type it iterates over,
    /// perform lookup on that type to access)
    pub iterable: Option<OpaqueId>,
}

/// Where the attribute was found. Some attributes are only allowed in some contexts
/// (e.g. namespaces cannot be specified on methods)
#[non_exhaustive] // might add module attrs in the future
#[derive(Debug)]
pub enum AttributeContext<'a, 'b> {
    Type(TypeDef<'a>),
    EnumVariant(&'a EnumVariant),
    Method(&'a Method, TypeId, &'b mut SpecialMethodPresence),
    Module,
}

impl Attrs {
    pub fn from_ast(
        ast: &ast::Attrs,
        validator: &(impl AttributeValidator + ?Sized),
        parent_attrs: &Attrs,
        errors: &mut ErrorStore,
    ) -> Self {
        let mut this = parent_attrs.clone();
        // Backends must support this since it applies to the macro/C code.
        // No special inheritance, was already appropriately inherited in AST
        this.abi_rename = ast.abi_rename.clone();

        let support = validator.attrs_supported();
        let backend = validator.primary_name();
        for attr in &ast.attrs {
            let satisfies = match validator.satisfies_cfg(&attr.cfg) {
                Ok(satisfies) => satisfies,
                Err(e) => {
                    errors.push(e);
                    continue;
                }
            };
            if satisfies {
                let path = attr.meta.path();
                if let Some(path) = path.get_ident() {
                    if path == "disable" {
                        if let Meta::Path(_) = attr.meta {
                            if this.disable {
                                errors.push(LoweringError::Other(
                                    "Duplicate `disable` attribute".into(),
                                ));
                            } else if !support.disabling {
                                errors.push(LoweringError::Other(format!(
                                    "`disable` not supported in backend {backend}"
                                )))
                            } else {
                                this.disable = true;
                            }
                        } else {
                            errors.push(LoweringError::Other(
                                "`disable` must be a simple path".into(),
                            ))
                        }
                    } else if path == "rename" {
                        match RenameAttr::from_meta(&attr.meta) {
                            Ok(rename) => {
                                // We use the override extend mode: a single ast::Attrs
                                // will have had these attributes inherited into the list by appending
                                // to the end; so a later attribute in the list is more pertinent.
                                this.rename.extend(&rename);
                            }
                            Err(e) => errors.push(LoweringError::Other(format!(
                                "`rename` attr failed to parse: {e:?}"
                            ))),
                        }
                    } else if path == "namespace" {
                        if !support.namespacing {
                            errors.push(LoweringError::Other(format!(
                                "`namespace` not supported in backend {backend}"
                            )));
                            continue;
                        }
                        match StandardAttribute::from_meta(&attr.meta) {
                            Ok(StandardAttribute::String(s)) if s.is_empty() => {
                                this.namespace = None
                            }
                            Ok(StandardAttribute::String(s)) => this.namespace = Some(s),
                            Ok(_) | Err(_) => {
                                errors.push(LoweringError::Other(
                                    "`namespace` must have a single string parameter".to_string(),
                                ));
                                continue;
                            }
                        }
                    } else if path == "constructor"
                        || path == "stringifier"
                        || path == "comparison"
                        || path == "iterable"
                        || path == "iterator"
                        || path == "indexer"
                    {
                        if let Some(ref existing) = this.special_method {
                            errors.push(LoweringError::Other(format!(
                            "Multiple special method markers found on the same method, found {path} and {existing:?}"
                        )));
                            continue;
                        }
                        let kind = if path == "constructor" {
                            if !support.constructors {
                                errors.push(LoweringError::Other(format!(
                                    "constructor not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::Constructor
                        } else if path == "stringifier" {
                            if !support.stringifiers {
                                errors.push(LoweringError::Other(format!(
                                    "stringifier not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::Stringifier
                        } else if path == "iterable" {
                            if !support.iterables {
                                errors.push(LoweringError::Other(format!(
                                    "iterable not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::Iterable
                        } else if path == "iterator" {
                            if !support.iterators {
                                errors.push(LoweringError::Other(format!(
                                    "iterator not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::Iterator
                        } else if path == "indexer" {
                            if !support.indexing {
                                errors.push(LoweringError::Other(format!(
                                    "indexing not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::Indexer
                        } else {
                            if !support.comparators {
                                errors.push(LoweringError::Other(format!(
                                    "comparison overload not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::Comparison
                        };

                        this.special_method = Some(kind);
                    } else if path == "named_constructor" || path == "getter" || path == "setter" {
                        if let Some(ref existing) = this.special_method {
                            errors.push(LoweringError::Other(format!(
                            "Multiple special method markers found on the same method, found {path} and {existing:?}"
                        )));
                            continue;
                        }
                        let kind = if path == "named_constructor" {
                            if !support.named_constructors {
                                errors.push(LoweringError::Other(format!(
                                    "named constructors not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::NamedConstructor
                        } else if path == "getter" {
                            if !support.accessors {
                                errors.push(LoweringError::Other(format!(
                                    "accessors not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::Getter
                        } else {
                            if !support.accessors {
                                errors.push(LoweringError::Other(format!(
                                    "accessors not supported in backend {backend}"
                                )))
                            }
                            SpecialMethod::Setter
                        };
                        match StandardAttribute::from_meta(&attr.meta) {
                            Ok(StandardAttribute::String(s)) => {
                                this.special_method = Some(kind(Some(s)))
                            }
                            Ok(StandardAttribute::Empty) => this.special_method = Some(kind(None)),
                            Ok(_) | Err(_) => {
                                errors.push(LoweringError::Other(format!(
                                    "`{path}` must have a single string parameter or no parameter",
                                )));
                                continue;
                            }
                        }
                    } else {
                        errors.push(LoweringError::Other(format!(
                        "Unknown diplomat attribute {path}: expected one of: `disable, rename, namespace, constructor, stringifier, comparison, named_constructor, getter, setter, indexer`"
                    )));
                    }
                } else {
                    errors.push(LoweringError::Other(format!(
                        "Unknown diplomat attribute {path:?}: expected one of: `disable, rename, namespace, constructor, stringifier, comparison, named_constructor, getter, setter, indexer`"
                    )));
                }
            }
        }

        this
    }

    /// Validate that this attribute is allowed in this context
    pub(crate) fn validate(
        &self,
        validator: &(impl AttributeValidator + ?Sized),
        mut context: AttributeContext,
        errors: &mut ErrorStore,
    ) {
        // use an exhaustive destructure so new attributes are handled
        let Attrs {
            disable,
            namespace,
            rename: _,
            abi_rename: _,
            special_method,
        } = &self;

        if *disable && matches!(context, AttributeContext::EnumVariant(..)) {
            errors.push(LoweringError::Other(
                "`disable` cannot be used on enum variants".into(),
            ))
        }

        if let Some(ref special) = special_method {
            if let AttributeContext::Method(method, self_id, ref mut special_method_presence) =
                context
            {
                match special {
                    SpecialMethod::Constructor | SpecialMethod::NamedConstructor(..) => {
                        if method.param_self.is_some() {
                            errors.push(LoweringError::Other(
                                "Constructors must not accept a self parameter".to_string(),
                            ))
                        }
                        let output = method.output.success_type();
                        match method.output {
                            ReturnType::Infallible(_) => (),
                            ReturnType::Fallible(..) => {
                                if !validator.attrs_supported().fallible_constructors {
                                    errors.push(LoweringError::Other(
                                        "This backend doesn't support fallible constructors"
                                            .to_string(),
                                    ))
                                }
                            }
                            ReturnType::Nullable(..) => {
                                errors.push(LoweringError::Other("Diplomat doesn't support turning nullable methods into constructors".to_string()));
                            }
                        }

                        if let SuccessType::OutType(t) = &output {
                            if t.id() != Some(self_id) {
                                errors.push(LoweringError::Other(
                                    "Constructors must return Self!".to_string(),
                                ));
                            }
                        } else {
                            errors.push(LoweringError::Other(
                                "Constructors must return Self!".to_string(),
                            ));
                        }
                    }
                    SpecialMethod::Getter(_) => {
                        if !method.params.is_empty() {
                            errors
                                .push(LoweringError::Other("Getter cannot have parameters".into()));
                        }

                        // Currently does not forbid nullable getters, could if desired
                    }

                    SpecialMethod::Setter(_) => {
                        if !matches!(method.output.success_type(), SuccessType::Unit) {
                            errors.push(LoweringError::Other("Setters must return unit".into()));
                        }
                        if method.params.len() != 1 {
                            errors.push(LoweringError::Other(
                                "Setter must have exactly one parameter".into(),
                            ))
                        }

                        // Currently does not forbid fallible setters, could if desired
                    }
                    SpecialMethod::Stringifier => {
                        if !method.params.is_empty() {
                            errors
                                .push(LoweringError::Other("Getter cannot have parameters".into()));
                        }
                        if !matches!(method.output.success_type(), SuccessType::Writeable) {
                            errors.push(LoweringError::Other(
                                "Stringifier must return Writeable".into(),
                            ));
                        }
                    }
                    SpecialMethod::Comparison => {
                        if method.params.len() != 1 {
                            errors.push(LoweringError::Other(
                                "Comparator must have single parameter".into(),
                            ));
                        }
                        if special_method_presence.comparator {
                            errors.push(LoweringError::Other(
                                "Cannot define two comparators on the same type".into(),
                            ));
                        }
                        special_method_presence.comparator = true;
                        // In the long run we can actually support heterogeneous comparators. Not a priority right now.
                        const COMPARATOR_ERROR: &str =
                            "Comparator's parameter must be identical to self";
                        if let Some(ref selfty) = method.param_self {
                            if let Some(param) = method.params.first() {
                                match (&selfty.ty, &param.ty) {
                                    (SelfType::Opaque(p), Type::Opaque(p2)) => {
                                        if p.tcx_id != p2.tcx_id {
                                            errors.push(LoweringError::Other(
                                                COMPARATOR_ERROR.into(),
                                            ));
                                        }

                                        if p.owner.mutability != Mutability::Immutable
                                            || p2.owner.mutability != Mutability::Immutable
                                        {
                                            errors.push(LoweringError::Other(
                                                "comparators must accept immutable parameters"
                                                    .into(),
                                            ));
                                        }

                                        if p2.optional.0 {
                                            errors.push(LoweringError::Other(
                                                "comparators must accept non-optional parameters"
                                                    .into(),
                                            ));
                                        }
                                    }
                                    (SelfType::Struct(p), Type::Struct(p2)) => {
                                        if p.tcx_id != p2.tcx_id {
                                            errors.push(LoweringError::Other(
                                                COMPARATOR_ERROR.into(),
                                            ));
                                        }
                                    }
                                    (SelfType::Enum(p), Type::Enum(p2)) => {
                                        if p.tcx_id != p2.tcx_id {
                                            errors.push(LoweringError::Other(
                                                COMPARATOR_ERROR.into(),
                                            ));
                                        }
                                    }
                                    _ => {
                                        errors.push(LoweringError::Other(COMPARATOR_ERROR.into()));
                                    }
                                }
                            }
                        } else {
                            errors
                                .push(LoweringError::Other("Comparator must be non-static".into()));
                        }
                    }
                    SpecialMethod::Iterator => {
                        if special_method_presence.iterator.is_some() {
                            errors.push(LoweringError::Other(
                                "Cannot mark type as iterator twice".into(),
                            ));
                        }
                        if !method.params.is_empty() {
                            errors.push(LoweringError::Other(
                                "Iterators cannot take parameters".into(),
                            ))
                        }
                        // In theory we could support struct and enum iterators. The benefit is slight:
                        // it generates probably inefficient code whilst being rather weird when it comes to the
                        // "structs and enums convert across the boundary" norm for backends.
                        //
                        // Essentially, the `&mut self` behavior won't work right.
                        //
                        // Furthermore, in some backends (like Dart) defining an iterator may requiring adding fields,
                        // which may not be possible for enums, and would still be an odd-one-out field for structs.g s
                        if let Some(this) = &method.param_self {
                            if !matches!(this.ty, SelfType::Opaque(..)) {
                                errors.push(LoweringError::Other(
                                    "Iterators only allowed on opaques".into(),
                                ))
                            }
                        } else {
                            errors.push(LoweringError::Other("Iterators must take self".into()))
                        }

                        if let ReturnType::Nullable(ref o) = method.output {
                            if let SuccessType::Unit = o {
                                errors.push(LoweringError::Other(
                                    "Iterator method must return something".into(),
                                ));
                            }
                            special_method_presence.iterator = Some(o.clone());
                        } else if let ReturnType::Infallible(SuccessType::OutType(
                            crate::hir::OutType::Opaque(
                                ref o @ crate::hir::OpaquePath {
                                    optional: crate::hir::Optional(true),
                                    ..
                                },
                            ),
                        )) = method.output
                        {
                            let mut o = o.clone();
                            o.optional = crate::hir::Optional(false);

                            special_method_presence.iterator =
                                Some(SuccessType::OutType(crate::hir::OutType::Opaque(o)));
                        } else {
                            errors.push(LoweringError::Other(
                                "Iterator method must return nullable value".into(),
                            ));
                        }
                    }
                    SpecialMethod::Iterable => {
                        if special_method_presence.iterable.is_some() {
                            errors.push(LoweringError::Other(
                                "Cannot mark type as iterable twice".into(),
                            ));
                        }
                        if !method.params.is_empty() {
                            errors.push(LoweringError::Other(
                                "Iterables cannot take parameters".into(),
                            ))
                        }
                        if method.param_self.is_none() {
                            errors.push(LoweringError::Other("Iterables must take self".into()))
                        }

                        match method.output.success_type() {
                            SuccessType::OutType(ty) => {
                                if let Some(TypeId::Opaque(id)) = ty.id() {
                                    special_method_presence.iterable = Some(id);
                                } else {
                                    errors.push(LoweringError::Other(
                                        "Iterables must return a custom opaque type".into(),
                                    ))
                                }
                            }
                            _ => errors.push(LoweringError::Other(
                                "Iterables must return a custom type".into(),
                            )),
                        }
                    }
                    SpecialMethod::Indexer => {
                        if method.params.len() != 1 {
                            errors.push(LoweringError::Other(
                                "Indexer must have exactly one parameter".into(),
                            ));
                        }

                        if method.output.success_type().is_unit() {
                            errors.push(LoweringError::Other("Indexer must return a value".into()));
                        }
                    }
                }
            } else {
                errors.push(LoweringError::Other(format!("Special method (type {special:?}) not allowed on non-method context {context:?}")))
            }
        }

        if namespace.is_some()
            && matches!(
                context,
                AttributeContext::Method(..) | AttributeContext::EnumVariant(..)
            )
        {
            errors.push(LoweringError::Other(
                "`namespace` can only be used on types".to_string(),
            ));
        }
    }

    pub(crate) fn for_inheritance(&self, context: AttrInheritContext) -> Attrs {
        let rename = self.rename.attrs_for_inheritance(context, false);

        // Disabling shouldn't inherit to variants
        let disable = if context == AttrInheritContext::Variant {
            false
        } else {
            self.disable
        };
        let namespace = if matches!(
            context,
            AttrInheritContext::Module | AttrInheritContext::Type
        ) {
            self.namespace.clone()
        } else {
            None
        };

        Attrs {
            disable,
            rename,
            namespace,
            // Was already inherited on the AST side
            abi_rename: Default::default(),
            // Never inherited
            special_method: None,
        }
    }
}

#[non_exhaustive]
#[derive(Copy, Clone, Debug, Default)]
pub struct BackendAttrSupport {
    pub disabling: bool,
    pub renaming: bool,
    pub namespacing: bool,
    pub constructors: bool,
    pub named_constructors: bool,
    pub fallible_constructors: bool,
    pub accessors: bool,
    pub stringifiers: bool,
    pub comparators: bool,
    pub memory_sharing: bool,
    pub iterators: bool,
    pub iterables: bool,
    pub indexing: bool,
    // more to be added: namespace, etc
}

impl BackendAttrSupport {
    #[cfg(test)]
    fn all_true() -> Self {
        Self {
            disabling: true,
            renaming: true,
            namespacing: true,
            constructors: true,
            named_constructors: true,
            fallible_constructors: true,
            accessors: true,
            stringifiers: true,
            comparators: true,
            memory_sharing: true,
            iterators: true,
            iterables: true,
            indexing: true,
        }
    }
}

/// Defined by backends when validating attributes
pub trait AttributeValidator {
    /// The primary name of the backend, for use in diagnostics
    fn primary_name(&self) -> &str;
    /// Does this backend satisfy `cfg(backend_name)`?
    /// (Backends are allowed to satisfy multiple backend names, useful when there
    /// are multiple backends for a language)
    fn is_backend(&self, backend_name: &str) -> bool;
    /// does this backend satisfy cfg(name = value)?
    fn is_name_value(&self, name: &str, value: &str) -> Result<bool, LoweringError>;
    /// What backedn attrs does this support?
    fn attrs_supported(&self) -> BackendAttrSupport;

    /// Provided, checks if type satisfies a `DiplomatBackendAttrCfg`
    fn satisfies_cfg(&self, cfg: &DiplomatBackendAttrCfg) -> Result<bool, LoweringError> {
        Ok(match *cfg {
            DiplomatBackendAttrCfg::Not(ref c) => !self.satisfies_cfg(c)?,
            DiplomatBackendAttrCfg::Any(ref cs) => {
                for c in cs {
                    if self.satisfies_cfg(c)? {
                        return Ok(true);
                    }
                }
                false
            }
            DiplomatBackendAttrCfg::All(ref cs) => {
                for c in cs {
                    if !self.satisfies_cfg(c)? {
                        return Ok(false);
                    }
                }
                true
            }
            DiplomatBackendAttrCfg::Star => true,
            DiplomatBackendAttrCfg::BackendName(ref n) => self.is_backend(n),
            DiplomatBackendAttrCfg::NameValue(ref n, ref v) => self.is_name_value(n, v)?,
        })
    }

    // Provided, constructs an attribute
    fn attr_from_ast(
        &self,
        ast: &ast::Attrs,
        parent_attrs: &Attrs,
        errors: &mut ErrorStore,
    ) -> Attrs {
        Attrs::from_ast(ast, self, parent_attrs, errors)
    }

    // Provided: validates an attribute in the context in which it was constructed
    fn validate(&self, attrs: &Attrs, context: AttributeContext, errors: &mut ErrorStore) {
        attrs.validate(self, context, errors)
    }
}

/// A basic attribute validator
#[non_exhaustive]
#[derive(Default)]
pub struct BasicAttributeValidator {
    /// The primary name of this backend (should be unique, ideally)
    pub backend_name: String,
    /// The attributes supported
    pub support: BackendAttrSupport,
    /// Additional names for this backend
    pub other_backend_names: Vec<String>,
    /// override is_name_value()
    #[allow(clippy::type_complexity)] // dyn fn is not that complex
    pub is_name_value: Option<Box<dyn Fn(&str, &str) -> bool>>,
}

impl BasicAttributeValidator {
    pub fn new(backend_name: &str) -> Self {
        BasicAttributeValidator {
            backend_name: backend_name.into(),
            ..Self::default()
        }
    }
}

impl AttributeValidator for BasicAttributeValidator {
    fn primary_name(&self) -> &str {
        &self.backend_name
    }
    fn is_backend(&self, backend_name: &str) -> bool {
        self.backend_name == backend_name
            || self.other_backend_names.iter().any(|n| n == backend_name)
    }
    fn is_name_value(&self, name: &str, value: &str) -> Result<bool, LoweringError> {
        Ok(if name == "supports" {
            // destructure so new fields are forced to be added
            let BackendAttrSupport {
                disabling,
                renaming,
                namespacing,
                constructors,
                named_constructors,
                fallible_constructors,
                accessors,
                stringifiers,
                comparators,
                memory_sharing,
                iterators,
                iterables,
                indexing,
            } = self.support;
            match value {
                "disabling" => disabling,
                "renaming" => renaming,
                "namespacing" => namespacing,
                "constructors" => constructors,
                "named_constructors" => named_constructors,
                "fallible_constructors" => fallible_constructors,
                "accessors" => accessors,
                "stringifiers" => stringifiers,
                "comparators" => comparators,
                "memory_sharing" => memory_sharing,
                "iterators" => iterators,
                "iterables" => iterables,
                "indexing" => indexing,
                _ => {
                    return Err(LoweringError::Other(format!(
                        "Unknown supports = value found: {value}"
                    )))
                }
            }
        } else if let Some(ref nv) = self.is_name_value {
            nv(name, value)
        } else {
            false
        })
    }
    fn attrs_supported(&self) -> BackendAttrSupport {
        self.support
    }
}

#[cfg(test)]
mod tests {
    use crate::hir;
    use std::fmt::Write;

    macro_rules! uitest_lowering_attr {
        ($($file:tt)*) => {
            let parsed: syn::File = syn::parse_quote! { $($file)* };
            let custom_types = crate::ast::File::from(&parsed);
            let env = custom_types.all_types();

            let mut output = String::new();


            let mut attr_validator = hir::BasicAttributeValidator::new("tests");
            attr_validator.support = hir::BackendAttrSupport::all_true();
            match hir::TypeContext::from_ast(&env, attr_validator) {
                Ok(_context) => (),
                Err(e) => {
                    for (ctx, err) in e {
                        writeln!(&mut output, "Lowering error in {ctx}: {err}").unwrap();
                    }
                }
            };
            insta::with_settings!({}, {
                insta::assert_snapshot!(output)
            });
        }
    }

    #[test]
    fn test_comparator() {
        uitest_lowering_attr! {
            #[diplomat::bridge]
            mod ffi {
                use std::cmp;

                #[diplomat::opaque]
                struct Opaque;

                struct Struct {
                    field: u8
                }


                impl Opaque {
                    #[diplomat::attr(*, comparison)]
                    pub fn comparator_static(other: &Opaque) -> cmp::Ordering {
                        todo!()
                    }
                    #[diplomat::attr(*, comparison)]
                    pub fn comparator_none(&self) -> cmp::Ordering {
                        todo!()
                    }
                    #[diplomat::attr(*, comparison)]
                    pub fn comparator_othertype(other: Struct) -> cmp::Ordering {
                        todo!()
                    }
                    #[diplomat::attr(*, comparison)]
                    pub fn comparator_badreturn(&self, other: &Opaque) -> u8 {
                        todo!()
                    }
                    #[diplomat::attr(*, comparison)]
                    pub fn comparison_correct(&self, other: &Opaque) -> cmp::Ordering {
                        todo!()
                    }
                    pub fn comparison_unmarked(&self, other: &Opaque) -> cmp::Ordering {
                        todo!()
                    }
                    pub fn ordering_wrong(&self, other: cmp::Ordering) {
                        todo!()
                    }
                    #[diplomat::attr(*, comparison)]
                    pub fn comparison_mut(&self, other: &mut Opaque) -> cmp::Ordering {
                        todo!()
                    }
                    #[diplomat::attr(*, comparison)]
                    pub fn comparison_opt(&self, other: Option<&Opaque>) -> cmp::Ordering {
                        todo!()
                    }
                }

                impl Struct {
                    #[diplomat::attr(*, comparison)]
                    pub fn comparison_other(self, other: &Opaque) -> cmp::Ordering {
                        todo!()
                    }
                    #[diplomat::attr(*, comparison)]
                    pub fn comparison_correct(self, other: Self) -> cmp::Ordering {
                        todo!()
                    }
                }
            }
        }
    }

    #[test]
    fn test_iterator() {
        uitest_lowering_attr! {
            #[diplomat::bridge]
            mod ffi {

                #[diplomat::opaque]
                struct Opaque(Vec<u8>);
                #[diplomat::opaque]
                struct OpaqueIterator<'a>(std::slice::Iter<'a>);


                impl Opaque {
                    #[diplomat::attr(*, iterable)]
                    pub fn iterable<'a>(&'a self) -> Box<OpaqueIterator<'a>> {
                        Box::new(OpaqueIterator(self.0.iter()))
                    }
                }

                impl OpaqueIterator {
                    #[diplomat::attr(*, iterator)]
                    pub fn next(&mut self) -> Option<u8> {
                        self.0.next()
                    }
                }

                #[diplomat::opaque]
                struct Broken;

                impl Broken {
                    #[diplomat::attr(*, iterable)]
                    pub fn iterable_no_return(&self) {}
                    #[diplomat::attr(*, iterable)]
                    pub fn iterable_no_self() -> Box<BrokenIterator> { todo!() }

                    #[diplomat::attr(*, iterable)]
                    pub fn iterable_non_custom(&self) -> u8 { todo!() }
                }

                #[diplomat::opaque]
                struct BrokenIterator;

                impl BrokenIterator {
                    #[diplomat::attr(*, iterator)]
                    pub fn iterator_no_return(&self) {}
                    #[diplomat::attr(*, iterator)]
                    pub fn iterator_no_self() -> Option<u8> { todo!() }

                    #[diplomat::attr(*, iterator)]
                    pub fn iterator_no_option(&self) -> u8 { todo!() }
                }
            }
        }
    }
}
