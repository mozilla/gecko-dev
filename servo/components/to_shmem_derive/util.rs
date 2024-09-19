/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use darling::{FromDeriveInput, FromField};
use proc_macro2::{Span, TokenStream};
use quote::{quote, TokenStreamExt};
use syn::{self, parse_quote, DeriveInput, Field, Ident, WherePredicate};
use synstructure::{self, BindStyle, BindingInfo, VariantInfo};

pub(crate) fn parse_input_attrs<A>(input: &DeriveInput) -> A
where
    A: FromDeriveInput,
{
    match A::from_derive_input(input) {
        Ok(attrs) => attrs,
        Err(e) => panic!("failed to parse input attributes: {}", e),
    }
}

pub(crate) fn parse_field_attrs<A>(field: &Field) -> A
where
    A: FromField,
{
    match A::from_field(field) {
        Ok(attrs) => attrs,
        Err(e) => panic!("failed to parse field attributes: {}", e),
    }
}

pub(crate) fn add_predicate(where_clause: &mut Option<syn::WhereClause>, pred: WherePredicate) {
    where_clause
        .get_or_insert(parse_quote!(where))
        .predicates
        .push(pred);
}

pub(crate) fn fmap2_match<F, G>(
    input: &DeriveInput,
    bind_style: BindStyle,
    mut f: F,
    mut g: G,
) -> TokenStream
where
    F: FnMut(&BindingInfo) -> TokenStream,
    G: FnMut(&BindingInfo) -> Option<TokenStream>,
{
    let mut s = synstructure::Structure::new(input);
    s.variants_mut().iter_mut().for_each(|v| {
        v.bind_with(|_| bind_style);
    });
    s.each_variant(|variant| {
        let (mapped, mapped_fields) = value(variant, "mapped");
        let fields_pairs = variant.bindings().iter().zip(mapped_fields.iter());
        let mut computations = quote!();
        computations.append_all(fields_pairs.map(|(field, mapped_field)| {
            let expr = f(field);
            quote! { let #mapped_field = #expr; }
        }));
        computations.append_all(
            mapped_fields
                .iter()
                .map(|mapped_field| match g(mapped_field) {
                    Some(expr) => quote! { let #mapped_field = #expr; },
                    None => quote!(),
                }),
        );
        computations.append_all(mapped);
        Some(computations)
    })
}

fn value<'a>(variant: &'a VariantInfo, prefix: &str) -> (TokenStream, Vec<BindingInfo<'a>>) {
    let mut v = variant.clone();
    v.bindings_mut().iter_mut().for_each(|b| {
        b.binding = Ident::new(&format!("{}_{}", b.binding, prefix), Span::call_site())
    });
    v.bind_with(|_| BindStyle::Move);
    (v.pat(), v.bindings().to_vec())
}
