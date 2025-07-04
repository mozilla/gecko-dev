use super::errors::{ErrorKind, ParserError};
use super::{core::Parser, core::Result, slice::Slice};
use crate::ast;

#[derive(Debug, PartialEq)]
enum TextElementTermination {
    LineFeed,
    Crlf,
    PlaceableStart,
    Eof,
}

// This enum tracks the placement of the text element in the pattern, which is needed for
// dedentation logic.
#[derive(Debug, PartialEq)]
enum TextElementPosition {
    InitialLineStart,
    LineStart,
    Continuation,
}

// This enum allows us to mark pointers in the source which will later become text elements
// but without slicing them out of the source string. This makes the indentation adjustments
// cheaper since they'll happen on the pointers, rather than extracted slices.
#[derive(Debug)]
enum PatternElementPlaceholders<S> {
    Placeable(ast::Expression<S>),
    // (start, end, indent, position)
    TextElement(usize, usize, usize, TextElementPosition),
}

// This enum tracks whether the text element is blank or not.
// This is important to identify text elements which should not be taken into account
// when calculating common indent.
#[derive(Debug, PartialEq)]
enum TextElementType {
    Blank,
    NonBlank,
}

impl<'s, S> Parser<S>
where
    S: Slice<'s>,
{
    pub(super) fn get_pattern(&mut self) -> Result<Option<ast::Pattern<S>>> {
        let mut elements = vec![];
        let mut last_non_blank = None;
        let mut common_indent = None;

        self.skip_blank_inline();

        let mut text_element_role = if self.skip_eol() {
            self.skip_blank_block();
            TextElementPosition::LineStart
        } else {
            TextElementPosition::InitialLineStart
        };

        while self.ptr < self.length {
            if self.take_byte_if(b'{') {
                if text_element_role == TextElementPosition::LineStart {
                    common_indent = Some(0);
                }
                let exp = self.get_placeable()?;
                last_non_blank = Some(elements.len());
                elements.push(PatternElementPlaceholders::Placeable(exp));
                text_element_role = TextElementPosition::Continuation;
            } else {
                let slice_start = self.ptr;
                let mut indent = 0;
                if text_element_role == TextElementPosition::LineStart {
                    indent = self.skip_blank_inline();
                    if let Some(b) = get_current_byte!(self) {
                        if indent == 0 {
                            if b != &b'\r' && b != &b'\n' {
                                break;
                            }
                        } else if !Self::is_byte_pattern_continuation(*b) {
                            self.ptr = slice_start;
                            break;
                        }
                    } else {
                        break;
                    }
                }
                let (start, end, text_element_type, termination_reason) = self.get_text_slice()?;
                if start != end {
                    if text_element_role == TextElementPosition::LineStart
                        && text_element_type == TextElementType::NonBlank
                    {
                        if let Some(common) = common_indent {
                            if indent < common {
                                common_indent = Some(indent);
                            }
                        } else {
                            common_indent = Some(indent);
                        }
                    }
                    if text_element_role != TextElementPosition::LineStart
                        || text_element_type == TextElementType::NonBlank
                        || termination_reason == TextElementTermination::LineFeed
                    {
                        if text_element_type == TextElementType::NonBlank {
                            last_non_blank = Some(elements.len());
                        }
                        elements.push(PatternElementPlaceholders::TextElement(
                            slice_start,
                            end,
                            indent,
                            text_element_role,
                        ));
                    }
                }

                text_element_role = match termination_reason {
                    TextElementTermination::LineFeed => TextElementPosition::LineStart,
                    TextElementTermination::Crlf => TextElementPosition::LineStart,
                    TextElementTermination::PlaceableStart => TextElementPosition::Continuation,
                    TextElementTermination::Eof => TextElementPosition::Continuation,
                };
            }
        }

        if let Some(last_non_blank) = last_non_blank {
            let elements = elements
                .into_iter()
                .take(last_non_blank + 1)
                .enumerate()
                .map(|(i, elem)| match elem {
                    PatternElementPlaceholders::Placeable(expression) => {
                        ast::PatternElement::Placeable { expression }
                    }
                    PatternElementPlaceholders::TextElement(start, end, indent, role) => {
                        let start = if role == TextElementPosition::LineStart {
                            common_indent.map_or_else(
                                || start + indent,
                                |common_indent| start + std::cmp::min(indent, common_indent),
                            )
                        } else {
                            start
                        };
                        let mut value = self.source.slice(start..end);
                        if last_non_blank == i {
                            value.trim();
                        }
                        ast::PatternElement::TextElement { value }
                    }
                })
                .collect();
            return Ok(Some(ast::Pattern { elements }));
        }

        Ok(None)
    }

    fn get_text_slice(
        &mut self,
    ) -> Result<(usize, usize, TextElementType, TextElementTermination)> {
        let start_pos = self.ptr;
        let Some(rest) = get_remaining_bytes!(self) else {
            return Ok((
                start_pos,
                self.ptr,
                TextElementType::Blank,
                TextElementTermination::Eof,
            ));
        };
        let end = memchr::memchr3(b'\n', b'{', b'}', rest);
        let element_type = |text: &[u8]| {
            if text.iter().any(|&c| c != b' ') {
                TextElementType::NonBlank
            } else {
                TextElementType::Blank
            }
        };
        match end.map(|p| &rest[..=p]) {
            Some([text @ .., b'}']) => {
                self.ptr += text.len();
                error!(ErrorKind::UnbalancedClosingBrace, self.ptr)
            }
            Some([text @ .., b'\r', b'\n']) => {
                self.ptr += text.len() + 1;
                Ok((
                    start_pos,
                    self.ptr - 1,
                    element_type(text),
                    TextElementTermination::Crlf,
                ))
            }
            Some([text @ .., b'\n']) => {
                self.ptr += text.len() + 1;
                Ok((
                    start_pos,
                    self.ptr,
                    element_type(text),
                    TextElementTermination::LineFeed,
                ))
            }
            Some([text @ .., b'{']) => {
                self.ptr += text.len();
                Ok((
                    start_pos,
                    self.ptr,
                    element_type(text),
                    TextElementTermination::PlaceableStart,
                ))
            }
            None => {
                self.ptr += rest.len();
                Ok((
                    start_pos,
                    self.ptr,
                    element_type(rest),
                    TextElementTermination::Eof,
                ))
            }
            _ => unreachable!(),
        }
    }
}
