// Copyright 2018-2021 the Deno authors. All rights reserved. MIT license.
//! rust-urlpattern is an implementation of the
//! [URLPattern standard](https://wicg.github.io/urlpattern) for the Rust
//! programming language.
//!
//! For a usage example, see the [UrlPattern] documentation.

mod canonicalize_and_process;
mod component;
mod constructor_parser;
mod error;
mod matcher;
mod parser;
pub mod quirks;
mod regexp;
mod tokenizer;

pub use error::Error;
use serde::Deserialize;
use serde::Serialize;
use url::Url;

use crate::canonicalize_and_process::is_special_scheme;
use crate::canonicalize_and_process::process_base_url;
use crate::canonicalize_and_process::special_scheme_default_port;
use crate::canonicalize_and_process::ProcessType;
use crate::component::Component;
use crate::regexp::RegExp;

/// Options to create a URL pattern.
#[derive(Debug, Default, Clone, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct UrlPatternOptions {
  pub ignore_case: bool,
}

/// The structured input used to create a URL pattern.
#[derive(Debug, Default, Clone, Eq, PartialEq)]
pub struct UrlPatternInit {
  pub protocol: Option<String>,
  pub username: Option<String>,
  pub password: Option<String>,
  pub hostname: Option<String>,
  pub port: Option<String>,
  pub pathname: Option<String>,
  pub search: Option<String>,
  pub hash: Option<String>,
  pub base_url: Option<Url>,
}

impl UrlPatternInit {
  pub fn parse_constructor_string<R: RegExp>(
    pattern: &str,
    base_url: Option<Url>,
  ) -> Result<UrlPatternInit, Error> {
    let mut init = constructor_parser::parse_constructor_string::<R>(pattern)?;
    if base_url.is_none() && init.protocol.is_none() {
      return Err(Error::BaseUrlRequired);
    }
    init.base_url = base_url;
    Ok(init)
  }

  // Ref: https://wicg.github.io/urlpattern/#process-a-urlpatterninit
  // TODO: use UrlPatternInit for arguments?
  #[allow(clippy::too_many_arguments)]
  fn process(
    &self,
    kind: ProcessType,
    protocol: Option<String>,
    username: Option<String>,
    password: Option<String>,
    hostname: Option<String>,
    port: Option<String>,
    pathname: Option<String>,
    search: Option<String>,
    hash: Option<String>,
  ) -> Result<UrlPatternInit, Error> {
    let mut result = UrlPatternInit {
      protocol,
      username,
      password,
      hostname,
      port,
      pathname,
      search,
      hash,
      base_url: None,
    };

    let base_url = if let Some(parsed_base_url) = &self.base_url {
      if self.protocol.is_none() {
        result.protocol =
          Some(process_base_url(parsed_base_url.scheme(), &kind));
      }

      if kind != ProcessType::Pattern
        && (self.protocol.is_none()
          && self.hostname.is_none()
          && self.port.is_none()
          && self.username.is_none())
      {
        result.username =
          Some(process_base_url(parsed_base_url.username(), &kind));
      }

      if kind != ProcessType::Pattern
        && (self.protocol.is_none()
          && self.hostname.is_none()
          && self.port.is_none()
          && self.username.is_none()
          && self.password.is_none())
      {
        result.password = Some(process_base_url(
          parsed_base_url.password().unwrap_or_default(),
          &kind,
        ));
      }

      if self.protocol.is_none() && self.hostname.is_none() {
        result.hostname = Some(process_base_url(
          parsed_base_url.host_str().unwrap_or_default(),
          &kind,
        ));
      }

      if self.protocol.is_none()
        && self.hostname.is_none()
        && self.port.is_none()
      {
        result.port =
          Some(process_base_url(url::quirks::port(parsed_base_url), &kind));
      }

      if self.protocol.is_none()
        && self.hostname.is_none()
        && self.port.is_none()
        && self.pathname.is_none()
      {
        result.pathname = Some(process_base_url(
          url::quirks::pathname(parsed_base_url),
          &kind,
        ));
      }

      if self.protocol.is_none()
        && self.hostname.is_none()
        && self.port.is_none()
        && self.pathname.is_none()
        && self.search.is_none()
      {
        result.search = Some(process_base_url(
          parsed_base_url.query().unwrap_or_default(),
          &kind,
        ));
      }

      if self.protocol.is_none()
        && self.hostname.is_none()
        && self.port.is_none()
        && self.pathname.is_none()
        && self.search.is_none()
        && self.hash.is_none()
      {
        result.hash = Some(process_base_url(
          parsed_base_url.fragment().unwrap_or_default(),
          &kind,
        ));
      }

      Some(parsed_base_url)
    } else {
      None
    };

    if let Some(protocol) = &self.protocol {
      result.protocol = Some(canonicalize_and_process::process_protocol_init(
        protocol, &kind,
      )?);
    }
    if let Some(username) = &self.username {
      result.username = Some(canonicalize_and_process::process_username_init(
        username, &kind,
      )?);
    }
    if let Some(password) = &self.password {
      result.password = Some(canonicalize_and_process::process_password_init(
        password, &kind,
      )?);
    }
    if let Some(hostname) = &self.hostname {
      result.hostname = Some(canonicalize_and_process::process_hostname_init(
        hostname, &kind,
      )?);
    }
    if let Some(port) = &self.port {
      result.port = Some(canonicalize_and_process::process_port_init(
        port,
        result.protocol.as_deref(),
        &kind,
      )?);
    }
    if let Some(pathname) = &self.pathname {
      result.pathname = Some(pathname.clone());

      if let Some(base_url) = base_url {
        if !base_url.cannot_be_a_base()
          && !is_absolute_pathname(pathname, &kind)
        {
          let baseurl_path = url::quirks::pathname(base_url);
          let slash_index = baseurl_path.rfind('/');
          if let Some(slash_index) = slash_index {
            let new_pathname = baseurl_path[..=slash_index].to_string();
            result.pathname =
              Some(format!("{}{}", new_pathname, result.pathname.unwrap()));
          }
        }
      }

      result.pathname = Some(canonicalize_and_process::process_pathname_init(
        &result.pathname.unwrap(),
        result.protocol.as_deref(),
        &kind,
      )?);
    }
    if let Some(search) = &self.search {
      result.search = Some(canonicalize_and_process::process_search_init(
        search, &kind,
      )?);
    }
    if let Some(hash) = &self.hash {
      result.hash =
        Some(canonicalize_and_process::process_hash_init(hash, &kind)?);
    }
    Ok(result)
  }
}

// Ref: https://wicg.github.io/urlpattern/#is-an-absolute-pathname
fn is_absolute_pathname(
  input: &str,
  kind: &canonicalize_and_process::ProcessType,
) -> bool {
  if input.is_empty() {
    return false;
  }
  if input.starts_with('/') {
    return true;
  }
  if kind == &canonicalize_and_process::ProcessType::Url {
    return false;
  }
  // TODO: input code point length
  if input.len() < 2 {
    return false;
  }

  input.starts_with("\\/") || input.starts_with("{/")
}

// Ref: https://wicg.github.io/urlpattern/#urlpattern
/// A UrlPattern that can be matched against.
///
/// # Examples
///
/// ```
/// use urlpattern::UrlPattern;
/// use urlpattern::UrlPatternInit;
/// use urlpattern::UrlPatternMatchInput;
///
///# fn main() {
/// // Create the UrlPattern to match against.
/// let init = UrlPatternInit {
///   pathname: Some("/users/:id".to_owned()),
///   ..Default::default()
/// };
/// let pattern = <UrlPattern>::parse(init, Default::default()).unwrap();
///
/// // Match the pattern against a URL.
/// let url = "https://example.com/users/123".parse().unwrap();
/// let result = pattern.exec(UrlPatternMatchInput::Url(url)).unwrap().unwrap();
/// assert_eq!(result.pathname.groups.get("id").unwrap().as_ref().unwrap(), "123");
///# }
/// ```
#[derive(Debug)]
pub struct UrlPattern<R: RegExp = regex::Regex> {
  protocol: Component<R>,
  username: Component<R>,
  password: Component<R>,
  hostname: Component<R>,
  port: Component<R>,
  pathname: Component<R>,
  search: Component<R>,
  hash: Component<R>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum UrlPatternMatchInput {
  Init(UrlPatternInit),
  Url(Url),
}

impl<R: RegExp> UrlPattern<R> {
  // Ref: https://wicg.github.io/urlpattern/#dom-urlpattern-urlpattern
  /// Parse a [UrlPatternInit] into a [UrlPattern].
  pub fn parse(
    init: UrlPatternInit,
    options: UrlPatternOptions,
  ) -> Result<Self, Error> {
    Self::parse_internal(init, true, options)
  }

  pub(crate) fn parse_internal(
    init: UrlPatternInit,
    report_regex_errors: bool,
    options: UrlPatternOptions,
  ) -> Result<Self, Error> {
    let mut processed_init = init.process(
      ProcessType::Pattern,
      None,
      None,
      None,
      None,
      None,
      None,
      None,
      None,
    )?;

    //  If processedInit["protocol"] is a special scheme and processedInit["port"] is its corresponding default port
    if let Some(protocol) = &processed_init.protocol {
      if is_special_scheme(protocol) {
        let default_port = special_scheme_default_port(protocol);
        if default_port == processed_init.port.as_deref() {
          processed_init.port = Some(String::new())
        }
      }
    }

    let protocol = Component::compile(
      processed_init.protocol.as_deref(),
      canonicalize_and_process::canonicalize_protocol,
      parser::Options::default(),
    )?
    .optionally_transpose_regex_error(report_regex_errors)?;

    let hostname_is_ipv6 = processed_init
      .hostname
      .as_deref()
      .map(hostname_pattern_is_ipv6_address)
      .unwrap_or(false);

    let hostname = if hostname_is_ipv6 {
      Component::compile(
        processed_init.hostname.as_deref(),
        canonicalize_and_process::canonicalize_ipv6_hostname,
        parser::Options::hostname(),
      )?
      .optionally_transpose_regex_error(report_regex_errors)?
    } else {
      Component::compile(
        processed_init.hostname.as_deref(),
        canonicalize_and_process::canonicalize_hostname,
        parser::Options::hostname(),
      )?
      .optionally_transpose_regex_error(report_regex_errors)?
    };

    let compile_options = parser::Options {
      ignore_case: options.ignore_case,
      ..Default::default()
    };

    let pathname = if protocol.protocol_component_matches_special_scheme() {
      Component::compile(
        processed_init.pathname.as_deref(),
        canonicalize_and_process::canonicalize_pathname,
        parser::Options {
          ignore_case: options.ignore_case,
          ..parser::Options::pathname()
        },
      )?
      .optionally_transpose_regex_error(report_regex_errors)?
    } else {
      Component::compile(
        processed_init.pathname.as_deref(),
        canonicalize_and_process::canonicalize_an_opaque_pathname,
        compile_options.clone(),
      )?
      .optionally_transpose_regex_error(report_regex_errors)?
    };

    Ok(UrlPattern {
      protocol,
      username: Component::compile(
        processed_init.username.as_deref(),
        canonicalize_and_process::canonicalize_username,
        parser::Options::default(),
      )?
      .optionally_transpose_regex_error(report_regex_errors)?,
      password: Component::compile(
        processed_init.password.as_deref(),
        canonicalize_and_process::canonicalize_password,
        parser::Options::default(),
      )?
      .optionally_transpose_regex_error(report_regex_errors)?,
      hostname,
      port: Component::compile(
        processed_init.port.as_deref(),
        |port| canonicalize_and_process::canonicalize_port(port, None),
        parser::Options::default(),
      )?
      .optionally_transpose_regex_error(report_regex_errors)?,
      pathname,
      search: Component::compile(
        processed_init.search.as_deref(),
        canonicalize_and_process::canonicalize_search,
        compile_options.clone(),
      )?
      .optionally_transpose_regex_error(report_regex_errors)?,
      hash: Component::compile(
        processed_init.hash.as_deref(),
        canonicalize_and_process::canonicalize_hash,
        compile_options,
      )?
      .optionally_transpose_regex_error(report_regex_errors)?,
    })
  }

  /// The pattern used to match against the protocol of the URL.
  pub fn protocol(&self) -> &str {
    &self.protocol.pattern_string
  }

  /// The pattern used to match against the username of the URL.
  pub fn username(&self) -> &str {
    &self.username.pattern_string
  }

  /// The pattern used to match against the password of the URL.
  pub fn password(&self) -> &str {
    &self.password.pattern_string
  }

  /// The pattern used to match against the hostname of the URL.
  pub fn hostname(&self) -> &str {
    &self.hostname.pattern_string
  }

  /// The pattern used to match against the port of the URL.
  pub fn port(&self) -> &str {
    &self.port.pattern_string
  }

  /// The pattern used to match against the pathname of the URL.
  pub fn pathname(&self) -> &str {
    &self.pathname.pattern_string
  }

  /// The pattern used to match against the search string of the URL.
  pub fn search(&self) -> &str {
    &self.search.pattern_string
  }

  /// The pattern used to match against the hash fragment of the URL.
  pub fn hash(&self) -> &str {
    &self.hash.pattern_string
  }

  /// Returns whether the URLPattern contains one or more groups which uses regular expression matching.
  pub fn has_regexp_groups(&self) -> bool {
    self.protocol.has_regexp_group
      || self.username.has_regexp_group
      || self.password.has_regexp_group
      || self.hostname.has_regexp_group
      || self.port.has_regexp_group
      || self.pathname.has_regexp_group
      || self.search.has_regexp_group
      || self.hash.has_regexp_group
  }

  // Ref: https://wicg.github.io/urlpattern/#dom-urlpattern-test
  /// Test if a given [UrlPatternInput] (with optional base url), matches the
  /// pattern.
  pub fn test(&self, input: UrlPatternMatchInput) -> Result<bool, Error> {
    self.matches(input).map(|res| res.is_some())
  }

  // Ref: https://wicg.github.io/urlpattern/#dom-urlpattern-exec
  /// Execute the pattern against a [UrlPatternInput] (with optional base url),
  /// returning a [UrlPatternResult] if the pattern matches. If the pattern
  /// doesn't match, returns `None`.
  pub fn exec(
    &self,
    input: UrlPatternMatchInput,
  ) -> Result<Option<UrlPatternResult>, Error> {
    self.matches(input)
  }

  // Ref: https://wicg.github.io/urlpattern/#match
  fn matches(
    &self,
    input: UrlPatternMatchInput,
  ) -> Result<Option<UrlPatternResult>, Error> {
    let input = match quirks::parse_match_input(input) {
      Some(input) => input,
      None => return Ok(None),
    };

    let protocol_exec_result = self.protocol.matcher.matches(&input.protocol);
    let username_exec_result = self.username.matcher.matches(&input.username);
    let password_exec_result = self.password.matcher.matches(&input.password);
    let hostname_exec_result = self.hostname.matcher.matches(&input.hostname);
    let port_exec_result = self.port.matcher.matches(&input.port);
    let pathname_exec_result = self.pathname.matcher.matches(&input.pathname);
    let search_exec_result = self.search.matcher.matches(&input.search);
    let hash_exec_result = self.hash.matcher.matches(&input.hash);

    match (
      protocol_exec_result,
      username_exec_result,
      password_exec_result,
      hostname_exec_result,
      port_exec_result,
      pathname_exec_result,
      search_exec_result,
      hash_exec_result,
    ) {
      (
        Some(protocol_exec_result),
        Some(username_exec_result),
        Some(password_exec_result),
        Some(hostname_exec_result),
        Some(port_exec_result),
        Some(pathname_exec_result),
        Some(search_exec_result),
        Some(hash_exec_result),
      ) => Ok(Some(UrlPatternResult {
        protocol: self
          .protocol
          .create_match_result(input.protocol.clone(), protocol_exec_result),
        username: self
          .username
          .create_match_result(input.username.clone(), username_exec_result),
        password: self
          .password
          .create_match_result(input.password.clone(), password_exec_result),
        hostname: self
          .hostname
          .create_match_result(input.hostname.clone(), hostname_exec_result),
        port: self
          .port
          .create_match_result(input.port.clone(), port_exec_result),
        pathname: self
          .pathname
          .create_match_result(input.pathname.clone(), pathname_exec_result),
        search: self
          .search
          .create_match_result(input.search.clone(), search_exec_result),
        hash: self
          .hash
          .create_match_result(input.hash.clone(), hash_exec_result),
      })),
      _ => Ok(None),
    }
  }
}

// Ref: https://wicg.github.io/urlpattern/#hostname-pattern-is-an-ipv6-address
fn hostname_pattern_is_ipv6_address(input: &str) -> bool {
  // TODO: code point length
  if input.len() < 2 {
    return false;
  }

  input.starts_with('[') || input.starts_with("{[") || input.starts_with("\\[")
}

// Ref: https://wicg.github.io/urlpattern/#dictdef-urlpatternresult
/// A result of a URL pattern match.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UrlPatternResult {
  pub protocol: UrlPatternComponentResult,
  pub username: UrlPatternComponentResult,
  pub password: UrlPatternComponentResult,
  pub hostname: UrlPatternComponentResult,
  pub port: UrlPatternComponentResult,
  pub pathname: UrlPatternComponentResult,
  pub search: UrlPatternComponentResult,
  pub hash: UrlPatternComponentResult,
}

// Ref: https://wicg.github.io/urlpattern/#dictdef-urlpatterncomponentresult
/// A result of a URL pattern match on a single component.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UrlPatternComponentResult {
  /// The matched input for this component.
  pub input: String,
  /// The values for all named groups in the pattern.
  pub groups: std::collections::HashMap<String, Option<String>>,
}

#[cfg(test)]
mod tests {
  use regex::Regex;
  use std::collections::HashMap;

  use serde::Deserialize;
  use serde::Serialize;
  use url::Url;

  use crate::quirks;
  use crate::quirks::StringOrInit;
  use crate::UrlPatternComponentResult;
  use crate::UrlPatternOptions;
  use crate::UrlPatternResult;

  use super::UrlPattern;
  use super::UrlPatternInit;

  #[derive(Debug, Deserialize)]
  #[serde(untagged)]
  #[allow(clippy::large_enum_variant)]
  enum ExpectedMatch {
    String(String),
    MatchResult(MatchResult),
  }

  #[derive(Debug, Deserialize)]
  struct ComponentResult {
    input: String,
    groups: HashMap<String, Option<String>>,
  }

  #[allow(clippy::large_enum_variant)]
  #[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
  #[serde(untagged)]
  pub enum StringOrInitOrOptions {
    Options(UrlPatternOptions),
    StringOrInit(quirks::StringOrInit),
  }

  #[derive(Debug, Deserialize)]
  struct TestCase {
    skip: Option<String>,
    pattern: Vec<StringOrInitOrOptions>,
    #[serde(default)]
    inputs: Vec<quirks::StringOrInit>,
    expected_obj: Option<quirks::StringOrInit>,
    expected_match: Option<ExpectedMatch>,
    #[serde(default)]
    exactly_empty_components: Vec<String>,
  }

  #[derive(Debug, Deserialize)]
  struct MatchResult {
    #[serde(deserialize_with = "deserialize_match_result_inputs")]
    #[serde(default)]
    inputs: Option<(quirks::StringOrInit, Option<String>)>,

    protocol: Option<ComponentResult>,
    username: Option<ComponentResult>,
    password: Option<ComponentResult>,
    hostname: Option<ComponentResult>,
    port: Option<ComponentResult>,
    pathname: Option<ComponentResult>,
    search: Option<ComponentResult>,
    hash: Option<ComponentResult>,
  }

  fn deserialize_match_result_inputs<'de, D>(
    deserializer: D,
  ) -> Result<Option<(quirks::StringOrInit, Option<String>)>, D::Error>
  where
    D: serde::Deserializer<'de>,
  {
    #[derive(Debug, Deserialize)]
    #[serde(untagged)]
    enum MatchResultInputs {
      OneArgument((quirks::StringOrInit,)),
      TwoArguments(quirks::StringOrInit, String),
    }

    let res = Option::<MatchResultInputs>::deserialize(deserializer)?;
    Ok(match res {
      Some(MatchResultInputs::OneArgument((a,))) => Some((a, None)),
      Some(MatchResultInputs::TwoArguments(a, b)) => Some((a, Some(b))),
      None => None,
    })
  }

  fn test_case(case: TestCase) {
    let mut input = quirks::StringOrInit::Init(Default::default());
    let mut base_url = None;
    let mut options = None;

    for (i, pattern_input) in case.pattern.into_iter().enumerate() {
      match pattern_input {
        StringOrInitOrOptions::StringOrInit(str_or_init) => {
          if i == 0 {
            input = str_or_init;
          } else if i == 1 {
            base_url = match str_or_init {
              StringOrInit::String(str) => Some(str.clone()),
              StringOrInit::Init(_) => None,
            };
          } else if matches!(&case.expected_obj, Some(StringOrInit::String(s)) if s == "error")
          {
            println!("Expected not to pass due to bad parameters");
            println!("✅ Passed");
            return;
          } else {
            panic!("Failed to parse testcase");
          }
        }
        StringOrInitOrOptions::Options(opts) => {
          options = Some(opts);
        }
      }
    }

    println!("\n=====");
    println!(
      "Pattern: {}, {}",
      serde_json::to_string(&input).unwrap(),
      serde_json::to_string(&base_url).unwrap()
    );
    if let Some(options) = &options {
      println!("Options: {}", serde_json::to_string(&options).unwrap(),);
    }

    if let Some(reason) = case.skip {
      println!("🟠 Skipping: {reason}");
      return;
    }

    let init_res = quirks::process_construct_pattern_input(
      input.clone(),
      base_url.as_deref(),
    );

    let res = init_res.and_then(|init_res| {
      UrlPattern::<Regex>::parse(init_res, options.unwrap_or_default())
    });
    let expected_obj = match case.expected_obj {
      Some(StringOrInit::String(s)) if s == "error" => {
        assert!(res.is_err());
        println!("✅ Passed");
        return;
      }
      Some(StringOrInit::String(_)) => unreachable!(),
      Some(StringOrInit::Init(init)) => {
        let base_url = init.base_url.map(|url| url.parse().unwrap());
        UrlPatternInit {
          protocol: init.protocol,
          username: init.username,
          password: init.password,
          hostname: init.hostname,
          port: init.port,
          pathname: init.pathname,
          search: init.search,
          hash: init.hash,
          base_url,
        }
      }
      None => UrlPatternInit::default(),
    };
    let pattern = res.expect("failed to parse pattern");

    if let StringOrInit::Init(quirks::UrlPatternInit {
      base_url: Some(url),
      ..
    }) = &input
    {
      base_url = Some(url.clone())
    }

    macro_rules! assert_field {
      ($field:ident) => {{
        let mut expected = expected_obj.$field;
        if expected == None {
          if case
            .exactly_empty_components
            .contains(&stringify!($field).to_owned())
          {
            expected = Some(String::new())
          } else if let StringOrInit::Init(quirks::UrlPatternInit {
            $field: Some($field),
            ..
          }) = &input
          {
            expected = Some($field.to_owned())
          } else if {
            if let StringOrInit::Init(init) = &input {
              match stringify!($field) {
                "protocol" => false,
                "hostname" => init.protocol.is_some(),
                "port" => init.protocol.is_some() || init.hostname.is_some(),
                "username" => false,
                "password" => false,
                "pathname" => {
                  init.protocol.is_some()
                    || init.hostname.is_some()
                    || init.port.is_some()
                }
                "search" => {
                  init.protocol.is_some()
                    || init.hostname.is_some()
                    || init.port.is_some()
                    || init.pathname.is_some()
                }
                "hash" => {
                  init.protocol.is_some()
                    || init.hostname.is_some()
                    || init.port.is_some()
                    || init.pathname.is_some()
                    || init.search.is_some()
                }
                _ => unreachable!(),
              }
            } else {
              false
            }
          } {
            expected = Some("*".to_owned())
          } else if let Some(base_url) =
            base_url.as_ref().and_then(|base_url| {
              if !matches!(stringify!($field), "username" | "password") {
                Some(base_url)
              } else {
                None
              }
            })
          {
            let base_url = Url::parse(base_url).unwrap();
            let field = url::quirks::$field(&base_url);
            let field: String = match stringify!($field) {
              "protocol" if !field.is_empty() => {
                field[..field.len() - 1].to_owned()
              }
              "search" | "hash" if !field.is_empty() => field[1..].to_owned(),
              _ => field.to_owned(),
            };
            expected = Some(field)
          } else {
            expected = Some("*".to_owned())
          }
        }

        let expected = expected.unwrap();
        let pattern = &pattern.$field.pattern_string;

        assert_eq!(
          &expected,
          pattern,
          "pattern for {} does not match",
          stringify!($field)
        );
      }};
    }

    assert_field!(protocol);
    assert_field!(username);
    assert_field!(password);
    assert_field!(hostname);
    assert_field!(port);
    assert_field!(pathname);
    assert_field!(search);
    assert_field!(hash);

    let input = case.inputs.first().cloned();
    let base_url = case.inputs.get(1).map(|input| match input {
      StringOrInit::String(str) => str.clone(),
      StringOrInit::Init(_) => unreachable!(),
    });

    println!(
      "Input: {}, {}",
      serde_json::to_string(&input).unwrap(),
      serde_json::to_string(&base_url).unwrap(),
    );

    let input = input.unwrap_or_else(|| StringOrInit::Init(Default::default()));

    let expected_input = (input.clone(), base_url.clone());

    let match_input = quirks::process_match_input(input, base_url.as_deref());

    if let Some(ExpectedMatch::String(s)) = &case.expected_match {
      if s == "error" {
        assert!(match_input.is_err());
        println!("✅ Passed");
        return;
      }
    };

    let input = match_input.expect("failed to parse match input");

    if input.is_none() {
      assert!(case.expected_match.is_none());
      println!("✅ Passed");
      return;
    }
    let test_res = if let Some((input, _)) = input.clone() {
      pattern.test(input)
    } else {
      Ok(false)
    };
    let exec_res = if let Some((input, _)) = input.clone() {
      pattern.exec(input)
    } else {
      Ok(None)
    };
    if let Some(ExpectedMatch::String(s)) = &case.expected_match {
      if s == "error" {
        assert!(test_res.is_err());
        assert!(exec_res.is_err());
        println!("✅ Passed");
        return;
      }
    };

    let expected_match = case.expected_match.map(|x| match x {
      ExpectedMatch::String(_) => unreachable!(),
      ExpectedMatch::MatchResult(x) => x,
    });

    let test = test_res.unwrap();
    let actual_match = exec_res.unwrap();

    assert_eq!(
      expected_match.is_some(),
      test,
      "pattern.test result is not correct"
    );

    let expected_match = match expected_match {
      Some(x) => x,
      None => {
        assert!(actual_match.is_none(), "expected match to be None");
        println!("✅ Passed");
        return;
      }
    };

    let actual_match = actual_match.expect("expected match to be Some");

    let expected_inputs = expected_match.inputs.unwrap_or(expected_input);

    let (_, inputs) = input.unwrap();

    assert_eq!(inputs, expected_inputs, "expected inputs to be identical");

    let exactly_empty_components = case.exactly_empty_components;

    macro_rules! convert_result {
      ($component:ident) => {
        expected_match
          .$component
          .map(|c| UrlPatternComponentResult {
            input: c.input,
            groups: c.groups,
          })
          .unwrap_or_else(|| {
            let mut groups = HashMap::new();
            if !exactly_empty_components
              .contains(&stringify!($component).to_owned())
            {
              groups.insert("0".to_owned(), Some("".to_owned()));
            }
            UrlPatternComponentResult {
              input: "".to_owned(),
              groups,
            }
          })
      };
    }

    let expected_result = UrlPatternResult {
      protocol: convert_result!(protocol),
      username: convert_result!(username),
      password: convert_result!(password),
      hostname: convert_result!(hostname),
      port: convert_result!(port),
      pathname: convert_result!(pathname),
      search: convert_result!(search),
      hash: convert_result!(hash),
    };

    assert_eq!(
      actual_match, expected_result,
      "pattern.exec result is not correct"
    );

    println!("✅ Passed");
  }

  #[test]
  fn test_cases() {
    let testdata = include_str!("./testdata/urlpatterntestdata.json");
    let cases: Vec<TestCase> = serde_json::from_str(testdata).unwrap();
    for case in cases {
      test_case(case);
    }
  }

  #[test]
  fn issue26() {
    UrlPattern::<Regex>::parse(
      UrlPatternInit {
        pathname: Some("/:foo.".to_owned()),
        ..Default::default()
      },
      Default::default(),
    )
    .unwrap();
  }

  #[test]
  fn issue46() {
    quirks::process_construct_pattern_input(
      quirks::StringOrInit::String(":café://:foo".to_owned()),
      None,
    )
    .unwrap();
  }

  #[test]
  fn has_regexp_group() {
    let pattern = <UrlPattern>::parse(
      UrlPatternInit {
        pathname: Some("/:foo.".to_owned()),
        ..Default::default()
      },
      Default::default(),
    )
    .unwrap();
    assert!(!pattern.has_regexp_groups());

    let pattern = <UrlPattern>::parse(
      UrlPatternInit {
        pathname: Some("/(.*?)".to_owned()),
        ..Default::default()
      },
      Default::default(),
    )
    .unwrap();
    assert!(pattern.has_regexp_groups());
  }
}
