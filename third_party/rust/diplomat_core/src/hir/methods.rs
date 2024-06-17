//! Methods for types and navigating lifetimes within methods.

use std::collections::BTreeSet;
use std::ops::Deref;

use super::{Attrs, Docs, Ident, IdentBuf, OutType, SelfType, Type, TypeContext};

use super::lifetimes::{Lifetime, LifetimeEnv, Lifetimes, MaybeStatic};

use borrowing_field::BorrowingFieldVisitor;
use borrowing_param::BorrowingParamVisitor;

pub mod borrowing_field;
pub mod borrowing_param;

/// A method exposed to Diplomat.
#[derive(Debug)]
#[non_exhaustive]
pub struct Method {
    pub docs: Docs,
    pub name: IdentBuf,
    pub lifetime_env: LifetimeEnv,

    pub param_self: Option<ParamSelf>,
    pub params: Vec<Param>,
    pub output: ReturnType,
    pub attrs: Attrs,
}

/// Type that the method returns.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum SuccessType {
    Writeable,
    OutType(OutType),
    Unit,
}

/// Whether or not the method returns a value or a result.
#[derive(Debug)]
#[allow(clippy::exhaustive_enums)] // this only exists for fallible/infallible, breaking changes for more complex returns are ok
pub enum ReturnType {
    Infallible(SuccessType),
    Fallible(SuccessType, Option<OutType>),
    Nullable(SuccessType),
}

/// The `self` parameter of a method.
#[derive(Debug)]
#[non_exhaustive]
pub struct ParamSelf {
    pub ty: SelfType,
}

/// A parameter in a method.
#[derive(Debug)]
#[non_exhaustive]
pub struct Param {
    pub name: IdentBuf,
    pub ty: Type,
}

impl SuccessType {
    /// Returns whether the variant is `Writeable`.
    pub fn is_writeable(&self) -> bool {
        matches!(self, SuccessType::Writeable)
    }

    /// Returns whether the variant is `Unit`.
    pub fn is_unit(&self) -> bool {
        matches!(self, SuccessType::Unit)
    }

    pub fn as_type(&self) -> Option<&OutType> {
        match self {
            SuccessType::OutType(ty) => Some(ty),
            _ => None,
        }
    }
}

impl Deref for ReturnType {
    type Target = SuccessType;

    fn deref(&self) -> &Self::Target {
        match self {
            ReturnType::Infallible(ret) | ReturnType::Fallible(ret, _) | Self::Nullable(ret) => ret,
        }
    }
}

impl ReturnType {
    /// Returns `true` if the FFI function returns `void`. Not that this is different from `is_unit`,
    /// which will be true for `DiplomatResult<(), E>` and false for infallible writeable.
    pub fn is_ffi_unit(&self) -> bool {
        matches!(
            self,
            ReturnType::Infallible(SuccessType::Unit | SuccessType::Writeable)
        )
    }

    /// The "main" return type of this function: the Ok, Some, or regular type
    pub fn success_type(&self) -> &SuccessType {
        match &self {
            Self::Infallible(s) => s,
            Self::Fallible(s, _) => s,
            Self::Nullable(s) => s,
        }
    }

    /// Get the list of method lifetimes actually used by the method return type
    ///
    /// Most input lifetimes aren't actually used. An input lifetime is generated
    /// for each borrowing parameter but is only important if we use it in the return.
    pub fn used_method_lifetimes(&self) -> BTreeSet<Lifetime> {
        let mut set = BTreeSet::new();

        let mut add_to_set = |ty: &OutType| {
            for lt in ty.lifetimes() {
                if let MaybeStatic::NonStatic(lt) = lt {
                    set.insert(lt);
                }
            }
        };

        match self {
            ReturnType::Infallible(SuccessType::OutType(ref ty))
            | ReturnType::Nullable(SuccessType::OutType(ref ty)) => add_to_set(ty),
            ReturnType::Fallible(ref ok, ref err) => {
                if let SuccessType::OutType(ref ty) = ok {
                    add_to_set(ty)
                }
                if let Some(ref ty) = err {
                    add_to_set(ty)
                }
            }
            _ => (),
        }

        set
    }

    pub fn with_contained_types(&self, mut f: impl FnMut(&OutType)) {
        match self {
            Self::Infallible(SuccessType::OutType(o))
            | Self::Nullable(SuccessType::OutType(o))
            | Self::Fallible(SuccessType::OutType(o), None) => f(o),
            Self::Fallible(SuccessType::OutType(o), Some(o2)) => {
                f(o);
                f(o2)
            }
            Self::Fallible(_, Some(o)) => f(o),
            _ => (),
        }
    }
}

impl ParamSelf {
    pub(super) fn new(ty: SelfType) -> Self {
        Self { ty }
    }

    /// Return the number of fields and leaves that will show up in the [`BorrowingFieldVisitor`].
    ///
    /// This method is used to calculate how much space to allocate upfront.
    fn field_leaf_lifetime_counts(&self, tcx: &TypeContext) -> (usize, usize) {
        match self.ty {
            SelfType::Opaque(_) => (1, 1),
            SelfType::Struct(ref ty) => ty.resolve(tcx).fields.iter().fold((1, 0), |acc, field| {
                let inner = field.ty.field_leaf_lifetime_counts(tcx);
                (acc.0 + inner.0, acc.1 + inner.1)
            }),
            SelfType::Enum(_) => (0, 0),
        }
    }
}

impl Param {
    pub(super) fn new(name: IdentBuf, ty: Type) -> Self {
        Self { name, ty }
    }
}

impl Method {
    /// Returns a fresh [`Lifetimes`] corresponding to `self`.
    pub fn method_lifetimes(&self) -> Lifetimes {
        self.lifetime_env.lifetimes()
    }

    /// Returns a new [`BorrowingParamVisitor`], which can *shallowly* link output lifetimes
    /// to the parameters they borrow from.
    ///
    /// This is useful for backends which wish to have lifetime codegen for methods only handle the local
    /// method lifetime, and delegate to generated code on structs for handling the internals of struct lifetimes.
    pub fn borrowing_param_visitor<'tcx>(
        &'tcx self,
        tcx: &'tcx TypeContext,
    ) -> BorrowingParamVisitor<'tcx> {
        BorrowingParamVisitor::new(self, tcx)
    }

    /// Returns a new [`BorrowingFieldVisitor`], which allocates memory to
    /// efficiently represent all fields (and their paths!) of the inputs that
    /// have a lifetime.
    ///
    /// This is useful for backends which wish to "splat out" lifetime edge codegen for methods,
    /// linking each borrowed input param/field (however deep it may be in a struct) to a borrowed output param/field.
    ///
    /// ```ignore
    /// # use std::collections::BTreeMap;
    /// let visitor = method.borrowing_field_visitor(&tcx, "this".ck().unwrap());
    /// let mut map = BTreeMap::new();
    /// visitor.visit_borrowing_fields(|lifetime, field| {
    ///     map.entry(lifetime).or_default().push(field);
    /// })
    /// ```
    pub fn borrowing_field_visitor<'m>(
        &'m self,
        tcx: &'m TypeContext,
        self_name: &'m Ident,
    ) -> BorrowingFieldVisitor<'m> {
        BorrowingFieldVisitor::new(self, tcx, self_name)
    }
}
