// Copyright 2018-2021 the Deno authors. All rights reserved. MIT license.

use crate::canonicalize_and_process::escape_pattern_string;
use crate::matcher::InnerMatcher;
use crate::matcher::Matcher;
use crate::parser::Options;
use crate::parser::Part;
use crate::parser::PartModifier;
use crate::parser::PartType;
use crate::parser::FULL_WILDCARD_REGEXP_VALUE;
use crate::regexp::RegExp;
use crate::tokenizer::is_valid_name_codepoint;
use crate::Error;
use std::fmt::Write;

// Ref: https://wicg.github.io/urlpattern/#component
#[derive(Debug)]
pub(crate) struct Component<R: RegExp> {
  pub pattern_string: String,
  pub regexp: Result<R, Error>,
  pub group_name_list: Vec<String>,
  pub matcher: Matcher<R>,
  pub has_regexp_group: bool,
}

impl<R: RegExp> Component<R> {
  // Ref: https://wicg.github.io/urlpattern/#compile-a-component
  pub(crate) fn compile<F>(
    input: Option<&str>,
    encoding_callback: F,
    options: Options,
  ) -> Result<Self, Error>
  where
    F: Fn(&str) -> Result<String, Error>,
  {
    let part_list = crate::parser::parse_pattern_string(
      input.unwrap_or("*"),
      &options,
      encoding_callback,
    )?;
    let part_list = part_list.iter().collect::<Vec<_>>();
    let (regexp_string, name_list) =
      generate_regular_expression_and_name_list(&part_list, &options);
    let flags = if options.ignore_case { "ui" } else { "u" };
    let regexp = R::parse(&regexp_string, flags).map_err(Error::RegExp);
    let pattern_string = generate_pattern_string(&part_list, &options);
    let matcher = generate_matcher::<R>(&part_list, &options, flags);
    Ok(Component {
      pattern_string,
      regexp,
      group_name_list: name_list,
      matcher,
      has_regexp_group: part_list
        .iter()
        .any(|part| part.kind == PartType::Regexp),
    })
  }

  // Ref: https://wicg.github.io/urlpattern/#protocol-component-matches-a-special-scheme
  pub(crate) fn protocol_component_matches_special_scheme(&self) -> bool {
    const SPECIAL_SCHEMES: [&str; 6] =
      ["ftp", "file", "http", "https", "ws", "wss"];
    if let Ok(regex) = &self.regexp {
      for scheme in SPECIAL_SCHEMES {
        if regex.matches(scheme).is_some() {
          return true;
        }
      }
    }
    false
  }

  // Ref: https://wicg.github.io/urlpattern/#create-a-component-match-result
  pub(crate) fn create_match_result(
    &self,
    input: String,
    exec_result: Vec<Option<&str>>,
  ) -> crate::UrlPatternComponentResult {
    let groups = self
      .group_name_list
      .clone()
      .into_iter()
      .zip(exec_result.into_iter().map(|s| s.map(str::to_owned)))
      .collect();
    crate::UrlPatternComponentResult { input, groups }
  }

  pub(crate) fn optionally_transpose_regex_error(
    mut self,
    do_transpose: bool,
  ) -> Result<Self, Error> {
    if do_transpose {
      self.regexp = Ok(self.regexp?);
    }
    Ok(self)
  }
}

// Ref: https://wicg.github.io/urlpattern/#generate-a-regular-expression-and-name-list
fn generate_regular_expression_and_name_list(
  part_list: &[&Part],
  options: &Options,
) -> (String, Vec<String>) {
  let mut result = String::from("^");
  let mut name_list = vec![];
  for part in part_list {
    if part.kind == PartType::FixedText {
      if part.modifier == PartModifier::None {
        result.push_str(&options.escape_regexp_string(&part.value));
      } else {
        write!(
          result,
          "(?:{}){}",
          options.escape_regexp_string(&part.value),
          part.modifier
        )
        .unwrap();
      }
      continue;
    }

    assert!(!part.name.is_empty());
    name_list.push(part.name.clone());
    let regexp_value = if part.kind == PartType::SegmentWildcard {
      options.generate_segment_wildcard_regexp()
    } else if part.kind == PartType::FullWildcard {
      FULL_WILDCARD_REGEXP_VALUE.to_string()
    } else {
      part.value.clone()
    };

    if part.prefix.is_empty() && part.suffix.is_empty() {
      if matches!(part.modifier, PartModifier::None | PartModifier::Optional) {
        write!(result, "({}){}", regexp_value, part.modifier).unwrap();
      } else {
        write!(result, "((?:{}){})", regexp_value, part.modifier).unwrap();
      }
      continue;
    }
    if matches!(part.modifier, PartModifier::None | PartModifier::Optional) {
      write!(
        result,
        "(?:{}({}){}){}",
        options.escape_regexp_string(&part.prefix),
        regexp_value,
        options.escape_regexp_string(&part.suffix),
        part.modifier
      )
      .unwrap();
      continue;
    }
    assert!(!part.prefix.is_empty() || !part.suffix.is_empty());
    write!(
      result,
      "(?:{}((?:{})(?:{}{}(?:{}))*){}){}",
      options.escape_regexp_string(&part.prefix),
      regexp_value,
      options.escape_regexp_string(&part.suffix),
      options.escape_regexp_string(&part.prefix),
      regexp_value,
      options.escape_regexp_string(&part.suffix),
      if part.modifier == PartModifier::ZeroOrMore {
        "?"
      } else {
        ""
      }
    )
    .unwrap();
  }
  result.push('$');
  (result, name_list)
}

// Ref: https://wicg.github.io/urlpattern/#generate-a-pattern-string
fn generate_pattern_string(part_list: &[&Part], options: &Options) -> String {
  let mut result = String::new();
  for (i, part) in part_list.iter().enumerate() {
    let prev_part: Option<&Part> = if i == 0 {
      None
    } else {
      part_list.get(i - 1).copied()
    };
    let next_part: Option<&Part> = part_list.get(i + 1).copied();
    if part.kind == PartType::FixedText {
      if part.modifier == PartModifier::None {
        result.push_str(&escape_pattern_string(&part.value));
        continue;
      }
      write!(
        result,
        "{{{}}}{}",
        escape_pattern_string(&part.value),
        part.modifier
      )
      .unwrap();
      continue;
    }
    let custom_name = !part.name.chars().next().unwrap().is_ascii_digit();
    let mut needs_grouping = !part.suffix.is_empty()
      || (!part.prefix.is_empty() && part.prefix != options.prefix_code_point);
    if !needs_grouping
      && custom_name
      && part.kind == PartType::SegmentWildcard
      && part.modifier == PartModifier::None
      && matches!(next_part, Some(Part { prefix, suffix, .. }) if prefix.is_empty() && suffix.is_empty())
    {
      let next_part = next_part.unwrap();
      if next_part.kind == PartType::FixedText {
        needs_grouping = is_valid_name_codepoint(
          next_part.value.chars().next().unwrap_or_default(),
          false,
        );
      } else {
        needs_grouping =
          next_part.name.chars().next().unwrap().is_ascii_digit();
      }
    }
    if !needs_grouping
      && part.prefix.is_empty()
      && matches!(
        prev_part,
        Some(Part {
          kind: PartType::FixedText,
          value,
          ..
        }) if value.chars().last().unwrap().to_string() == options.prefix_code_point
      )
    {
      needs_grouping = true;
    }
    assert!(!part.name.is_empty());
    if needs_grouping {
      result.push('{');
    }
    result.push_str(&escape_pattern_string(&part.prefix));
    if custom_name {
      result.push(':');
      result.push_str(&part.name);
    }
    match part.kind {
      PartType::FixedText => unreachable!(),
      PartType::Regexp => write!(result, "({})", part.value).unwrap(),
      PartType::SegmentWildcard if !custom_name => {
        write!(result, "({})", options.generate_segment_wildcard_regexp())
          .unwrap()
      }
      PartType::SegmentWildcard => {}
      PartType::FullWildcard => {
        if !custom_name
          && (prev_part.is_none()
            || prev_part.unwrap().kind == PartType::FixedText
            || prev_part.unwrap().modifier != PartModifier::None
            || needs_grouping
            || !part.prefix.is_empty())
        {
          result.push('*');
        } else {
          result.push_str(&format!("({FULL_WILDCARD_REGEXP_VALUE})"));
        }
      }
    }
    if part.kind == PartType::SegmentWildcard
      && custom_name
      && !part.suffix.is_empty()
      && is_valid_name_codepoint(part.suffix.chars().next().unwrap(), false)
    {
      result.push('\\');
    }
    result.push_str(&escape_pattern_string(&part.suffix));
    if needs_grouping {
      result.push('}');
    }
    result.push_str(&part.modifier.to_string());
  }
  result
}

/// This function generates a matcher for a given parts list.
fn generate_matcher<R: RegExp>(
  mut part_list: &[&Part],
  options: &Options,
  flags: &str,
) -> Matcher<R> {
  fn is_literal(part: &Part) -> bool {
    part.kind == PartType::FixedText && part.modifier == PartModifier::None
  }

  // If the first part is a fixed string, we can use it as a literal prefix.
  let mut prefix = match part_list.first() {
    Some(part) if is_literal(part) => {
      part_list = &part_list[1..];
      part.value.clone()
    }
    _ => "".into(),
  };
  // If the last part is a fixed string, we can use it as a literal suffix.
  let mut suffix = match part_list.last() {
    Some(part) if is_literal(part) => {
      part_list = &part_list[..part_list.len() - 1];
      part.value.clone()
    }
    _ => "".into(),
  };

  // If there are no more parts, we must have a prefix and/or a suffix. We can
  // combine these into a single fixed text literal matcher.
  if part_list.is_empty() {
    return Matcher {
      prefix: "".to_string(),
      suffix: "".to_string(),
      inner: InnerMatcher::Literal {
        literal: format!("{prefix}{suffix}"),
      },
      ignore_case: options.ignore_case,
    };
  }

  let inner = match part_list {
    // If there is only one part, and it is a simple full wildcard with no
    // prefix or suffix, we can use a simple wildcard matcher.
    [part]
      if part.kind == PartType::FullWildcard
        && part.modifier == PartModifier::None =>
    {
      prefix += &part.prefix;
      if !part.suffix.is_empty() {
        suffix = format!("{}{suffix}", part.suffix);
      }
      InnerMatcher::SingleCapture {
        filter: None,
        allow_empty: true,
      }
    }
    // If there is only one part, and it is a simple segment wildcard with no
    // prefix or suffix, we can use a simple wildcard matcher.
    [part]
      if part.kind == PartType::SegmentWildcard
        && part.modifier == PartModifier::None =>
    {
      prefix += &part.prefix;
      if !part.suffix.is_empty() {
        suffix = format!("{}{suffix}", part.suffix);
      }
      InnerMatcher::SingleCapture {
        filter: options.delimiter_code_point,
        allow_empty: false,
      }
    }
    // For all other cases, we fall back to a regexp matcher.
    part_list => {
      let (regexp_string, _) =
        generate_regular_expression_and_name_list(part_list, options);
      let regexp = R::parse(&regexp_string, flags).map_err(Error::RegExp);
      InnerMatcher::RegExp { regexp }
    }
  };

  Matcher {
    prefix,
    suffix,
    inner,
    ignore_case: options.ignore_case,
  }
}
