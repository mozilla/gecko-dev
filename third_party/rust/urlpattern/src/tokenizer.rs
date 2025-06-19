// Copyright 2018-2021 the Deno authors. All rights reserved. MIT license.

use crate::error::TokenizerError;
use crate::Error;

// Ref: https://wicg.github.io/urlpattern/#tokens
// Ref: https://wicg.github.io/urlpattern/#tokenizing

// Ref: https://wicg.github.io/urlpattern/#token-type
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum TokenType {
  Open,
  Close,
  Regexp,
  Name,
  Char,
  EscapedChar,
  OtherModifier,
  Asterisk,
  End,
  InvalidChar,
}

// Ref: https://wicg.github.io/urlpattern/#token
#[derive(Debug, Clone)]
pub struct Token {
  pub kind: TokenType,
  pub index: usize,
  pub value: String,
}

// Ref: https://wicg.github.io/urlpattern/#tokenize-policy
#[derive(Debug, Eq, PartialEq)]
pub enum TokenizePolicy {
  Strict,
  Lenient,
}

// Ref: https://wicg.github.io/urlpattern/#tokenizer
struct Tokenizer {
  input: Vec<char>,
  policy: TokenizePolicy,
  token_list: Vec<Token>,
  index: usize,
  next_index: usize,
  code_point: Option<char>, // TODO: get rid of Option
}

impl Tokenizer {
  // Ref: https://wicg.github.io/urlpattern/#get-the-next-code-point
  #[inline]
  fn get_next_codepoint(&mut self) {
    self.code_point = Some(self.input[self.next_index]);
    self.next_index += 1;
  }

  // Ref: https://wicg.github.io/urlpattern/#add-a-token-with-default-position-and-length
  #[inline]
  fn add_token_with_default_pos_and_len(&mut self, kind: TokenType) {
    self.add_token_with_default_len(kind, self.next_index, self.index);
  }

  // Ref: https://wicg.github.io/urlpattern/#add-a-token-with-default-length
  #[inline]
  fn add_token_with_default_len(
    &mut self,
    kind: TokenType,
    next_pos: usize,
    value_pos: usize,
  ) {
    self.add_token(kind, next_pos, value_pos, next_pos - value_pos);
  }

  // Ref: https://wicg.github.io/urlpattern/#add-a-token
  #[inline]
  fn add_token(
    &mut self,
    kind: TokenType,
    next_pos: usize,
    value_pos: usize,
    value_len: usize,
  ) {
    let range = value_pos..(value_pos + value_len);
    let value = self.input[range].iter().collect::<String>();
    self.token_list.push(Token {
      kind,
      index: self.index,
      value,
    });
    self.index = next_pos;
  }

  // Ref: https://wicg.github.io/urlpattern/#process-a-tokenizing-error
  fn process_tokenizing_error(
    &mut self,
    next_pos: usize,
    value_pos: usize,
    error: TokenizerError,
  ) -> Result<(), Error> {
    if self.policy == TokenizePolicy::Strict {
      Err(Error::Tokenizer(error, value_pos))
    } else {
      self.add_token_with_default_len(
        TokenType::InvalidChar,
        next_pos,
        value_pos,
      );
      Ok(())
    }
  }

  // Ref: https://wicg.github.io/urlpattern/#seek-and-get-the-next-code-point
  #[inline]
  fn seek_and_get_next_codepoint(&mut self, index: usize) {
    self.next_index = index;
    self.get_next_codepoint();
  }
}

// Ref: https://wicg.github.io/urlpattern/#tokenize
pub fn tokenize(
  input: &str,
  policy: TokenizePolicy,
) -> Result<Vec<Token>, Error> {
  let mut tokenizer = Tokenizer {
    input: input.chars().collect::<Vec<char>>(),
    policy,
    token_list: vec![],
    index: 0,
    next_index: 0,
    code_point: None,
  };

  while tokenizer.index < tokenizer.input.len() {
    tokenizer.seek_and_get_next_codepoint(tokenizer.index);

    if tokenizer.code_point == Some('*') {
      tokenizer.add_token_with_default_pos_and_len(TokenType::Asterisk);
      continue;
    }
    if matches!(tokenizer.code_point, Some('+') | Some('?')) {
      tokenizer.add_token_with_default_pos_and_len(TokenType::OtherModifier);
      continue;
    }
    if tokenizer.code_point == Some('\\') {
      if tokenizer.index == (tokenizer.input.len() - 1) {
        tokenizer.process_tokenizing_error(
          tokenizer.next_index,
          tokenizer.index,
          TokenizerError::IncompleteEscapeCode,
        )?;
        continue;
      }
      let escaped_index = tokenizer.next_index;
      tokenizer.get_next_codepoint();
      tokenizer.add_token_with_default_len(
        TokenType::EscapedChar,
        tokenizer.next_index,
        escaped_index,
      );
      continue;
    }
    if tokenizer.code_point == Some('{') {
      tokenizer.add_token_with_default_pos_and_len(TokenType::Open);
      continue;
    }
    if tokenizer.code_point == Some('}') {
      tokenizer.add_token_with_default_pos_and_len(TokenType::Close);
      continue;
    }
    if tokenizer.code_point == Some(':') {
      let mut name_pos = tokenizer.next_index;
      let name_start = name_pos;
      while name_pos < tokenizer.input.len() {
        tokenizer.seek_and_get_next_codepoint(name_pos);
        let first_code_point = name_pos == name_start;
        let valid_codepoint = is_valid_name_codepoint(
          tokenizer.code_point.unwrap(),
          first_code_point,
        );
        if !valid_codepoint {
          break;
        }
        name_pos = tokenizer.next_index;
      }
      if name_pos <= name_start {
        tokenizer.process_tokenizing_error(
          name_start,
          tokenizer.index,
          TokenizerError::InvalidName,
        )?;
        continue;
      }
      tokenizer.add_token_with_default_len(
        TokenType::Name,
        name_pos,
        name_start,
      );
      continue;
    }

    if tokenizer.code_point == Some('(') {
      let mut depth = 1;
      let mut regexp_pos = tokenizer.next_index;
      let regexp_start = regexp_pos;
      let mut error = false;
      // TODO: input code point length
      while regexp_pos < tokenizer.input.len() {
        tokenizer.seek_and_get_next_codepoint(regexp_pos);
        if !tokenizer.code_point.unwrap().is_ascii()
          || (regexp_pos == regexp_start && tokenizer.code_point == Some('?'))
        {
          tokenizer.process_tokenizing_error(
            regexp_start,
            tokenizer.index,
            TokenizerError::InvalidRegex(
              "must not start with ?, and may only contain ascii",
            ),
          )?;
          error = true;
          break;
        }
        if tokenizer.code_point == Some('\\') {
          if regexp_pos == (tokenizer.input.len() - 1) {
            tokenizer.process_tokenizing_error(
              regexp_start,
              tokenizer.index,
              TokenizerError::IncompleteEscapeCode,
            )?;
            error = true;
            break;
          }
          tokenizer.get_next_codepoint();
          if !tokenizer.code_point.unwrap().is_ascii() {
            tokenizer.process_tokenizing_error(
              regexp_start,
              tokenizer.index,
              TokenizerError::InvalidRegex("non ascii character was escaped"),
            )?;
            error = true;
            break;
          }
          regexp_pos = tokenizer.next_index;
          continue;
        }
        if tokenizer.code_point == Some(')') {
          depth -= 1;
          if depth == 0 {
            regexp_pos = tokenizer.next_index;
            break;
          }
        } else if tokenizer.code_point == Some('(') {
          depth += 1;
          if regexp_pos == (tokenizer.input.len() - 1) {
            tokenizer.process_tokenizing_error(
              regexp_start,
              tokenizer.index,
              TokenizerError::InvalidRegex("nested groups not closed"),
            )?;
            error = true;
            break;
          }
          let temp_pos = tokenizer.next_index;
          tokenizer.get_next_codepoint();
          if tokenizer.code_point != Some('?') {
            tokenizer.process_tokenizing_error(
              regexp_start,
              tokenizer.index,
              TokenizerError::InvalidRegex("nested groups must start with ?"),
            )?;
            error = true;
            break;
          }
          tokenizer.next_index = temp_pos;
        }
        regexp_pos = tokenizer.next_index;
      }
      if error {
        continue;
      }
      if depth != 0 {
        tokenizer.process_tokenizing_error(
          regexp_start,
          tokenizer.index,
          TokenizerError::InvalidRegex("missing closing )"),
        )?;
        continue;
      }
      let regexp_len = regexp_pos - regexp_start - 1;
      if regexp_len == 0 {
        tokenizer.process_tokenizing_error(
          regexp_start,
          tokenizer.index,
          TokenizerError::InvalidRegex("length must be > 0"),
        )?;
        continue;
      }
      tokenizer.add_token(
        TokenType::Regexp,
        regexp_pos,
        regexp_start,
        regexp_len,
      );
      continue;
    }

    tokenizer.add_token_with_default_pos_and_len(TokenType::Char);
  }

  tokenizer.add_token_with_default_len(
    TokenType::End,
    tokenizer.index,
    tokenizer.index,
  );
  Ok(tokenizer.token_list)
}

// Ref: https://wicg.github.io/urlpattern/#is-a-valid-name-code-point
#[inline]
pub(crate) fn is_valid_name_codepoint(code_point: char, first: bool) -> bool {
  if first {
    unic_ucd_ident::is_id_start(code_point) || matches!(code_point, '$' | '_')
  } else {
    unic_ucd_ident::is_id_continue(code_point)
      || matches!(code_point, '$' | '\u{200C}' | '\u{200D}')
  }
}
