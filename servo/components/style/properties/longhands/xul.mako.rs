/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />

${helpers.single_keyword(
    "-moz-box-align",
    "stretch start center baseline end",
    engines="gecko",
    gecko_ffi_name="mBoxAlign",
    gecko_enum_prefix="StyleBoxAlign",
    animation_type="discrete",
    aliases="-webkit-box-align",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/box-align)",
    affects="layout",
)}

${helpers.single_keyword(
    "-moz-box-direction",
    "normal reverse",
    engines="gecko",
    gecko_ffi_name="mBoxDirection",
    gecko_enum_prefix="StyleBoxDirection",
    animation_type="discrete",
    aliases="-webkit-box-direction",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/box-direction)",
    affects="layout",
)}

${helpers.predefined_type(
    "-moz-box-flex",
    "NonNegativeNumber",
    "From::from(0.)",
    engines="gecko",
    gecko_ffi_name="mBoxFlex",
    aliases="-webkit-box-flex",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/box-flex)",
    affects="layout",
)}

${helpers.single_keyword(
    "-moz-box-orient",
    "horizontal vertical",
    engines="gecko",
    gecko_ffi_name="mBoxOrient",
    gecko_aliases="inline-axis=horizontal block-axis=vertical",
    gecko_enum_prefix="StyleBoxOrient",
    animation_type="discrete",
    aliases="-webkit-box-orient",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/box-orient)",
    affects="layout",
)}

${helpers.single_keyword(
    "-moz-box-pack",
    "start center end justify",
    engines="gecko",
    gecko_ffi_name="mBoxPack",
    gecko_enum_prefix="StyleBoxPack",
    animation_type="discrete",
    aliases="-webkit-box-pack",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/box-pack)",
    affects="layout",
)}

// NOTE(heycam): Odd that the initial value is 1 yet 0 is a valid value. There
// are uses of `-moz-box-ordinal-group: 0` in the tree, too.
${helpers.predefined_type(
    "-moz-box-ordinal-group",
    "Integer",
    "1",
    engines="gecko",
    parse_method="parse_non_negative",
    aliases="-webkit-box-ordinal-group",
    gecko_ffi_name="mBoxOrdinal",
    animation_type="discrete",
    spec="Nonstandard (https://developer.mozilla.org/en-US/docs/Web/CSS/-moz-box-ordinal-group)",
    affects="layout",
)}
