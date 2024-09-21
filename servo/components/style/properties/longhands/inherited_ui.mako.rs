/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />

${helpers.predefined_type(
    "cursor",
    "Cursor",
    "computed::Cursor::auto()",
    engines="gecko servo",
    initial_specified_value="specified::Cursor::auto()",
    animation_type="discrete",
    spec="https://drafts.csswg.org/css-ui/#cursor",
    affects="paint",
)}

${helpers.predefined_type(
    "pointer-events",
    "PointerEvents",
    "specified::PointerEvents::Auto",
    engines="gecko servo",
    animation_type="discrete",
    spec="https://svgwg.org/svg2-draft/interact.html#PointerEventsProperty",
    affects="paint",
)}

${helpers.predefined_type(
    "-moz-inert",
    "Inert",
    "specified::Inert::None",
    engines="gecko",
    gecko_ffi_name="mInert",
    animation_type="discrete",
    enabled_in="ua",
    spec="Nonstandard (https://html.spec.whatwg.org/multipage/#inert-subtrees)",
    affects="paint",
)}

${helpers.predefined_type(
    "-moz-user-input",
    "UserInput",
    "specified::UserInput::Auto",
    engines="gecko",
    gecko_ffi_name="mUserInput",
    animation_type="discrete",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/-moz-user-input)",
    affects="",
)}

${helpers.predefined_type(
    "-moz-user-modify",
    "UserModify",
    "specified::UserModify::ReadOnly",
    engines="gecko",
    gecko_ffi_name="mUserModify",
    animation_type="discrete",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/-moz-user-modify)",
    affects="",
)}

${helpers.predefined_type(
    "-moz-user-focus",
    "UserFocus",
    "specified::UserFocus::Normal",
    engines="gecko",
    gecko_ffi_name="mUserFocus",
    animation_type="discrete",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/-moz-user-focus)",
    enabled_in="chrome",
    affects="",
)}

${helpers.predefined_type(
    "caret-color",
    "color::CaretColor",
    "generics::color::CaretColor::auto()",
    engines="gecko",
    spec="https://drafts.csswg.org/css-ui/#caret-color",
    ignored_when_colors_disabled=True,
    affects="paint",
)}

${helpers.predefined_type(
    "accent-color",
    "ColorOrAuto",
    "generics::color::ColorOrAuto::Auto",
    engines="gecko",
    spec="https://drafts.csswg.org/css-ui-4/#widget-accent",
    ignored_when_colors_disabled=True,
    affects="paint",
)}

${helpers.predefined_type(
    "color-scheme",
    "ColorScheme",
    "specified::color::ColorScheme::normal()",
    engines="gecko",
    spec="https://drafts.csswg.org/css-color-adjust/#color-scheme-prop",
    animation_type="discrete",
    ignored_when_colors_disabled=True,
    affects="paint",
)}

${helpers.predefined_type(
    "scrollbar-color",
    "ScrollbarColor",
    "Default::default()",
    engines="gecko",
    spec="https://drafts.csswg.org/css-scrollbars-1/#scrollbar-color",
    boxed=True,
    ignored_when_colors_disabled=True,
    affects="paint",
)}

${helpers.predefined_type(
    "-moz-theme",
    "MozTheme",
    "specified::MozTheme::Auto",
    engines="gecko",
    enabled_in="chrome",
    animation_type="discrete",
    spec="Internal",
    affects="paint",
)}
