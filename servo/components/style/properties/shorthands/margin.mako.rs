/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />
<% from data import DEFAULT_RULES_AND_PAGE, POSITION_TRY_RULE, DEFAULT_RULES_AND_POSITION_TRY %>

${helpers.four_sides_shorthand(
    "margin",
    "margin-%s",
    "specified::Margin::parse",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-box/#propdef-margin",
    rule_types_allowed=DEFAULT_RULES_AND_PAGE | POSITION_TRY_RULE,
    allow_quirks="Yes",
)}

${helpers.two_properties_shorthand(
    "margin-block",
    "margin-block-start",
    "margin-block-end",
    "specified::Margin::parse",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-logical/#propdef-margin-block",
    rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY
)}

${helpers.two_properties_shorthand(
    "margin-inline",
    "margin-inline-start",
    "margin-inline-end",
    "specified::Margin::parse",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-logical/#propdef-margin-inline",
    rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY
)}

${helpers.four_sides_shorthand(
    "scroll-margin",
    "scroll-margin-%s",
    "specified::Length::parse",
    engines="gecko",
    spec="https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-margin",
)}

${helpers.two_properties_shorthand(
    "scroll-margin-block",
    "scroll-margin-block-start",
    "scroll-margin-block-end",
    "specified::Length::parse",
    engines="gecko",
    spec="https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-margin-block",
)}

${helpers.two_properties_shorthand(
    "scroll-margin-inline",
    "scroll-margin-inline-start",
    "scroll-margin-inline-end",
    "specified::Length::parse",
    engines="gecko",
    spec="https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-margin-inline",
)}
