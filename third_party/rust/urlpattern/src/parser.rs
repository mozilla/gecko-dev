// Copyright 2018-2021 the Deno authors. All rights reserved. MIT license.

use crate::error::ParserError;
use crate::tokenizer::Token;
use crate::tokenizer::TokenType;
use crate::Error;

// Ref: https://wicg.github.io/urlpattern/#full-wildcard-regexp-value
pub const FULL_WILDCARD_REGEXP_VALUE: &str = ".*";

/// The regexp syntax that should be used.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RegexSyntax {
  /// Compile regexes to rust-regex syntax. This is the default.
  Rust,
  /// Compile regexes to ECMAScript syntax. This should be used with the
  /// [crate::quirks::component_regex].
  ///
  /// NOTE: enabling this syntax kind, means the regex syntax will NOT be
  /// validated during parsing.
  EcmaScript,
}

// Ref: https://wicg.github.io/urlpattern/#options-header
#[derive(Debug, Clone)]
pub struct Options {
  pub delimiter_code_point: Option<char>,
  pub prefix_code_point: String, // TODO: It must contain one ASCII code point or the empty string. maybe Option<char>?
  pub regex_syntax: RegexSyntax,
  pub ignore_case: bool,
}

impl std::default::Default for Options {
  // Ref: https://wicg.github.io/urlpattern/#default-options
  #[inline]
  fn default() -> Self {
    Options {
      delimiter_code_point: None,
      prefix_code_point: String::new(),
      regex_syntax: RegexSyntax::Rust,
      ignore_case: false,
    }
  }
}

impl Options {
  // Ref: https://wicg.github.io/urlpattern/#hostname-options
  #[inline]
  pub fn hostname() -> Self {
    Options {
      delimiter_code_point: Some('.'),
      prefix_code_point: String::new(),
      regex_syntax: RegexSyntax::Rust,
      ignore_case: false,
    }
  }

  // Ref: https://wicg.github.io/urlpattern/#pathname-options
  #[inline]
  pub fn pathname() -> Self {
    Options {
      delimiter_code_point: Some('/'),
      prefix_code_point: String::from("/"),
      regex_syntax: RegexSyntax::Rust,
      ignore_case: false,
    }
  }

  // Ref: https://wicg.github.io/urlpattern/#escape-a-regexp-string
  pub fn escape_regexp_string(&self, input: &str) -> String {
    assert!(input.is_ascii());
    let mut result = String::new();
    for char in input.chars() {
      if matches!(
        char,
        '.'
        | '+'
        | '*'
        | '?'
        | '^'
        | '$'
        | '{'
        | '}'
        | '('
        | ')'
        | '['
        | ']'
        | '|'
        // | '/': deviation from spec, rust regexp crate does not handle '\/' as a valid escape sequence
        | '\\'
      ) || (char == '/' && self.regex_syntax == RegexSyntax::EcmaScript)
      {
        result.push('\\');
      }
      result.push(char);
    }
    result
  }

  // Ref: https://wicg.github.io/urlpattern/#generate-a-segment-wildcard-regexp
  #[inline]
  pub fn generate_segment_wildcard_regexp(&self) -> String {
    // NOTE: this is a deliberate deviation from the spec. In rust-regex, you
    // can not have a negative character class without specifying any
    // characters.
    if let Some(code_point) = self.delimiter_code_point {
      let mut buffer = [0; 4];
      format!(
        "[^{}]+?",
        self.escape_regexp_string(code_point.encode_utf8(&mut buffer))
      )
    } else {
      ".+?".to_owned()
    }
  }
}

// Ref: https://wicg.github.io/urlpattern/#part-type
#[derive(Debug, Eq, PartialEq)]
pub enum PartType {
  FixedText,
  Regexp,
  SegmentWildcard,
  FullWildcard,
}

// Ref: https://wicg.github.io/urlpattern/#part-modifier
#[derive(Debug, Eq, PartialEq)]
pub enum PartModifier {
  None,
  Optional,
  ZeroOrMore,
  OneOrMore,
}

impl std::fmt::Display for PartModifier {
  // Ref: https://wicg.github.io/urlpattern/#convert-a-modifier-to-a-string
  fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
    f.write_str(match self {
      PartModifier::None => "",
      PartModifier::Optional => "?",
      PartModifier::ZeroOrMore => "*",
      PartModifier::OneOrMore => "+",
    })
  }
}

// Ref: https://wicg.github.io/urlpattern/#part
#[derive(Debug)]
pub struct Part {
  pub kind: PartType,
  pub value: String,
  pub modifier: PartModifier,
  pub name: String,
  pub prefix: String,
  pub suffix: String,
}

impl Part {
  fn new(kind: PartType, value: String, modifier: PartModifier) -> Self {
    Part {
      kind,
      value,
      modifier,
      name: String::new(),
      prefix: String::new(),
      suffix: String::new(),
    }
  }
}

// Ref: https://wicg.github.io/urlpattern/#pattern-parser
struct PatternParser<F>
where
  F: Fn(&str) -> Result<String, Error>,
{
  token_list: Vec<Token>,
  encoding_callback: F,
  segment_wildcard_regexp: String,
  part_list: Vec<Part>,
  pending_fixed_value: String,
  index: usize,
  next_numeric_name: usize,
}

impl<F> PatternParser<F>
where
  F: Fn(&str) -> Result<String, Error>,
{
  // Ref: https://wicg.github.io/urlpattern/#try-to-consume-a-token
  fn try_consume_token(&mut self, kind: TokenType) -> Option<Token> {
    assert!(self.index < self.token_list.len());
    let next_token = self.token_list[self.index].clone();
    if next_token.kind != kind {
      None
    } else {
      self.index += 1;
      Some(next_token)
    }
  }

  // Ref: https://wicg.github.io/urlpattern/#try-to-consume-a-regexp-or-wildcard-token
  #[inline]
  fn try_consume_regexp_or_wildcard_token(
    &mut self,
    name_token_is_none: bool,
  ) -> Option<Token> {
    let token = self.try_consume_token(TokenType::Regexp);
    if name_token_is_none && token.is_none() {
      self.try_consume_token(TokenType::Asterisk)
    } else {
      token
    }
  }

  // Ref: https://wicg.github.io/urlpattern/#try-to-consume-a-modifier-token
  #[inline]
  fn try_consume_modifier_token(&mut self) -> Option<Token> {
    self
      .try_consume_token(TokenType::OtherModifier)
      .or_else(|| self.try_consume_token(TokenType::Asterisk))
  }

  // Ref: https://wicg.github.io/urlpattern/#maybe-add-a-part-from-the-pending-fixed-value
  #[inline]
  fn maybe_add_part_from_pending_fixed_value(&mut self) -> Result<(), Error> {
    if self.pending_fixed_value.is_empty() {
      return Ok(());
    }
    let encoded_value = (self.encoding_callback)(&self.pending_fixed_value)?;
    self.pending_fixed_value = String::new();
    self.part_list.push(Part::new(
      PartType::FixedText,
      encoded_value,
      PartModifier::None,
    ));

    Ok(())
  }

  // Ref: https://wicg.github.io/urlpattern/#add-a-part
  fn add_part(
    &mut self,
    prefix: &str,
    name_token: Option<Token>,
    regexp_or_wildcard_token: Option<Token>,
    suffix: &str,
    modifier_token: Option<Token>,
  ) -> Result<(), Error> {
    let mut modifier = PartModifier::None;
    if let Some(modifier_token) = modifier_token {
      modifier = match modifier_token.value.as_ref() {
        "?" => PartModifier::Optional,
        "*" => PartModifier::ZeroOrMore,
        "+" => PartModifier::OneOrMore,
        _ => unreachable!(),
      };
    }
    if name_token.is_none()
      && regexp_or_wildcard_token.is_none()
      && modifier == PartModifier::None
    {
      self.pending_fixed_value.push_str(prefix);
      return Ok(());
    }
    self.maybe_add_part_from_pending_fixed_value()?;
    if name_token.is_none() && regexp_or_wildcard_token.is_none() {
      assert!(suffix.is_empty());
      if prefix.is_empty() {
        return Ok(());
      }
      let encoded_value = (self.encoding_callback)(prefix)?;
      self.part_list.push(Part::new(
        PartType::FixedText,
        encoded_value,
        modifier,
      ));
      return Ok(());
    }

    let mut regexp_value = match &regexp_or_wildcard_token {
      None => self.segment_wildcard_regexp.to_owned(),
      Some(regexp_or_wildcard_token) => {
        if regexp_or_wildcard_token.kind == TokenType::Asterisk {
          FULL_WILDCARD_REGEXP_VALUE.to_string()
        } else {
          regexp_or_wildcard_token.value.to_owned()
        }
      }
    };

    let mut kind = PartType::Regexp;
    if regexp_value == self.segment_wildcard_regexp {
      kind = PartType::SegmentWildcard;
      regexp_value = String::new();
    } else if regexp_value == FULL_WILDCARD_REGEXP_VALUE {
      kind = PartType::FullWildcard;
      regexp_value = String::new();
    }

    let mut name = String::new();
    if let Some(name_token) = name_token {
      name = name_token.value;
    } else if regexp_or_wildcard_token.is_some() {
      name = self.next_numeric_name.to_string();
      self.next_numeric_name += 1;
    }
    if self.is_duplicate_name(&name) {
      return Err(Error::Parser(ParserError::DuplicateName(name)));
    }
    let encoded_prefix = (self.encoding_callback)(prefix)?;
    let encoded_suffix = (self.encoding_callback)(suffix)?;
    self.part_list.push(Part {
      kind,
      value: regexp_value,
      modifier,
      name,
      prefix: encoded_prefix,
      suffix: encoded_suffix,
    });

    Ok(())
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-duplicate-name
  fn is_duplicate_name(&self, name: &str) -> bool {
    self.part_list.iter().any(|p| p.name == name)
  }

  // Ref: https://wicg.github.io/urlpattern/#consume-text
  fn consume_text(&mut self) -> String {
    let mut result = String::new();
    loop {
      let mut token = self.try_consume_token(TokenType::Char);
      if token.is_none() {
        token = self.try_consume_token(TokenType::EscapedChar);
      }
      if token.is_none() {
        break;
      }
      result.push_str(&token.unwrap().value);
    }
    result
  }

  // Ref: https://wicg.github.io/urlpattern/#consume-a-required-token
  #[inline]
  fn consume_required_token(
    &mut self,
    kind: TokenType,
  ) -> Result<Token, Error> {
    self.try_consume_token(kind.clone()).ok_or_else(|| {
      Error::Parser(ParserError::ExpectedToken(
        kind,
        self.token_list[self.index].kind.clone(),
        self.token_list[self.index].value.clone(),
      ))
    })
  }
}

// Ref: https://wicg.github.io/urlpattern/#parse-a-pattern-string
pub fn parse_pattern_string<F>(
  input: &str,
  options: &Options,
  encoding_callback: F,
) -> Result<Vec<Part>, Error>
where
  F: Fn(&str) -> Result<String, Error>,
{
  let token_list = crate::tokenizer::tokenize(
    input,
    crate::tokenizer::TokenizePolicy::Strict,
  )?;

  let mut parser = PatternParser {
    token_list,
    encoding_callback,
    segment_wildcard_regexp: options.generate_segment_wildcard_regexp(),
    part_list: vec![],
    pending_fixed_value: String::new(),
    index: 0,
    next_numeric_name: 0,
  };

  while parser.index < parser.token_list.len() {
    let char_token = parser.try_consume_token(TokenType::Char);
    let mut name_token = parser.try_consume_token(TokenType::Name);
    let mut regexp_or_wildcard_token =
      parser.try_consume_regexp_or_wildcard_token(name_token.is_none());
    if name_token.is_some() || regexp_or_wildcard_token.is_some() {
      let mut prefix = String::new();
      if let Some(char_token) = char_token {
        char_token.value.clone_into(&mut prefix);
      }
      if !prefix.is_empty() && prefix != options.prefix_code_point {
        parser.pending_fixed_value.push_str(&prefix);
        prefix = String::new();
      }
      parser.maybe_add_part_from_pending_fixed_value()?;
      let modifier_token = parser.try_consume_modifier_token();
      parser.add_part(
        &prefix,
        name_token,
        regexp_or_wildcard_token,
        "",
        modifier_token,
      )?;
      continue;
    }
    let mut fixed_token = char_token;
    if fixed_token.is_none() {
      fixed_token = parser.try_consume_token(TokenType::EscapedChar);
    }
    if let Some(fixed_token) = fixed_token {
      parser.pending_fixed_value.push_str(&fixed_token.value);
      continue;
    }
    let open_token = parser.try_consume_token(TokenType::Open);
    if open_token.is_some() {
      let prefix = parser.consume_text();
      name_token = parser.try_consume_token(TokenType::Name);
      regexp_or_wildcard_token =
        parser.try_consume_regexp_or_wildcard_token(name_token.is_none());
      let suffix = parser.consume_text();
      parser.consume_required_token(TokenType::Close)?;
      let modifier_token = parser.try_consume_modifier_token();
      parser.add_part(
        &prefix,
        name_token,
        regexp_or_wildcard_token,
        &suffix,
        modifier_token,
      )?;
      continue;
    }
    parser.maybe_add_part_from_pending_fixed_value()?;
    parser.consume_required_token(TokenType::End)?;
  }

  Ok(parser.part_list)
}
