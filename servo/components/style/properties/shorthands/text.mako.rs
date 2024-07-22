/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />

<%helpers:shorthand name="text-decoration"
                    engines="gecko servo"
                    sub_properties="text-decoration-color text-decoration-line text-decoration-style
                    ${' text-decoration-thickness' if engine == 'gecko' else ''}"
                    spec="https://drafts.csswg.org/css-text-decor/#propdef-text-decoration">
    use crate::properties::longhands::{text_decoration_color, text_decoration_line, text_decoration_style};

    % if engine == "gecko":
        use crate::properties::longhands::text_decoration_thickness;
    % endif

    pub fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Longhands, ParseError<'i>> {
        let (mut line, mut style, mut color, mut any) = (None, None, None, false);
        % if engine == "gecko":
            let mut thickness = None;
        % endif

        loop {
            macro_rules! parse_component {
                ($value:ident, $module:ident) => (
                    if $value.is_none() {
                        if let Ok(value) = input.try_parse(|input| $module::parse(context, input)) {
                            $value = Some(value);
                            any = true;
                            continue;
                        }
                    }
                )
            }

            parse_component!(line, text_decoration_line);
            parse_component!(style, text_decoration_style);
            parse_component!(color, text_decoration_color);

            % if engine == "gecko":
                parse_component!(thickness, text_decoration_thickness);
            % endif

            break;
        }

        if !any {
            return Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError));
        }

        Ok(expanded! {
            text_decoration_line: unwrap_or_initial!(text_decoration_line, line),
            text_decoration_style: unwrap_or_initial!(text_decoration_style, style),
            text_decoration_color: unwrap_or_initial!(text_decoration_color, color),

            % if engine == "gecko":
                text_decoration_thickness: unwrap_or_initial!(text_decoration_thickness, thickness),
            % endif
        })
    }

    impl<'a> ToCss for LonghandsToSerialize<'a>  {
        #[allow(unused)]
        fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result where W: fmt::Write {
            use crate::values::specified::TextDecorationLine;
            use crate::values::specified::Color;

            let (is_solid_style, is_current_color) =
            (
                *self.text_decoration_style == text_decoration_style::SpecifiedValue::Solid,
                *self.text_decoration_color == Color::CurrentColor,
            );

            % if engine == "gecko":
                let is_auto_thickness = self.text_decoration_thickness.is_auto();
            % else:
                let is_auto_thickness = true;
            % endif


            let mut has_value = false;
            let is_none = *self.text_decoration_line == TextDecorationLine::none();
            if (is_solid_style && is_current_color && is_auto_thickness) || !is_none {
                self.text_decoration_line.to_css(dest)?;
                has_value = true;
            }

            % if engine == "gecko":
            if !is_auto_thickness {
                if has_value {
                    dest.write_char(' ')?;
                }
                self.text_decoration_thickness.to_css(dest)?;
                has_value = true;
            }
            % endif

            if !is_solid_style {
                if has_value {
                    dest.write_char(' ')?;
                }
                self.text_decoration_style.to_css(dest)?;
                has_value = true;
            }

            if !is_current_color {
                if has_value {
                    dest.write_char(' ')?;
                }
                self.text_decoration_color.to_css(dest)?;
                has_value = true;
            }

            Ok(())
        }
    }
</%helpers:shorthand>
