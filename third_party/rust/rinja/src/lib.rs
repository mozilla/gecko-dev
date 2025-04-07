//! [![Crates.io](https://img.shields.io/crates/v/rinja?logo=rust&style=flat-square&logoColor=white "Crates.io")](https://crates.io/crates/rinja)
//! [![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/rinja-rs/rinja/rust.yml?branch=master&logo=github&style=flat-square&logoColor=white "GitHub Workflow Status")](https://github.com/rinja-rs/rinja/actions/workflows/rust.yml)
//! [![Book](https://img.shields.io/readthedocs/rinja?label=book&logo=readthedocs&style=flat-square&logoColor=white "Book")](https://rinja.readthedocs.io/)
//! [![docs.rs](https://img.shields.io/docsrs/rinja?logo=docsdotrs&style=flat-square&logoColor=white "docs.rs")](https://docs.rs/rinja/)
//!
//! Rinja implements a type-safe compiler for Jinja-like templates.
//! It lets you write templates in a Jinja-like syntax,
//! which are linked to a `struct` defining the template context.
//! This is done using a custom derive implementation (implemented
//! in [`rinja_derive`](https://crates.io/crates/rinja_derive)).
//!
//! For feature highlights and a quick start, please review the
//! [README](https://github.com/rinja-rs/rinja/blob/master/README.md).
//!
//! You can find the documentation about our syntax, features, configuration in our book:
//! [rinja.readthedocs.io](https://rinja.readthedocs.io/).
//!
//! # Creating Rinja templates
//!
//! The main feature of Rinja is the [`Template`] derive macro
//! which reads your template code, so your `struct` can implement
//! the [`Template`] trait and [`Display`][std::fmt::Display], type-safe and fast:
//!
//! ```rust
//! # use rinja::Template;
//! #[derive(Template)]
//! #[template(
//!     ext = "html",
//!     source = "<p>© {{ year }} {{ enterprise|upper }}</p>"
//! )]
//! struct Footer<'a> {
//!     year: u16,
//!     enterprise: &'a str,
//! }
//!
//! assert_eq!(
//!     Footer { year: 2024, enterprise: "<em>Rinja</em> developers" }.to_string(),
//!     "<p>© 2024 &#60;EM&#62;RINJA&#60;/EM&#62; DEVELOPERS</p>",
//! );
//! // In here you see can Rinja's auto-escaping. You, the developer,
//! // can easily disable the auto-escaping with the `|safe` filter,
//! // but a malicious user cannot insert e.g. HTML scripts this way.
//! ```
//!
//! A Rinja template is a `struct` definition which provides the template
//! context combined with a UTF-8 encoded text file (or inline source).
//! Rinja can be used to generate any kind of text-based format.
//! The template file's extension may be used to provide content type hints.
//!
//! A template consists of **text contents**, which are passed through as-is,
//! **expressions**, which get replaced with content while being rendered, and
//! **tags**, which control the template's logic.
//! The template syntax is very similar to [Jinja](http://jinja.pocoo.org/),
//! as well as Jinja-derivatives like [Twig](http://twig.sensiolabs.org/) or
//! [Tera](https://github.com/Keats/tera).

#![cfg_attr(docsrs, feature(doc_cfg, doc_auto_cfg))]
#![deny(elided_lifetimes_in_paths)]
#![deny(unreachable_pub)]
#![deny(missing_docs)]

mod error;
pub mod filters;
#[doc(hidden)]
pub mod helpers;
mod html;

use std::{fmt, io};

pub use rinja_derive::Template;

#[doc(hidden)]
pub use crate as shared;
pub use crate::error::{Error, Result};

/// Main `Template` trait; implementations are generally derived
///
/// If you need an object-safe template, use [`DynTemplate`].
///
/// ## Rendering performance
///
/// When rendering a rinja template, you should prefer the methods
///
/// * [`.render()`][Template::render] (to render the content into a new string),
/// * [`.render_into()`][Template::render_into] (to render the content into an [`fmt::Write`]
///   object, e.g. [`String`]) or
/// * [`.write_into()`][Template::write_into] (to render the content into an [`io::Write`] object,
///   e.g. [`Vec<u8>`])
///
/// over [`.to_string()`][std::string::ToString::to_string] or [`format!()`].
/// While `.to_string()` and `format!()` give you the same result, they generally perform much worse
/// than rinja's own methods, because [`fmt::Write`] uses [dynamic methods calls] instead of
/// monomorphised code. On average, expect `.to_string()` to be 100% to 200% slower than
/// `.render()`.
///
/// [dynamic methods calls]: <https://doc.rust-lang.org/stable/std/keyword.dyn.html>
pub trait Template: fmt::Display {
    /// Helper method which allocates a new `String` and renders into it
    fn render(&self) -> Result<String> {
        let mut buf = String::new();
        let _ = buf.try_reserve(Self::SIZE_HINT);
        self.render_into(&mut buf)?;
        Ok(buf)
    }

    /// Renders the template to the given `writer` fmt buffer
    fn render_into<W: fmt::Write + ?Sized>(&self, writer: &mut W) -> Result<()>;

    /// Renders the template to the given `writer` io buffer
    fn write_into<W: io::Write + ?Sized>(&self, writer: &mut W) -> io::Result<()> {
        struct Wrapped<W: io::Write> {
            writer: W,
            err: Option<io::Error>,
        }

        impl<W: io::Write> fmt::Write for Wrapped<W> {
            fn write_str(&mut self, s: &str) -> fmt::Result {
                if let Err(err) = self.writer.write_all(s.as_bytes()) {
                    self.err = Some(err);
                    Err(fmt::Error)
                } else {
                    Ok(())
                }
            }
        }

        let mut wrapped = Wrapped { writer, err: None };
        if self.render_into(&mut wrapped).is_ok() {
            Ok(())
        } else {
            let err = wrapped.err.take();
            Err(err.unwrap_or_else(|| io::Error::new(io::ErrorKind::Other, fmt::Error)))
        }
    }

    /// The template's extension, if provided
    const EXTENSION: Option<&'static str>;

    /// Provides a rough estimate of the expanded length of the rendered template. Larger
    /// values result in higher memory usage but fewer reallocations. Smaller values result in the
    /// opposite. This value only affects [`render`]. It does not take effect when calling
    /// [`render_into`], [`write_into`], the [`fmt::Display`] implementation, or the blanket
    /// [`ToString::to_string`] implementation.
    ///
    /// [`render`]: Template::render
    /// [`render_into`]: Template::render_into
    /// [`write_into`]: Template::write_into
    const SIZE_HINT: usize;

    /// The MIME type (Content-Type) of the data that gets rendered by this Template
    const MIME_TYPE: &'static str;
}

impl<T: Template + ?Sized> Template for &T {
    #[inline]
    fn render_into<W: fmt::Write + ?Sized>(&self, writer: &mut W) -> Result<()> {
        T::render_into(self, writer)
    }

    #[inline]
    fn render(&self) -> Result<String> {
        T::render(self)
    }

    #[inline]
    fn write_into<W: io::Write + ?Sized>(&self, writer: &mut W) -> io::Result<()> {
        T::write_into(self, writer)
    }

    const EXTENSION: Option<&'static str> = T::EXTENSION;

    const SIZE_HINT: usize = T::SIZE_HINT;

    const MIME_TYPE: &'static str = T::MIME_TYPE;
}

/// Object-safe wrapper trait around [`Template`] implementers
///
/// This trades reduced performance (mostly due to writing into `dyn Write`) for object safety.
pub trait DynTemplate {
    /// Helper method which allocates a new `String` and renders into it
    fn dyn_render(&self) -> Result<String>;

    /// Renders the template to the given `writer` fmt buffer
    fn dyn_render_into(&self, writer: &mut dyn fmt::Write) -> Result<()>;

    /// Renders the template to the given `writer` io buffer
    fn dyn_write_into(&self, writer: &mut dyn io::Write) -> io::Result<()>;

    /// Helper function to inspect the template's extension
    fn extension(&self) -> Option<&'static str>;

    /// Provides a conservative estimate of the expanded length of the rendered template
    fn size_hint(&self) -> usize;

    /// The MIME type (Content-Type) of the data that gets rendered by this Template
    fn mime_type(&self) -> &'static str;
}

impl<T: Template> DynTemplate for T {
    fn dyn_render(&self) -> Result<String> {
        <Self as Template>::render(self)
    }

    fn dyn_render_into(&self, writer: &mut dyn fmt::Write) -> Result<()> {
        <Self as Template>::render_into(self, writer)
    }

    #[inline]
    fn dyn_write_into(&self, writer: &mut dyn io::Write) -> io::Result<()> {
        <Self as Template>::write_into(self, writer)
    }

    fn extension(&self) -> Option<&'static str> {
        Self::EXTENSION
    }

    fn size_hint(&self) -> usize {
        Self::SIZE_HINT
    }

    fn mime_type(&self) -> &'static str {
        Self::MIME_TYPE
    }
}

impl fmt::Display for dyn DynTemplate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.dyn_render_into(f).map_err(|_| fmt::Error {})
    }
}

/// Implement the trait `$Trait` for a list of reference (wrapper) types to `$T: $Trait + ?Sized`
macro_rules! impl_for_ref {
    (impl $Trait:ident for $T:ident $body:tt) => {
        crate::impl_for_ref! {
            impl<$T> $Trait for [
                &T
                &mut T
                Box<T>
                std::cell::Ref<'_, T>
                std::cell::RefMut<'_, T>
                std::rc::Rc<T>
                std::sync::Arc<T>
                std::sync::MutexGuard<'_, T>
                std::sync::RwLockReadGuard<'_, T>
                std::sync::RwLockWriteGuard<'_, T>
            ] $body
        }
    };
    (impl<$T:ident> $Trait:ident for [$($ty:ty)*] $body:tt) => {
        $(impl<$T: $Trait + ?Sized> $Trait for $ty $body)*
    }
}

pub(crate) use impl_for_ref;

#[cfg(test)]
mod tests {
    use std::fmt;

    use super::*;
    use crate::{DynTemplate, Template};

    #[test]
    fn dyn_template() {
        struct Test;
        impl Template for Test {
            fn render_into<W: fmt::Write + ?Sized>(&self, writer: &mut W) -> Result<()> {
                Ok(writer.write_str("test")?)
            }

            const EXTENSION: Option<&'static str> = Some("txt");

            const SIZE_HINT: usize = 4;

            const MIME_TYPE: &'static str = "text/plain; charset=utf-8";
        }

        impl fmt::Display for Test {
            #[inline]
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                self.render_into(f).map_err(|_| fmt::Error {})
            }
        }

        fn render(t: &dyn DynTemplate) -> String {
            t.dyn_render().unwrap()
        }

        let test = &Test as &dyn DynTemplate;

        assert_eq!(render(test), "test");

        assert_eq!(test.to_string(), "test");

        assert_eq!(format!("{test}"), "test");

        let mut vec = Vec::new();
        test.dyn_write_into(&mut vec).unwrap();
        assert_eq!(vec, vec![b't', b'e', b's', b't']);
    }
}
