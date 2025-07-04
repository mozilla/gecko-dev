//! Fluent Translation List serialization utilities
//!
//! This modules provides a way to serialize an abstract syntax tree representing a
//! Fluent Translation List. Use cases include normalization and programmatic
//! manipulation of a Fluent Translation List.
//!
//! # Example
//!
//! ```
//! use fluent_syntax::parser;
//! use fluent_syntax::serializer;
//!
//! let ftl = r#"# This is a message comment
//! hello-world = Hello World!
//! "#;
//!
//! let resource = parser::parse(ftl).expect("Failed to parse an FTL resource.");
//!
//! let serialized = serializer::serialize(&resource);
//!
//! assert_eq!(ftl, serialized);
//! ```

use crate::{ast::*, parser::matches_fluent_ws, parser::Slice};
use std::fmt::Write;

/// Serializes an abstract syntax tree representing a Fluent Translation List into a
/// String.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::serializer;
///
/// let ftl = r#"
/// unnormalized-message=This message has
///   abnormal spacing and indentation"#;
///
/// let resource = parser::parse(ftl).expect("Failed to parse an FTL resource.");
///
/// let serialized = serializer::serialize(&resource);
///
/// let expected = r#"unnormalized-message =
///     This message has
///     abnormal spacing and indentation
/// "#;
///
/// assert_eq!(expected, serialized);
/// ```
pub fn serialize<'s, S: Slice<'s>>(resource: &Resource<S>) -> String {
    serialize_with_options(resource, Options::default())
}

/// Serializes an abstract syntax tree representing a Fluent Translation List into a
/// String accepting custom options.
pub fn serialize_with_options<'s, S: Slice<'s>>(
    resource: &Resource<S>,
    options: Options,
) -> String {
    let mut ser = Serializer::new(options);
    ser.serialize_resource(resource);
    ser.into_serialized_text()
}

#[derive(Debug)]
struct Serializer {
    writer: TextWriter,
    options: Options,
    state: State,
}

impl Serializer {
    fn new(options: Options) -> Self {
        Serializer {
            writer: TextWriter::default(),
            options,
            state: State::default(),
        }
    }

    fn serialize_resource<'s, S: Slice<'s>>(&mut self, res: &Resource<S>) {
        for entry in &res.body {
            match entry {
                Entry::Message(msg) => self.serialize_message(msg),
                Entry::Term(term) => self.serialize_term(term),
                Entry::Comment(comment) => self.serialize_free_comment(comment, "#"),
                Entry::GroupComment(comment) => self.serialize_free_comment(comment, "##"),
                Entry::ResourceComment(comment) => self.serialize_free_comment(comment, "###"),
                Entry::Junk { content } => {
                    if self.options.with_junk {
                        self.serialize_junk(content.as_ref());
                    }
                }
            };

            self.state.wrote_non_junk_entry = !matches!(entry, Entry::Junk { .. });
        }
    }

    fn into_serialized_text(self) -> String {
        self.writer.buffer
    }

    fn serialize_junk(&mut self, junk: &str) {
        self.writer.write_literal(junk);
    }

    fn serialize_free_comment<'s, S: Slice<'s>>(&mut self, comment: &Comment<S>, prefix: &str) {
        if self.state.wrote_non_junk_entry {
            self.writer.newline();
        }
        self.serialize_comment(comment, prefix);
        self.writer.newline();
    }

    fn serialize_comment<'s, S: Slice<'s>>(&mut self, comment: &Comment<S>, prefix: &str) {
        for line in &comment.content {
            self.writer.write_literal(prefix);

            if !line.as_ref().trim_matches(matches_fluent_ws).is_empty() {
                self.writer.write_literal(" ");
                self.writer.write_literal(line.as_ref());
            }

            self.writer.newline();
        }
    }

    fn serialize_message<'s, S: Slice<'s>>(&mut self, msg: &Message<S>) {
        if let Some(comment) = msg.comment.as_ref() {
            self.serialize_comment(comment, "#");
        }

        self.writer.write_literal(msg.id.name.as_ref());
        self.writer.write_literal(" =");

        if let Some(value) = msg.value.as_ref() {
            self.serialize_pattern(value);
        }

        self.serialize_attributes(&msg.attributes);

        self.writer.newline();
    }

    fn serialize_term<'s, S: Slice<'s>>(&mut self, term: &Term<S>) {
        if let Some(comment) = term.comment.as_ref() {
            self.serialize_comment(comment, "#");
        }

        self.writer.write_literal("-");
        self.writer.write_literal(term.id.name.as_ref());
        self.writer.write_literal(" =");
        self.serialize_pattern(&term.value);

        self.serialize_attributes(&term.attributes);

        self.writer.newline();
    }

    fn serialize_pattern<'s, S: Slice<'s>>(&mut self, pattern: &Pattern<S>) {
        let start_on_newline = pattern.starts_on_new_line();

        if start_on_newline {
            self.writer.newline();
            self.writer.indent();
        } else {
            self.writer.write_literal(" ");
        }

        for element in &pattern.elements {
            self.serialize_element(element);
        }

        if start_on_newline {
            self.writer.dedent();
        }
    }

    fn serialize_attributes<'s, S: Slice<'s>>(&mut self, attrs: &[Attribute<S>]) {
        if attrs.is_empty() {
            return;
        }

        self.writer.indent();

        for attr in attrs {
            self.writer.newline();
            self.serialize_attribute(attr);
        }

        self.writer.dedent();
    }

    fn serialize_attribute<'s, S: Slice<'s>>(&mut self, attr: &Attribute<S>) {
        self.writer.write_literal(".");
        self.writer.write_literal(attr.id.name.as_ref());
        self.writer.write_literal(" =");

        self.serialize_pattern(&attr.value);
    }

    fn serialize_element<'s, S: Slice<'s>>(&mut self, elem: &PatternElement<S>) {
        match elem {
            PatternElement::TextElement { value } => self.writer.write_literal(value.as_ref()),
            PatternElement::Placeable { expression } => match expression {
                Expression::Inline(InlineExpression::Placeable { expression }) => {
                    // A placeable inside a placeable is a special case because we
                    // don't want the braces to look silly (e.g. "{ { Foo() } }").
                    self.writer.write_literal("{{ ");
                    self.serialize_expression(expression);
                    self.writer.write_literal(" }}");
                }
                Expression::Select { .. } => {
                    // select adds its own newline and indent, emit the brace
                    // *without* a space so we don't get 5 spaces instead of 4
                    self.writer.write_literal("{ ");
                    self.serialize_expression(expression);
                    self.writer.write_literal("}");
                }
                Expression::Inline(_) => {
                    self.writer.write_literal("{ ");
                    self.serialize_expression(expression);
                    self.writer.write_literal(" }");
                }
            },
        }
    }

    fn serialize_expression<'s, S: Slice<'s>>(&mut self, expr: &Expression<S>) {
        match expr {
            Expression::Inline(inline) => self.serialize_inline_expression(inline),
            Expression::Select { selector, variants } => {
                self.serialize_select_expression(selector, variants);
            }
        }
    }

    fn serialize_inline_expression<'s, S: Slice<'s>>(&mut self, expr: &InlineExpression<S>) {
        match expr {
            InlineExpression::StringLiteral { value } => {
                self.writer.write_literal("\"");
                self.writer.write_literal(value.as_ref());
                self.writer.write_literal("\"");
            }
            InlineExpression::NumberLiteral { value } => self.writer.write_literal(value.as_ref()),
            InlineExpression::VariableReference {
                id: Identifier { name: value },
            } => {
                self.writer.write_literal("$");
                self.writer.write_literal(value.as_ref());
            }
            InlineExpression::FunctionReference { id, arguments } => {
                self.writer.write_literal(id.name.as_ref());
                self.serialize_call_arguments(arguments);
            }
            InlineExpression::MessageReference { id, attribute } => {
                self.writer.write_literal(id.name.as_ref());

                if let Some(attr) = attribute.as_ref() {
                    self.writer.write_literal(".");
                    self.writer.write_literal(attr.name.as_ref());
                }
            }
            InlineExpression::TermReference {
                id,
                attribute,
                arguments,
            } => {
                self.writer.write_literal("-");
                self.writer.write_literal(id.name.as_ref());

                if let Some(attr) = attribute.as_ref() {
                    self.writer.write_literal(".");
                    self.writer.write_literal(attr.name.as_ref());
                }
                if let Some(args) = arguments.as_ref() {
                    self.serialize_call_arguments(args);
                }
            }
            InlineExpression::Placeable { expression } => {
                self.writer.write_literal("{");
                self.serialize_expression(expression);
                self.writer.write_literal("}");
            }
        }
    }

    fn serialize_select_expression<'s, S: Slice<'s>>(
        &mut self,
        selector: &InlineExpression<S>,
        variants: &[Variant<S>],
    ) {
        self.serialize_inline_expression(selector);
        self.writer.write_literal(" ->");

        self.writer.newline();
        self.writer.indent();

        for variant in variants {
            self.serialize_variant(variant);
            self.writer.newline();
        }

        self.writer.dedent();
    }

    fn serialize_variant<'s, S: Slice<'s>>(&mut self, variant: &Variant<S>) {
        if variant.default {
            self.writer.write_char_into_indent('*');
        }

        self.writer.write_literal("[");
        self.serialize_variant_key(&variant.key);
        self.writer.write_literal("]");
        self.serialize_pattern(&variant.value);
    }

    fn serialize_variant_key<'s, S: Slice<'s>>(&mut self, key: &VariantKey<S>) {
        match key {
            VariantKey::NumberLiteral { value } | VariantKey::Identifier { name: value } => {
                self.writer.write_literal(value.as_ref());
            }
        }
    }

    fn serialize_call_arguments<'s, S: Slice<'s>>(&mut self, args: &CallArguments<S>) {
        let mut argument_written = false;

        self.writer.write_literal("(");

        for positional in &args.positional {
            if argument_written {
                self.writer.write_literal(", ");
            }

            self.serialize_inline_expression(positional);
            argument_written = true;
        }

        for named in &args.named {
            if argument_written {
                self.writer.write_literal(", ");
            }

            self.writer.write_literal(named.name.name.as_ref());
            self.writer.write_literal(": ");
            self.serialize_inline_expression(&named.value);
            argument_written = true;
        }

        self.writer.write_literal(")");
    }
}

impl<'s, S: Slice<'s>> Pattern<S> {
    fn starts_on_new_line(&self) -> bool {
        !self.has_leading_text_dot() && self.is_multiline()
    }

    fn is_multiline(&self) -> bool {
        self.elements.iter().any(|elem| match elem {
            PatternElement::TextElement { value } => value.as_ref().contains('\n'),
            PatternElement::Placeable { expression } => is_select_expr(expression),
        })
    }

    fn has_leading_text_dot(&self) -> bool {
        if let Some(PatternElement::TextElement { value }) = self.elements.first() {
            value.as_ref().starts_with('.')
        } else {
            false
        }
    }
}

fn is_select_expr<'s, S: Slice<'s>>(expr: &Expression<S>) -> bool {
    match expr {
        Expression::Select { .. } => true,
        Expression::Inline(InlineExpression::Placeable { expression }) => {
            is_select_expr(expression)
        }
        Expression::Inline(_) => false,
    }
}

/// Options for serializing an abstract syntax tree.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct Options {
    /// Whether invalid text fragments should be serialized, too.
    pub with_junk: bool,
}

#[derive(Debug, Default, PartialEq)]
struct State {
    wrote_non_junk_entry: bool,
}

#[derive(Clone, Debug, Default)]
struct TextWriter {
    buffer: String,
    indent_level: usize,
}

impl TextWriter {
    fn indent(&mut self) {
        self.indent_level += 1;
    }

    fn dedent(&mut self) {
        self.indent_level = self
            .indent_level
            .checked_sub(1)
            .expect("Dedenting without a corresponding indent");
    }

    fn write_indent(&mut self) {
        for _ in 0..self.indent_level {
            self.buffer.push_str("    ");
        }
    }

    fn newline(&mut self) {
        if self.buffer.ends_with('\r') {
            // handle rare edge case, where the trailing `\r` would get confused
            // as part of the line ending
            self.buffer.push('\r');
        }
        self.buffer.push('\n');
    }

    fn write_literal(&mut self, item: &str) {
        if self.buffer.ends_with('\n') {
            // we've just added a newline, make sure it's properly indented
            self.write_indent();
        }

        write!(self.buffer, "{}", item).expect("Writing to an in-memory buffer never fails");
    }

    fn write_char_into_indent(&mut self, ch: char) {
        if self.buffer.ends_with('\n') {
            self.write_indent();
        }
        self.buffer.pop();
        self.buffer.push(ch);
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::parser::parse;

    #[test]
    fn write_something_then_indent() {
        let mut writer = TextWriter::default();

        writer.write_literal("foo =");
        writer.newline();
        writer.indent();
        writer.write_literal("first line");
        writer.newline();
        writer.write_literal("second line");
        writer.newline();
        writer.dedent();
        writer.write_literal("not indented");
        writer.newline();

        let got = &writer.buffer;
        assert_eq!(
            got,
            "foo =\n    first line\n    second line\nnot indented\n"
        );
    }

    macro_rules! text_message {
        ($name:expr, $value:expr) => {
            Entry::Message(Message {
                id: Identifier { name: $name },
                value: Some(Pattern {
                    elements: vec![PatternElement::TextElement { value: $value }],
                }),
                attributes: vec![],
                comment: None,
            })
        };
    }

    impl<'a> Entry<&'a str> {
        fn as_message(&mut self) -> &mut Message<&'a str> {
            match self {
                Self::Message(msg) => msg,
                _ => panic!("Expected Message"),
            }
        }
    }

    impl<'a> Message<&'a str> {
        fn as_pattern(&mut self) -> &mut Pattern<&'a str> {
            self.value.as_mut().expect("Expected Pattern")
        }
    }

    impl<'a> PatternElement<&'a str> {
        fn as_text(&mut self) -> &mut &'a str {
            match self {
                Self::TextElement { value } => value,
                _ => panic!("Expected TextElement"),
            }
        }

        fn as_expression(&mut self) -> &mut Expression<&'a str> {
            match self {
                Self::Placeable { expression } => expression,
                _ => panic!("Expected Placeable"),
            }
        }
    }

    impl<'a> Expression<&'a str> {
        fn as_variants(&mut self) -> &mut Vec<Variant<&'a str>> {
            match self {
                Self::Select { variants, .. } => variants,
                _ => panic!("Expected Select"),
            }
        }
        fn as_inline_variable_id(&mut self) -> &mut Identifier<&'a str> {
            match self {
                Self::Inline(InlineExpression::VariableReference { id }) => id,
                _ => panic!("Expected Inline"),
            }
        }
    }

    #[test]
    fn change_id() {
        let mut ast = parse("foo = bar\n").expect("failed to parse ftl resource");
        ast.body[0].as_message().id.name = "baz";
        assert_eq!(serialize(&ast), "baz = bar\n");
    }

    #[test]
    fn change_value() {
        let mut ast = parse("foo = bar\n").expect("failed to parse ftl resource");
        *ast.body[0].as_message().as_pattern().elements[0].as_text() = "baz";
        assert_eq!("foo = baz\n", serialize(&ast));
    }

    #[test]
    fn add_expression_variant() {
        let message = concat!(
            "foo =\n",
            "    { $num ->\n",
            "       *[other] { $num } bars\n",
            "    }\n"
        );
        let mut ast = parse(message).expect("failed to parse ftl resource");

        let one_variant = Variant {
            key: VariantKey::Identifier { name: "one" },
            value: Pattern {
                elements: vec![
                    PatternElement::Placeable {
                        expression: Expression::Inline(InlineExpression::VariableReference {
                            id: Identifier { name: "num" },
                        }),
                    },
                    PatternElement::TextElement { value: " bar" },
                ],
            },
            default: false,
        };
        ast.body[0].as_message().as_pattern().elements[0]
            .as_expression()
            .as_variants()
            .insert(0, one_variant);

        let expected = concat!(
            "foo =\n",
            "    { $num ->\n",
            "        [one] { $num } bar\n",
            "       *[other] { $num } bars\n",
            "    }\n"
        );
        assert_eq!(serialize(&ast), expected);
    }

    #[test]
    fn change_variable_reference() {
        let mut ast = parse("foo = { $bar }\n").expect("failed to parse ftl resource");
        ast.body[0].as_message().as_pattern().elements[0]
            .as_expression()
            .as_inline_variable_id()
            .name = "qux";
        assert_eq!("foo = { $qux }\n", serialize(&ast));
    }

    #[test]
    fn remove_message() {
        let mut ast = parse("foo = bar\nbaz = qux\n").expect("failed to parse ftl resource");
        ast.body.pop();
        assert_eq!("foo = bar\n", serialize(&ast));
    }

    #[test]
    fn add_message_at_top() {
        let mut ast = parse("foo = bar\n").expect("failed to parse ftl resource");
        ast.body.insert(0, text_message!("baz", "qux"));
        assert_eq!("baz = qux\nfoo = bar\n", serialize(&ast));
    }

    #[test]
    fn add_message_at_end() {
        let mut ast = parse("foo = bar\n").expect("failed to parse ftl resource");
        ast.body.push(text_message!("baz", "qux"));
        assert_eq!("foo = bar\nbaz = qux\n", serialize(&ast));
    }

    #[test]
    fn add_message_in_between() {
        let mut ast = parse("foo = bar\nbaz = qux\n").expect("failed to parse ftl resource");
        ast.body.insert(1, text_message!("hello", "there"));
        assert_eq!("foo = bar\nhello = there\nbaz = qux\n", serialize(&ast));
    }

    #[test]
    fn add_message_comment() {
        let mut ast = parse("foo = bar\n").expect("failed to parse ftl resource");
        ast.body[0].as_message().comment.replace(Comment {
            content: vec!["great message!"],
        });
        assert_eq!("# great message!\nfoo = bar\n", serialize(&ast));
    }

    #[test]
    fn remove_message_comment() {
        let mut ast = parse("# great message!\nfoo = bar\n").expect("failed to parse ftl resource");
        ast.body[0].as_message().comment.take();
        assert_eq!("foo = bar\n", serialize(&ast));
    }

    #[test]
    fn edit_message_comment() {
        let mut ast = parse("# great message!\nfoo = bar\n").expect("failed to parse ftl resource");
        ast.body[0]
            .as_message()
            .comment
            .as_mut()
            .expect("comment is missing")
            .content[0] = "very original";
        assert_eq!("# very original\nfoo = bar\n", serialize(&ast));
    }
}
