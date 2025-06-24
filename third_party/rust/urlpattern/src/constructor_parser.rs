// Copyright 2018-2021 the Deno authors. All rights reserved. MIT license.

use crate::error::Error;
use crate::regexp::RegExp;
use crate::tokenizer::Token;
use crate::tokenizer::TokenType;
use crate::UrlPatternInit;

// Ref: https://wicg.github.io/urlpattern/#constructor-string-parser-state
#[derive(Debug, Eq, PartialEq)]
enum ConstructorStringParserState {
  Init,
  Protocol,
  Authority,
  Username,
  Password,
  Hostname,
  Port,
  Pathname,
  Search,
  Hash,
  Done,
}

// Ref: https://wicg.github.io/urlpattern/#constructor-string-parser
struct ConstructorStringParser<'a> {
  input: &'a str,
  token_list: Vec<Token>,
  result: UrlPatternInit,
  component_start: usize,
  token_index: usize,
  token_increment: usize,
  group_depth: usize,
  hostname_ipv6_bracket_depth: usize,
  protocol_matches_special_scheme: bool,
  state: ConstructorStringParserState,
}

impl<'a> ConstructorStringParser<'a> {
  // Ref: https://wicg.github.io/urlpattern/#rewind
  #[inline]
  fn rewind(&mut self) {
    self.token_index = self.component_start;
    self.token_increment = 0;
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-hash-prefix
  #[inline]
  fn is_hash_prefix(&self) -> bool {
    self.is_non_special_pattern_char(self.token_index, "#")
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-protocol-suffix
  #[inline]
  fn is_protocol_suffix(&self) -> bool {
    self.is_non_special_pattern_char(self.token_index, ":")
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-search-prefix
  fn is_search_prefix(&self) -> bool {
    if self.is_non_special_pattern_char(self.token_index, "?") {
      return true;
    }
    if self.token_list[self.token_index].value != "?" {
      return false;
    }
    if self.token_index == 0 {
      return true;
    }
    let previous_token = self.get_safe_token(self.token_index - 1);
    !matches!(
      previous_token.kind,
      TokenType::Name
        | TokenType::Regexp
        | TokenType::Close
        | TokenType::Asterisk
    )
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-password-prefix
  #[inline]
  fn is_password_prefix(&self) -> bool {
    self.is_non_special_pattern_char(self.token_index, ":")
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-port-prefix
  #[inline]
  fn is_port_prefix(&self) -> bool {
    self.is_non_special_pattern_char(self.token_index, ":")
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-pathname-start
  #[inline]
  fn is_pathname_start(&self) -> bool {
    self.is_non_special_pattern_char(self.token_index, "/")
  }

  // Ref: https://wicg.github.io/urlpattern/#is-an-identity-terminator
  #[inline]
  fn is_identity_terminator(&self) -> bool {
    self.is_non_special_pattern_char(self.token_index, "@")
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-non-special-pattern-char
  fn is_non_special_pattern_char(&self, index: usize, value: &str) -> bool {
    let token = self.get_safe_token(index);
    if token.value != value {
      false
    } else {
      matches!(
        token.kind,
        TokenType::Char | TokenType::EscapedChar | TokenType::InvalidChar
      )
    }
  }

  // Ref: https://wicg.github.io/urlpattern/#get-a-safe-token
  fn get_safe_token(&self, index: usize) -> &Token {
    if index < self.token_list.len() {
      &self.token_list[index]
    } else {
      assert!(!self.token_list.is_empty());
      let token = self.token_list.last().unwrap();
      assert!(token.kind == TokenType::End);
      token
    }
  }

  // Ref: https://wicg.github.io/urlpattern/#change-state
  fn change_state(
    &mut self,
    new_state: ConstructorStringParserState,
    skip: usize,
  ) {
    match self.state {
      ConstructorStringParserState::Protocol => {
        self.result.protocol = Some(self.make_component_string())
      }
      ConstructorStringParserState::Username => {
        self.result.username = Some(self.make_component_string())
      }
      ConstructorStringParserState::Password => {
        self.result.password = Some(self.make_component_string())
      }
      ConstructorStringParserState::Hostname => {
        self.result.hostname = Some(self.make_component_string())
      }
      ConstructorStringParserState::Port => {
        self.result.port = Some(self.make_component_string())
      }
      ConstructorStringParserState::Pathname => {
        self.result.pathname = Some(self.make_component_string())
      }
      ConstructorStringParserState::Search => {
        self.result.search = Some(self.make_component_string())
      }
      ConstructorStringParserState::Hash => {
        self.result.hash = Some(self.make_component_string())
      }
      ConstructorStringParserState::Init
      | ConstructorStringParserState::Authority
      | ConstructorStringParserState::Done => {}
    }

    if self.state != ConstructorStringParserState::Init
      && new_state != ConstructorStringParserState::Done
    {
      if matches!(
        self.state,
        ConstructorStringParserState::Protocol
          | ConstructorStringParserState::Authority
          | ConstructorStringParserState::Username
          | ConstructorStringParserState::Password
      ) && matches!(
        new_state,
        ConstructorStringParserState::Port
          | ConstructorStringParserState::Pathname
          | ConstructorStringParserState::Search
          | ConstructorStringParserState::Hash
      ) && self.result.hostname.is_none()
      {
        self.result.hostname = Some(String::new());
      }

      if matches!(
        self.state,
        ConstructorStringParserState::Protocol
          | ConstructorStringParserState::Authority
          | ConstructorStringParserState::Username
          | ConstructorStringParserState::Password
          | ConstructorStringParserState::Hostname
          | ConstructorStringParserState::Port
      ) && matches!(
        new_state,
        ConstructorStringParserState::Search
          | ConstructorStringParserState::Hash
      ) && self.result.pathname.is_none()
      {
        if self.protocol_matches_special_scheme {
          self.result.pathname = Some(String::from("/"));
        } else {
          self.result.pathname = Some(String::new());
        }
      }

      if matches!(
        self.state,
        ConstructorStringParserState::Protocol
          | ConstructorStringParserState::Authority
          | ConstructorStringParserState::Username
          | ConstructorStringParserState::Password
          | ConstructorStringParserState::Hostname
          | ConstructorStringParserState::Port
          | ConstructorStringParserState::Pathname
      ) && new_state == ConstructorStringParserState::Hash
        && self.result.search.is_none()
      {
        self.result.search = Some(String::new());
      }
    }

    self.state = new_state;
    self.token_index += skip;
    self.component_start = self.token_index;
    self.token_increment = 0;
  }

  // Ref: https://wicg.github.io/urlpattern/#make-a-component-string
  fn make_component_string(&self) -> String {
    assert!(self.token_index < self.token_list.len());
    let token = &self.token_list[self.token_index];
    let component_start_index = self.get_safe_token(self.component_start).index;

    self
      .input
      .chars()
      .skip(component_start_index)
      .take(token.index - component_start_index)
      .collect()
  }

  // Ref: https://wicg.github.io/urlpattern/#rewind-and-set-state
  #[inline]
  fn rewind_and_set_state(&mut self, state: ConstructorStringParserState) {
    self.rewind();
    self.state = state;
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-group-open
  #[inline]
  fn is_group_open(&self) -> bool {
    self.token_list[self.token_index].kind == TokenType::Open
  }

  // Ref: https://wicg.github.io/urlpattern/#is-a-group-close
  #[inline]
  fn is_group_close(&self) -> bool {
    self.token_list[self.token_index].kind == TokenType::Close
  }

  // Ref: https://wicg.github.io/urlpattern/#compute-protocol-matches-a-special-scheme-flag
  fn compute_protocol_matches_special_scheme<R: RegExp>(
    &mut self,
  ) -> Result<(), Error> {
    let protocol_string = self.make_component_string();
    let protocol_component = crate::component::Component::<R>::compile(
      Some(&protocol_string),
      crate::canonicalize_and_process::canonicalize_protocol,
      Default::default(),
    )?;
    if protocol_component.protocol_component_matches_special_scheme() {
      self.protocol_matches_special_scheme = true;
    }
    Ok(())
  }

  // Ref: https://wicg.github.io/urlpattern/#next-is-authority-slashes
  #[inline]
  fn next_is_authority_slashes(&self) -> bool {
    if !self.is_non_special_pattern_char(self.token_index + 1, "/") {
      false
    } else {
      self.is_non_special_pattern_char(self.token_index + 2, "/")
    }
  }

  // Ref: https://wicg.github.io/urlpattern/#is-an-ipv6-open
  #[inline]
  fn is_ipv6_open(&self) -> bool {
    self.is_non_special_pattern_char(self.token_index, "[")
  }

  // Ref: https://wicg.github.io/urlpattern/#is-an-ipv6-close
  #[inline]
  fn is_ipv6_close(&self) -> bool {
    self.is_non_special_pattern_char(self.token_index, "]")
  }
}

// Ref: https://wicg.github.io/urlpattern/#parse-a-constructor-string
pub(crate) fn parse_constructor_string<R: RegExp>(
  input: &str,
) -> Result<UrlPatternInit, Error> {
  let token_list = crate::tokenizer::tokenize(
    input,
    crate::tokenizer::TokenizePolicy::Lenient,
  )?;

  let mut parser = ConstructorStringParser {
    input,
    token_list,
    result: UrlPatternInit {
      protocol: None,
      username: None,
      password: None,
      hostname: None,
      port: None,
      pathname: None,
      search: None,
      hash: None,
      base_url: None,
    },
    component_start: 0,
    token_index: 0,
    token_increment: 1,
    group_depth: 0,
    hostname_ipv6_bracket_depth: 0,
    protocol_matches_special_scheme: false,
    state: ConstructorStringParserState::Init,
  };

  while parser.token_index < parser.token_list.len() {
    parser.token_increment = 1;
    if parser.token_list[parser.token_index].kind == TokenType::End {
      if parser.state == ConstructorStringParserState::Init {
        parser.rewind();
        if parser.is_hash_prefix() {
          parser.change_state(ConstructorStringParserState::Hash, 1);
        } else if parser.is_search_prefix() {
          parser.change_state(ConstructorStringParserState::Search, 1);
        } else {
          parser.change_state(ConstructorStringParserState::Pathname, 0);
        }
        parser.token_index += parser.token_increment;
        continue;
      }
      if parser.state == ConstructorStringParserState::Authority {
        parser.rewind_and_set_state(ConstructorStringParserState::Hostname);
        parser.token_index += parser.token_increment;
        continue;
      }
      parser.change_state(ConstructorStringParserState::Done, 0);
      break;
    }
    if parser.is_group_open() {
      parser.group_depth += 1;
      parser.token_index += parser.token_increment;
      continue;
    }
    if parser.group_depth > 0 {
      if parser.is_group_close() {
        parser.group_depth -= 1;
      } else {
        parser.token_index += parser.token_increment;
        continue;
      }
    }
    match parser.state {
      ConstructorStringParserState::Init => {
        if parser.is_protocol_suffix() {
          parser.rewind_and_set_state(ConstructorStringParserState::Protocol);
        }
      }
      ConstructorStringParserState::Protocol => {
        if parser.is_protocol_suffix() {
          parser.compute_protocol_matches_special_scheme::<R>()?;
          let mut next_state = ConstructorStringParserState::Pathname;
          let mut skip = 1;
          if parser.next_is_authority_slashes() {
            next_state = ConstructorStringParserState::Authority;
            skip = 3;
          } else if parser.protocol_matches_special_scheme {
            next_state = ConstructorStringParserState::Authority;
          }
          parser.change_state(next_state, skip);
        }
      }
      ConstructorStringParserState::Authority => {
        if parser.is_identity_terminator() {
          parser.rewind_and_set_state(ConstructorStringParserState::Username);
        } else if parser.is_pathname_start()
          || parser.is_search_prefix()
          || parser.is_hash_prefix()
        {
          parser.rewind_and_set_state(ConstructorStringParserState::Hostname);
        }
      }
      ConstructorStringParserState::Username => {
        if parser.is_password_prefix() {
          parser.change_state(ConstructorStringParserState::Password, 1);
        } else if parser.is_identity_terminator() {
          parser.change_state(ConstructorStringParserState::Hostname, 1);
        }
      }
      ConstructorStringParserState::Password => {
        if parser.is_identity_terminator() {
          parser.change_state(ConstructorStringParserState::Hostname, 1);
        }
      }
      ConstructorStringParserState::Hostname => {
        if parser.is_ipv6_open() {
          parser.hostname_ipv6_bracket_depth += 1;
        } else if parser.is_ipv6_close() {
          parser.hostname_ipv6_bracket_depth -= 1;
        } else if parser.is_port_prefix()
          && parser.hostname_ipv6_bracket_depth == 0
        {
          parser.change_state(ConstructorStringParserState::Port, 1);
        } else if parser.is_pathname_start() {
          parser.change_state(ConstructorStringParserState::Pathname, 0);
        } else if parser.is_search_prefix() {
          parser.change_state(ConstructorStringParserState::Search, 1);
        } else if parser.is_hash_prefix() {
          parser.change_state(ConstructorStringParserState::Hash, 1);
        }
      }
      ConstructorStringParserState::Port => {
        if parser.is_pathname_start() {
          parser.change_state(ConstructorStringParserState::Pathname, 0);
        } else if parser.is_search_prefix() {
          parser.change_state(ConstructorStringParserState::Search, 1);
        } else if parser.is_hash_prefix() {
          parser.change_state(ConstructorStringParserState::Hash, 1);
        }
      }
      ConstructorStringParserState::Pathname => {
        if parser.is_search_prefix() {
          parser.change_state(ConstructorStringParserState::Search, 1);
        } else if parser.is_hash_prefix() {
          parser.change_state(ConstructorStringParserState::Hash, 1);
        }
      }
      ConstructorStringParserState::Search => {
        if parser.is_hash_prefix() {
          parser.change_state(ConstructorStringParserState::Hash, 1);
        }
      }
      ConstructorStringParserState::Hash => {}
      ConstructorStringParserState::Done => unreachable!(),
    }
    parser.token_index += parser.token_increment;
  }

  if parser.result.hostname.is_some() && parser.result.port.is_none() {
    parser.result.port = Some(String::new());
  }

  Ok(parser.result)
}
