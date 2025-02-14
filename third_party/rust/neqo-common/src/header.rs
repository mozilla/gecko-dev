// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[derive(Debug, PartialEq, PartialOrd, Eq, Ord, Clone)]
pub struct Header {
    name: String,
    value: String,
}

impl Header {
    pub fn new<N, V>(name: N, value: V) -> Self
    where
        N: Into<String>,
        V: Into<String>,
    {
        Self {
            name: name.into(),
            value: value.into(),
        }
    }

    #[must_use]
    pub fn is_allowed_for_response(&self) -> bool {
        !matches!(
            self.name.as_str(),
            "connection"
                | "host"
                | "keep-alive"
                | "proxy-connection"
                | "te"
                | "transfer-encoding"
                | "upgrade"
        )
    }

    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    #[must_use]
    pub fn value(&self) -> &str {
        &self.value
    }
}

impl<T: AsRef<str>, U: AsRef<str>> PartialEq<(T, U)> for Header {
    fn eq(&self, other: &(T, U)) -> bool {
        self.name == other.0.as_ref() && self.value == other.1.as_ref()
    }
}

pub trait HeadersExt<'h> {
    fn contains_header<T: AsRef<str>, U: AsRef<str>>(self, name: T, value: U) -> bool;
    fn find_header<T: AsRef<str> + 'h>(self, name: T) -> Option<&'h Header>;
}

impl<'h, H> HeadersExt<'h> for H
where
    H: IntoIterator<Item = &'h Header> + 'h,
{
    fn contains_header<T: AsRef<str>, U: AsRef<str>>(self, name: T, value: U) -> bool {
        let (name, value) = (name.as_ref(), value.as_ref());
        self.into_iter().any(|h| h == &(name, value))
    }

    fn find_header<T: AsRef<str> + 'h>(self, name: T) -> Option<&'h Header> {
        let name = name.as_ref();
        self.into_iter().find(|h| h.name == name)
    }
}
