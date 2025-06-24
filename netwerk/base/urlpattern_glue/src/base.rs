/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nsstring::nsCString;
use std::ffi::c_void;
use thin_vec::ThinVec;

// Note that we use MaybeString throughout the c++/rust ffi boundary functions
// since having a *nsCString to emulate Option<nsCString> (which has no representation on both rust and C++
// operate on the string with common methods like slice() and to_utf8().
// side) doesn't seem to survive crossing the ffi boundary, resulting in a crash when we attempt to
// We were getting errors like this:
// * Hit MOZ_CRASH(unsafe precondition(s) violated: slice::from_raw_parts requires the pointer
// * to be aligned and non-null, and the total size of the slice not to exceed `isize::MAX`)
// * at core/src/panicking.rs:223
#[derive(Debug, Clone)]
#[repr(C)]
pub struct MaybeString {
    pub string: nsCString,
    pub valid: bool,
}

impl MaybeString {
    pub fn new(s: &nsCString) -> Self {
        Self {
            string: s.clone(),
            valid: true,
        }
    }
    pub fn none() -> Self {
        Self {
            string: nsCString::new(),
            valid: false,
        }
    }
}

// this used to hide info of internal urlpattern::url from C++ compiler
// so cpp compilation doesn't fail since we don't expose url to gecko
#[repr(C)]
pub struct UrlpPattern(pub *mut c_void); // structs with unnamed fields

#[derive(Debug, Clone)]
#[repr(C)]
pub struct UrlpInit {
    pub protocol: MaybeString,
    pub username: MaybeString,
    pub password: MaybeString,
    pub hostname: MaybeString,
    pub port: MaybeString,
    pub pathname: MaybeString,
    pub search: MaybeString,
    pub hash: MaybeString,
    pub base_url: MaybeString,
}

impl UrlpInit {
    pub fn none() -> Self {
        Self {
            protocol: MaybeString::none(),
            username: MaybeString::none(),
            password: MaybeString::none(),
            hostname: MaybeString::none(),
            port: MaybeString::none(),
            pathname: MaybeString::none(),
            search: MaybeString::none(),
            hash: MaybeString::none(),
            base_url: MaybeString::none(),
        }
    }
}

#[derive(Debug)]
#[repr(C)]
pub struct UrlpMatchInput {
    pub protocol: nsCString,
    pub username: nsCString,
    pub password: nsCString,
    pub hostname: nsCString,
    pub port: nsCString,
    pub pathname: nsCString,
    pub search: nsCString,
    pub hash: nsCString,
}

#[derive(Debug)]
#[repr(C)]
pub enum UrlpStringOrInitType {
    String,
    Init,
}

// Note: rust's enum variant do not survive cbindgen's c++ generation because
// cbindgen creates a anonymous unions which creates fields that are inaccessible
// from caller code on the C++ side. We instead break the variants down into their
// own more easily digestable structs so that we can modify them on both sides
// of the ffi boundary
#[derive(Debug)]
#[repr(C)]
pub struct UrlpInput {
    pub string_or_init_type: UrlpStringOrInitType,
    pub str: nsCString,
    pub init: UrlpInit,
    pub base: MaybeString,
}

#[derive(Debug)]
#[repr(C)]
pub struct UrlpMatchInputAndInputs {
    pub input: UrlpMatchInput,
    pub inputs: UrlpInput,
}

#[derive(Debug)]
#[repr(C)]
pub struct UrlpOptions {
    pub ignore_case: bool,
}

#[derive(Debug, Clone, PartialEq)]
#[repr(C)]
pub enum UrlpInnerMatcherType {
    Literal,
    SingleCapture,
    RegExp,
}

#[derive(Debug, Clone, PartialEq)]
#[repr(C)]
pub struct UrlpInnerMatcher {
    pub inner_type: UrlpInnerMatcherType,
    pub literal: nsCString, // Literal
    pub allow_empty: bool,  // SingleCapture
    pub filter_exists: bool,
    pub filter: char,      // SingleCapture
    pub regexp: nsCString, // RegExp
}

#[derive(Debug, Clone, PartialEq)]
#[repr(C)]
pub struct UrlpMatcher {
    pub prefix: nsCString,
    pub suffix: nsCString,
    pub inner: UrlpInnerMatcher,
}

#[derive(Debug, Clone, PartialEq)]
#[repr(C)]
pub struct UrlpComponent {
    pub pattern_string: nsCString,
    pub regexp_string: nsCString,
    pub matcher: UrlpMatcher,
    pub group_name_list: ThinVec<nsCString>,
}
