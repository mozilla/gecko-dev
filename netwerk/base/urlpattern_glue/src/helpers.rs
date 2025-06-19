/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nsstring;
use nsstring::nsACString;
use nsstring::nsCString;
use thin_vec::ThinVec;

use url::Url;
use urlpattern::quirks as Uq;
use urlpattern::{UrlPatternInit, UrlPatternOptions};

use crate::base::*;

pub fn init_from_string_and_base_url(
    input: *const nsACString,
    base_url: *const nsACString,
) -> Option<UrlPatternInit> {
    if input.is_null() {
        return None;
    }
    if let Some(tmp) = unsafe { input.as_ref().map(|x| x.to_utf8().into_owned()) } {
        let maybe_base = if !base_url.is_null() {
            let tmp = unsafe { base_url.as_ref() }
                .map(|x| x.to_utf8().into_owned())
                .as_deref()
                .map(Url::parse);
            match tmp {
                Some(Ok(t)) => Some(t),
                _ => None,
            }
        } else {
            None
        };
        if let Ok(init) =
            urlpattern::UrlPatternInit::parse_constructor_string::<regex::Regex>(&tmp, maybe_base)
        {
            return Some(init.clone());
        }
    }
    None
}

pub fn maybe_to_option_string(m_str: &MaybeString) -> Option<String> {
    if !m_str.valid {
        return None;
    }
    Some(m_str.string.to_string().to_owned())
}

pub fn option_to_maybe_string(os: Option<String>) -> MaybeString {
    let s = match os {
        Some(s) => s,
        _ => {
            return MaybeString {
                string: nsCString::from(""),
                valid: false,
            }
        }
    };
    let s = nsCString::from(s.as_str());
    MaybeString {
        string: s,
        valid: true,
    }
}

// creates the regex object with the desired flags
// this function was adapted from
// https://github.com/denoland/rust-urlpattern/blob/main/src/regexp.rs
pub fn parse_regex(pattern: &str, flags: &str) -> Result<regex::Regex, ()> {
    regex::Regex::new(&format!("(?{flags}){pattern}")).map_err(|_| ())
}

// returns the list of regex capture groups contained in the text input
// this function was adapted from
// https://github.com/denoland/rust-urlpattern/blob/main/src/regexp.rs
pub fn regex_matches<'a>(regexp: regex::Regex, text: &'a str) -> Option<Vec<Option<String>>> {
    let captures = regexp.captures(text)?;
    let captures = captures
        .iter()
        .skip(1)
        .map(|c| c.map(|m| m.as_str().to_string()))
        .collect();
    Some(captures)
}

// creates a regex object and returns the list of capture groups from a given input string
// this function was adapted from
// https://github.com/denoland/rust-urlpattern/blob/main/src/quirks.rs
pub fn regexp_parse_and_matches<'a>(
    regexp: &'a str,
    input: &'a str,
    ignore_case: bool,
) -> Option<Vec<Option<String>>> {
    let flags = if ignore_case { "ui" } else { "u" };
    let regexp = parse_regex(regexp, flags).ok()?;
    regex_matches(regexp, input)
}

// performs the match on the inner matcher of a component with a given input string
// this function was adapted from
// https://github.com/denoland/rust-urlpattern/blob/main/src/matcher.rs
pub fn matcher_matches<'a>(
    matcher: &Uq::Matcher,
    mut input: &'a str,
    ignore_case: bool,
) -> Option<Vec<Option<String>>> {
    let prefix_len = matcher.prefix.len();
    let suffix_len = matcher.suffix.len();
    let input_len = input.len();
    if prefix_len + suffix_len > 0 {
        // The input must be at least as long as the prefix and suffix combined,
        // because these must both be present, and not overlap.
        if input_len < prefix_len + suffix_len {
            return None;
        }
        if !input.starts_with(&matcher.prefix) {
            return None;
        }
        if !input.ends_with(&matcher.suffix) {
            return None;
        }

        input = &input[prefix_len..input_len - suffix_len];
    }

    match &matcher.inner {
        Uq::InnerMatcher::Literal { literal } => {
            if ignore_case {
                (input.to_lowercase() == literal.to_lowercase()).then(Vec::new)
            } else {
                (input == literal).then(Vec::new)
            }
        }

        Uq::InnerMatcher::SingleCapture {
            filter,
            allow_empty,
        } => {
            if input.is_empty() && !allow_empty {
                return None;
            }
            if let Some(filter) = filter {
                if ignore_case {
                    if input
                        .to_lowercase()
                        .contains(filter.to_lowercase().collect::<Vec<_>>().as_slice())
                    {
                        return None;
                    }
                } else if input.contains(*filter) {
                    return None;
                }
            }
            Some(vec![Some(input.to_string())])
        }
        Uq::InnerMatcher::RegExp { regexp, .. } => {
            regexp_parse_and_matches(regexp.as_str(), input, ignore_case)
        }
    }
}

impl From<Uq::MatchInput> for UrlpMatchInput {
    fn from(match_input: Uq::MatchInput) -> UrlpMatchInput {
        UrlpMatchInput {
            protocol: nsCString::from(match_input.protocol),
            username: nsCString::from(match_input.username),
            password: nsCString::from(match_input.password),
            hostname: nsCString::from(match_input.hostname),
            port: nsCString::from(match_input.port),
            pathname: nsCString::from(match_input.pathname),
            search: nsCString::from(match_input.search),
            hash: nsCString::from(match_input.hash),
        }
    }
}

// convert from UrlpInit to lib::UrlPatternInit, used by:
// * parse_pattern_from_string
// * parse_pattern_from_init
impl From<UrlpInit> for UrlPatternInit {
    fn from(wrapper: UrlpInit) -> UrlPatternInit {
        let maybe_base = if wrapper.base_url.valid {
            let s = wrapper.base_url.string.to_string().to_owned();
            if s.is_empty() {
                None
            } else {
                Url::parse(s.as_str()).ok()
            }
        } else {
            None
        };
        UrlPatternInit {
            protocol: maybe_to_option_string(&wrapper.protocol),
            username: maybe_to_option_string(&wrapper.username),
            password: maybe_to_option_string(&wrapper.password),
            hostname: maybe_to_option_string(&wrapper.hostname),
            port: maybe_to_option_string(&wrapper.port),
            pathname: maybe_to_option_string(&wrapper.pathname),
            search: maybe_to_option_string(&wrapper.search),
            hash: maybe_to_option_string(&wrapper.hash),
            base_url: maybe_base,
        }
    }
}

impl From<&UrlpInit> for UrlPatternInit {
    fn from(wrapper: &UrlpInit) -> UrlPatternInit {
        let maybe_base = if wrapper.base_url.valid {
            let s = wrapper.base_url.string.to_string().to_owned();
            if s.is_empty() {
                None
            } else {
                Url::parse(s.as_str()).ok()
            }
        } else {
            None
        };
        UrlPatternInit {
            protocol: maybe_to_option_string(&wrapper.protocol),
            username: maybe_to_option_string(&wrapper.username),
            password: maybe_to_option_string(&wrapper.password),
            hostname: maybe_to_option_string(&wrapper.hostname),
            port: maybe_to_option_string(&wrapper.port),
            pathname: maybe_to_option_string(&wrapper.pathname),
            search: maybe_to_option_string(&wrapper.search),
            hash: maybe_to_option_string(&wrapper.hash),
            base_url: maybe_base,
        }
    }
}

// convert from UrlpInit to quirks::UrlPatternInit
// used by parse_pattern into the internal function
// MatchInput `From` conversion also uses
impl From<UrlpInit> for Uq::UrlPatternInit {
    fn from(wrapper: UrlpInit) -> Uq::UrlPatternInit {
        let maybe_base = if wrapper.base_url.valid {
            Some(wrapper.base_url.string.to_string())
        } else {
            None
        };

        Uq::UrlPatternInit {
            protocol: maybe_to_option_string(&wrapper.protocol),
            username: maybe_to_option_string(&wrapper.username),
            password: maybe_to_option_string(&wrapper.password),
            hostname: maybe_to_option_string(&wrapper.hostname),
            port: maybe_to_option_string(&wrapper.port),
            pathname: maybe_to_option_string(&wrapper.pathname),
            search: maybe_to_option_string(&wrapper.search),
            hash: maybe_to_option_string(&wrapper.hash),
            base_url: maybe_base,
        }
    }
}

// needed for process_match_input_from_init
impl From<&UrlpInit> for Uq::UrlPatternInit {
    fn from(wrapper: &UrlpInit) -> Self {
        let maybe_base = if wrapper.base_url.valid {
            Some(wrapper.base_url.string.to_string())
        } else {
            None
        };
        Uq::UrlPatternInit {
            protocol: maybe_to_option_string(&wrapper.protocol),
            username: maybe_to_option_string(&wrapper.username),
            password: maybe_to_option_string(&wrapper.password),
            hostname: maybe_to_option_string(&wrapper.hostname),
            port: maybe_to_option_string(&wrapper.port),
            pathname: maybe_to_option_string(&wrapper.pathname),
            search: maybe_to_option_string(&wrapper.search),
            hash: maybe_to_option_string(&wrapper.hash),
            base_url: maybe_base,
        }
    }
}

impl From<Uq::UrlPatternInit> for UrlpInit {
    fn from(init: Uq::UrlPatternInit) -> UrlpInit {
        let base = match init.base_url.as_ref() {
            Some(s) => MaybeString {
                valid: true,
                string: nsCString::from(s),
            },
            _ => MaybeString {
                valid: false,
                string: nsCString::from(""),
            },
        };

        UrlpInit {
            protocol: option_to_maybe_string(init.protocol),
            username: option_to_maybe_string(init.username),
            password: option_to_maybe_string(init.password),
            hostname: option_to_maybe_string(init.hostname),
            port: option_to_maybe_string(init.port),
            pathname: option_to_maybe_string(init.pathname),
            search: option_to_maybe_string(init.search),
            hash: option_to_maybe_string(init.hash),
            base_url: base,
        }
    }
}

impl From<UrlpInnerMatcher> for Uq::InnerMatcher {
    fn from(wrapper: UrlpInnerMatcher) -> Uq::InnerMatcher {
        match wrapper.inner_type {
            UrlpInnerMatcherType::Literal => Uq::InnerMatcher::Literal {
                literal: wrapper.literal.to_string().to_owned(),
            },
            UrlpInnerMatcherType::SingleCapture => {
                let maybe_filter = if wrapper.filter_exists {
                    Some(wrapper.filter)
                } else {
                    None
                };
                Uq::InnerMatcher::SingleCapture {
                    allow_empty: wrapper.allow_empty,
                    filter: maybe_filter,
                }
            }
            UrlpInnerMatcherType::RegExp => Uq::InnerMatcher::RegExp {
                regexp: wrapper.regexp.to_string().to_owned(),
            },
        }
    }
}

impl From<Uq::InnerMatcher> for UrlpInnerMatcher {
    fn from(inner: Uq::InnerMatcher) -> UrlpInnerMatcher {
        match inner {
            Uq::InnerMatcher::Literal { literal } => {
                UrlpInnerMatcher {
                    inner_type: UrlpInnerMatcherType::Literal,
                    literal: nsCString::from(literal).to_owned(),
                    allow_empty: false, // maybe should be an optional
                    filter_exists: false,
                    filter: 'x'.to_owned(),
                    regexp: nsCString::from("").to_owned(),
                }
            }
            Uq::InnerMatcher::SingleCapture {
                filter,
                allow_empty,
            } => {
                UrlpInnerMatcher {
                    inner_type: UrlpInnerMatcherType::SingleCapture,
                    literal: nsCString::from("").to_owned(),
                    allow_empty, // maybe should be an optional
                    filter_exists: true,
                    filter: filter.unwrap_or('x'.to_owned()),
                    regexp: nsCString::from("").to_owned(),
                }
            }
            Uq::InnerMatcher::RegExp { regexp } => UrlpInnerMatcher {
                inner_type: UrlpInnerMatcherType::RegExp,
                literal: nsCString::from("").to_owned(),
                allow_empty: false,
                filter_exists: false,
                filter: 'x'.to_owned(),
                regexp: nsCString::from(regexp).to_owned(),
            },
        }
    }
}

impl From<UrlpMatcher> for Uq::Matcher {
    fn from(wrapper: UrlpMatcher) -> Uq::Matcher {
        Uq::Matcher {
            prefix: wrapper.prefix.to_string().to_owned(),
            suffix: wrapper.suffix.to_string().to_owned(),
            inner: wrapper.inner.into(),
        }
    }
}

impl From<Uq::Matcher> for UrlpMatcher {
    fn from(matcher: Uq::Matcher) -> UrlpMatcher {
        UrlpMatcher {
            prefix: nsCString::from(matcher.prefix).to_owned(),
            suffix: nsCString::from(matcher.suffix).to_owned(),
            inner: matcher.inner.into(),
        }
    }
}

impl From<Uq::UrlPatternComponent> for UrlpComponent {
    fn from(comp: Uq::UrlPatternComponent) -> UrlpComponent {
        UrlpComponent {
            pattern_string: nsCString::from(comp.pattern_string).to_owned(),
            regexp_string: nsCString::from(comp.regexp_string).to_owned(),
            matcher: comp.matcher.into(),
            group_name_list: comp
                .group_name_list
                .into_iter()
                .map(nsCString::from)
                .collect::<ThinVec<_>>(),
        }
    }
}

// easily convert from OptionsWrapper to internal type
// used by parse_pattern
impl Into<UrlPatternOptions> for UrlpOptions {
    fn into(self) -> UrlPatternOptions {
        UrlPatternOptions {
            ignore_case: self.ignore_case,
        }
    }
}
