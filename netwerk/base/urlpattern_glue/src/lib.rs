/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate urlpattern;
use urlpattern::quirks as Uq;

extern crate nsstring;
use nsstring::nsACString;
use nsstring::nsCString;
use thin_vec::ThinVec;

mod helpers;
use helpers::*;

pub mod base;
use base::*;

use log::debug;

#[no_mangle]
pub extern "C" fn urlp_parse_pattern_from_string(
    input: *const nsACString,
    base_url: *const nsACString,
    options: UrlpOptions,
    res: *mut UrlpPattern,
) -> bool {
    debug!("urlp_parse_pattern_from_string()");
    let init = if let Some(init) = init_from_string_and_base_url(input, base_url) {
        init
    } else {
        return false;
    };

    if let Ok(pattern) = Uq::parse_pattern(init, options.into()) {
        unsafe {
            *res = UrlpPattern(Box::into_raw(Box::new(pattern)) as *mut _);
        }
        return true;
    }
    false
}

#[no_mangle]
pub unsafe extern "C" fn urlp_parse_pattern_from_init(
    init: &UrlpInit,
    options: UrlpOptions,
    res: *mut UrlpPattern,
) -> bool {
    debug!("urlp_parse_pattern_from_init()");
    if let Ok(pattern) = Uq::parse_pattern(init.into(), options.into()) {
        *res = UrlpPattern(Box::into_raw(Box::new(pattern)) as *mut _);
        return true;
    }
    false
}

#[no_mangle]
pub unsafe extern "C" fn urlp_pattern_free(pattern: UrlpPattern) {
    drop(Box::from_raw(pattern.0 as *mut Uq::UrlPattern));
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_protocol_component(
    pattern: UrlpPattern,
    res: *mut UrlpComponent,
) {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    let tmp: UrlpComponent = q_pattern.protocol.clone().into();
    *res = tmp;
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_username_component(
    pattern: UrlpPattern,
    res: *mut UrlpComponent,
) {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    let tmp: UrlpComponent = q_pattern.username.clone().into();
    *res = tmp;
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_password_component(
    pattern: UrlpPattern,
    res: *mut UrlpComponent,
) {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    let tmp: UrlpComponent = q_pattern.password.clone().into();
    *res = tmp;
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_hostname_component(
    pattern: UrlpPattern,
    res: *mut UrlpComponent,
) {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    let tmp: UrlpComponent = q_pattern.hostname.clone().into();
    *res = tmp;
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_port_component(pattern: UrlpPattern, res: *mut UrlpComponent) {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    let tmp: UrlpComponent = q_pattern.port.clone().into();
    *res = tmp;
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_pathname_component(
    pattern: UrlpPattern,
    res: *mut UrlpComponent,
) {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    let tmp: UrlpComponent = q_pattern.pathname.clone().into();
    *res = tmp;
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_search_component(pattern: UrlpPattern, res: *mut UrlpComponent) {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    let tmp: UrlpComponent = q_pattern.search.clone().into();
    *res = tmp;
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_hash_component(pattern: UrlpPattern, res: *mut UrlpComponent) {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    let tmp: UrlpComponent = q_pattern.hash.clone().into();
    *res = tmp;
}

#[no_mangle]
pub unsafe extern "C" fn urlp_get_has_regexp_groups(pattern: UrlpPattern) -> bool {
    let q_pattern = &*(pattern.0 as *const Uq::UrlPattern);
    q_pattern.has_regexp_groups
}

// note: the ThinVec<MaybeString> is being returned as an out-param
// because if you attempt to return the vector in the normal way
// we end up with an incongruent ABI layout between C++ and rust
// which re-orders the input parameter pointers such that we cannot reference them
// by address reliably.
// Ie. ThinVec/nsTArray is a non-trivial for the purpose of calls
// so we use an out-param instead. We see similar patterns elsewhere in this file
// for return values on the C++/rust ffi boundary
#[no_mangle]
pub extern "C" fn urlp_matcher_matches_component(
    matcher: &UrlpMatcher,
    input: &nsACString,
    ignore_case: bool,
    res: &mut ThinVec<MaybeString>,
) -> bool {
    debug!("urlp_matcher_matches_component()");
    let q_matcher: Uq::Matcher = matcher.clone().into();
    let i: &str = &input.to_string();
    let matches = matcher_matches(&q_matcher, i, ignore_case);
    if let Some(inner_vec) = matches {
        for item in inner_vec {
            match item {
                Some(s) => {
                    res.push(MaybeString {
                        string: s.into(),
                        valid: true,
                    });
                }
                None => {
                    res.push(MaybeString {
                        string: nsCString::from(""),
                        valid: false,
                    });
                }
            }
        }
        true
    } else {
        false
    }
}

// note: can't return Result<Option<...>> since cbindgen doesn't handle well
// so we need to return a type that can be used in C++ and rust
#[no_mangle]
pub extern "C" fn urlp_process_match_input_from_string(
    url_str: *const nsACString,
    base_url: *const nsACString,
    res: *mut UrlpMatchInputAndInputs,
) -> bool {
    debug!("urlp_process_match_input_from_string()");
    if let Some(url) = unsafe { url_str.as_ref().map(|x| x.to_utf8().into_owned()) } {
        let str_or_init = Uq::StringOrInit::String(url);
        let maybe_base_url = if base_url.is_null() {
            None
        } else {
            let x = unsafe { (*base_url).as_str_unchecked() };
            Some(x)
        };

        let match_input_and_inputs = Uq::process_match_input(str_or_init, maybe_base_url);
        if let Ok(Some(tuple_struct)) = match_input_and_inputs {
            // parse "input"
            let match_input = tuple_struct.0;
            let maybe_match_input = Uq::parse_match_input(match_input);

            if maybe_match_input.is_none() {
                return false;
            }

            // convert "inputs"
            let tuple_soi_and_string = tuple_struct.1;
            let string = match tuple_soi_and_string.0 {
                Uq::StringOrInit::String(x) => x,
                _ => {
                    assert!(
                        false,
                        "Pulling init out of StringOrInit shouldn't happen in _from_string"
                    );
                    return false;
                }
            };
            let base = match tuple_soi_and_string.1 {
                Some(x) => MaybeString::new(&nsCString::from(x)),
                _ => MaybeString::none(),
            };
            let tmp = UrlpMatchInputAndInputs {
                input: maybe_match_input.unwrap().into(),
                inputs: UrlpInput {
                    string_or_init_type: UrlpStringOrInitType::String,
                    str: nsCString::from(string),
                    init: UrlpInit::none(),
                    base,
                },
            };
            unsafe { *res = tmp };
            return true;
        } else {
            return false;
        }
    }
    false
}

#[no_mangle]
pub extern "C" fn urlp_process_match_input_from_init(
    init: &UrlpInit,
    base_url: *const nsACString,
    res: *mut UrlpMatchInputAndInputs,
) -> bool {
    debug!("urlp_process_match_input_from_init()");
    let q_init = init.into();
    let str_or_init = Uq::StringOrInit::Init(q_init);

    let maybe_base_url = if base_url.is_null() {
        None
    } else {
        Some(unsafe { (*base_url).as_str_unchecked() })
    };
    let match_input_and_inputs = Uq::process_match_input(str_or_init, maybe_base_url);
    // an empty string passed to base_url will cause url-parsing failure
    // in process_match_input, which we handle here
    if let Ok(Some(tuple_struct)) = match_input_and_inputs {
        let match_input = tuple_struct.0;
        let maybe_match_input = Uq::parse_match_input(match_input);
        if maybe_match_input.is_none() {
            return false;
        }
        let tuple_soi_and_string = tuple_struct.1;
        let init = match tuple_soi_and_string.0 {
            Uq::StringOrInit::Init(x) => x,
            _ => {
                assert!(
                    false,
                    "Pulling string out of StringOrInit shouldn't happen in _from_init"
                );
                return false;
            }
        };

        let base = match tuple_soi_and_string.1 {
            Some(x) => MaybeString::new(&nsCString::from(x)),
            _ => MaybeString::none(),
        };

        let tmp = UrlpMatchInputAndInputs {
            input: maybe_match_input.unwrap().into(),
            inputs: UrlpInput {
                string_or_init_type: UrlpStringOrInitType::Init,
                str: nsCString::new(),
                init: init.into(),
                base,
            },
        };
        unsafe { *res = tmp };
        return true;
    } else {
        return false;
    }
}
