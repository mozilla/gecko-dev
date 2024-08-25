/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />
<% from data import ALL_SIDES, maybe_moz_logical_alias %>

% for index, side in enumerate(ALL_SIDES):
    <%
        spec = "https://drafts.csswg.org/css-box/#propdef-padding-%s" % side[0]
        if side[1]:
            spec = "https://drafts.csswg.org/css-logical-props/#propdef-padding-%s" % side[1]
    %>
    ${helpers.predefined_type(
        "padding-%s" % side[0],
        "NonNegativeLengthPercentage",
        "computed::NonNegativeLengthPercentage::zero()",
        engines="gecko servo",
        aliases=maybe_moz_logical_alias(engine, side, "-moz-padding-%s"),
        logical=side[1],
        logical_group="padding",
        spec=spec,
        gecko_ffi_name="mPadding.{}".format(index),
        allow_quirks="No" if side[1] else "Yes",
        servo_restyle_damage="reflow rebuild_and_reflow_inline",
        affects="layout",
    )}
% endfor

% for index, side in enumerate(ALL_SIDES):
    ${helpers.predefined_type(
        "scroll-padding-%s" % side[0],
        "NonNegativeLengthPercentageOrAuto",
        "computed::NonNegativeLengthPercentageOrAuto::auto()",
        engines="gecko",
        logical=side[1],
        logical_group="scroll-padding",
        gecko_ffi_name="mScrollPadding.{}".format(index),
        spec="https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-padding-%s" % side[0],
        affects="",
    )}
% endfor
