//! This module contains functions required to integrate this library into
//! browsers. If you are not building a browser, you can ignore this module.

use serde::Deserialize;
use serde::Serialize;
use url::Url;

use crate::component::Component;
use crate::parser::RegexSyntax;
use crate::regexp::RegExp;
pub use crate::Error;
use crate::UrlPatternOptions;

#[derive(Debug, Default, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct UrlPatternInit {
  #[serde(skip_serializing_if = "Option::is_none")]
  pub protocol: Option<String>,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub username: Option<String>,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub password: Option<String>,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub hostname: Option<String>,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub port: Option<String>,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub pathname: Option<String>,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub search: Option<String>,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub hash: Option<String>,
  #[serde(rename = "baseURL", skip_serializing_if = "Option::is_none")]
  pub base_url: Option<String>,
}

#[allow(clippy::large_enum_variant)]
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum StringOrInit {
  String(String),
  Init(UrlPatternInit),
}

/// This function constructs a UrlPattern given a string or UrlPatternInit and
/// optionally a base url.
pub fn process_construct_pattern_input(
  input: StringOrInit,
  base_url: Option<&str>,
) -> Result<crate::UrlPatternInit, Error> {
  let init = match input {
    StringOrInit::String(pattern) => {
      let base_url =
        base_url.map(Url::parse).transpose().map_err(Error::Url)?;
      crate::UrlPatternInit::parse_constructor_string::<regex::Regex>(
        &pattern, base_url,
      )?
    }
    StringOrInit::Init(init) => {
      if base_url.is_some() {
        return Err(Error::BaseUrlWithInit);
      }
      let base_url = init
        .base_url
        .map(|s| Url::parse(&s))
        .transpose()
        .map_err(Error::Url)?;
      crate::UrlPatternInit {
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
  };
  Ok(init)
}
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct UrlPattern {
  pub protocol: UrlPatternComponent,
  pub username: UrlPatternComponent,
  pub password: UrlPatternComponent,
  pub hostname: UrlPatternComponent,
  pub port: UrlPatternComponent,
  pub pathname: UrlPatternComponent,
  pub search: UrlPatternComponent,
  pub hash: UrlPatternComponent,
  pub has_regexp_groups: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct UrlPatternComponent {
  pub pattern_string: String,
  pub regexp_string: String,
  pub matcher: Matcher,
  pub group_name_list: Vec<String>,
}

impl From<Component<EcmaRegexp>> for UrlPatternComponent {
  fn from(component: Component<EcmaRegexp>) -> Self {
    Self {
      pattern_string: component.pattern_string,
      regexp_string: component.regexp.unwrap().0,
      matcher: component.matcher.into(),
      group_name_list: component.group_name_list,
    }
  }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Matcher {
  pub prefix: String,
  pub suffix: String,
  #[serde(flatten)]
  pub inner: InnerMatcher,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", tag = "kind")]
pub enum InnerMatcher {
  Literal {
    literal: String,
  },
  #[serde(rename_all = "camelCase")]
  SingleCapture {
    filter: Option<char>,
    allow_empty: bool,
  },
  RegExp {
    regexp: String,
  },
}

impl From<crate::matcher::Matcher<EcmaRegexp>> for Matcher {
  fn from(matcher: crate::matcher::Matcher<EcmaRegexp>) -> Self {
    Self {
      prefix: matcher.prefix,
      suffix: matcher.suffix,
      inner: matcher.inner.into(),
    }
  }
}

impl From<crate::matcher::InnerMatcher<EcmaRegexp>> for InnerMatcher {
  fn from(inner: crate::matcher::InnerMatcher<EcmaRegexp>) -> Self {
    match inner {
      crate::matcher::InnerMatcher::Literal { literal } => {
        Self::Literal { literal }
      }
      crate::matcher::InnerMatcher::SingleCapture {
        filter,
        allow_empty,
      } => Self::SingleCapture {
        filter,
        allow_empty,
      },
      crate::matcher::InnerMatcher::RegExp { regexp } => Self::RegExp {
        regexp: regexp.unwrap().0,
      },
    }
  }
}

struct EcmaRegexp(String, String);

impl RegExp for EcmaRegexp {
  fn syntax() -> RegexSyntax {
    RegexSyntax::EcmaScript
  }

  fn parse(pattern: &str, flags: &str) -> Result<Self, ()> {
    Ok(EcmaRegexp(pattern.to_string(), flags.to_string()))
  }

  fn matches<'a>(&self, text: &'a str) -> Option<Vec<Option<&'a str>>> {
    let regexp = regex::Regex::parse(&self.0, &self.1).ok()?;
    regexp.matches(text)
  }
}

/// Parse a pattern into its components.
pub fn parse_pattern(
  init: crate::UrlPatternInit,
  options: UrlPatternOptions,
) -> Result<UrlPattern, Error> {
  let pattern =
    crate::UrlPattern::<EcmaRegexp>::parse_internal(init, false, options)?;
  let urlpattern = UrlPattern {
    has_regexp_groups: pattern.has_regexp_groups(),
    protocol: pattern.protocol.into(),
    username: pattern.username.into(),
    password: pattern.password.into(),
    hostname: pattern.hostname.into(),
    port: pattern.port.into(),
    pathname: pattern.pathname.into(),
    search: pattern.search.into(),
    hash: pattern.hash.into(),
  };
  Ok(urlpattern)
}

pub type Inputs = (StringOrInit, Option<String>);

pub fn process_match_input(
  input: StringOrInit,
  base_url_str: Option<&str>,
) -> Result<Option<(crate::UrlPatternMatchInput, Inputs)>, Error> {
  let mut inputs = (input.clone(), None);
  let init = match input {
    StringOrInit::String(url) => {
      let base_url = if let Some(base_url_str) = base_url_str {
        match Url::parse(base_url_str) {
          Ok(base_url) => {
            inputs.1 = Some(base_url_str.to_string());
            Some(base_url)
          }
          Err(_) => return Ok(None),
        }
      } else {
        None
      };
      match Url::options().base_url(base_url.as_ref()).parse(&url) {
        Ok(url) => crate::UrlPatternMatchInput::Url(url),
        Err(_) => return Ok(None),
      }
    }
    StringOrInit::Init(init) => {
      if base_url_str.is_some() {
        return Err(Error::BaseUrlWithInit);
      }
      let base_url = match init.base_url.map(|s| Url::parse(&s)).transpose() {
        Ok(base_url) => base_url,
        Err(_) => return Ok(None),
      };
      let init = crate::UrlPatternInit {
        protocol: init.protocol,
        username: init.username,
        password: init.password,
        hostname: init.hostname,
        port: init.port,
        pathname: init.pathname,
        search: init.search,
        hash: init.hash,
        base_url,
      };
      crate::UrlPatternMatchInput::Init(init)
    }
  };

  Ok(Some((init, inputs)))
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct MatchInput {
  pub protocol: String,
  pub username: String,
  pub password: String,
  pub hostname: String,
  pub port: String,
  pub pathname: String,
  pub search: String,
  pub hash: String,
}

pub fn parse_match_input(
  input: crate::UrlPatternMatchInput,
) -> Option<MatchInput> {
  let mut i = MatchInput::default();
  match input {
    crate::UrlPatternMatchInput::Init(init) => {
      if let Ok(apply_result) = init.process(
        crate::canonicalize_and_process::ProcessType::Url,
        Some(i.protocol),
        Some(i.username),
        Some(i.password),
        Some(i.hostname),
        Some(i.port),
        Some(i.pathname),
        Some(i.search),
        Some(i.hash),
      ) {
        i.protocol = apply_result.protocol.unwrap();
        i.username = apply_result.username.unwrap();
        i.password = apply_result.password.unwrap();
        i.hostname = apply_result.hostname.unwrap();
        i.port = apply_result.port.unwrap();
        i.pathname = apply_result.pathname.unwrap();
        i.search = apply_result.search.unwrap();
        i.hash = apply_result.hash.unwrap();
      } else {
        return None;
      }
    }
    crate::UrlPatternMatchInput::Url(url) => {
      i.protocol = url.scheme().to_string();
      i.username = url.username().to_string();
      i.password = url.password().unwrap_or_default().to_string();
      i.hostname = url.host_str().unwrap_or_default().to_string();
      i.port = url::quirks::port(&url).to_string();
      i.pathname = url::quirks::pathname(&url).to_string();
      i.search = url.query().unwrap_or_default().to_string();
      i.hash = url.fragment().unwrap_or_default().to_string();
    }
  }

  Some(i)
}
