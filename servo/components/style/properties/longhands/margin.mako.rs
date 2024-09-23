/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />
<% from data import ALL_SIDES, DEFAULT_RULES, DEFAULT_RULES_AND_PAGE, POSITION_TRY_RULE, maybe_moz_logical_alias %>

% for index, side in enumerate(ALL_SIDES):
    <%
        spec = "https://drafts.csswg.org/css-box/#propdef-margin-%s" % side[0]
        if side[1]:
            spec = "https://drafts.csswg.org/css-logical-props/#propdef-margin-%s" % side[1]
    %>
    ${helpers.predefined_type(
        "margin-%s" % side[0],
        "Margin",
        "computed::Margin::zero()",
        engines="gecko servo",
        aliases=maybe_moz_logical_alias(engine, side, "-moz-margin-%s"),
        allow_quirks="No" if side[1] else "Yes",
        logical=side[1],
        logical_group="margin",
        gecko_ffi_name="mMargin.{}".format(index),
        spec=spec,
        rule_types_allowed=(DEFAULT_RULES if side[1] else DEFAULT_RULES_AND_PAGE) | POSITION_TRY_RULE,
        servo_restyle_damage="reflow",
        affects="layout",
    )}
% endfor

${helpers.predefined_type(
    "overflow-clip-margin",
    "Length",
    "computed::Length::zero()",
    parse_method="parse_non_negative",
    engines="gecko",
    spec="https://drafts.csswg.org/css-overflow/#propdef-overflow-clip-margin",
    affects="overflow",
)}

% for index, side in enumerate(ALL_SIDES):
    ${helpers.predefined_type(
        "scroll-margin-%s" % side[0],
        "Length",
        "computed::Length::zero()",
        engines="gecko",
        logical=side[1],
        logical_group="scroll-margin",
        gecko_ffi_name="mScrollMargin.{}".format(index),
        spec="https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-margin-%s" % side[0],
        affects="",
    )}
% endfor
