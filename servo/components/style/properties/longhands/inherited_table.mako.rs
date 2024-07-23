/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />

${helpers.single_keyword(
    "border-collapse",
    "separate collapse",
    engines="gecko servo",
    gecko_enum_prefix="StyleBorderCollapse",
    animation_type="discrete",
    spec="https://drafts.csswg.org/css-tables/#propdef-border-collapse",
    servo_restyle_damage = "reflow",
    affects="layout",
)}

${helpers.single_keyword(
    "empty-cells",
    "show hide",
    engines="gecko servo",
    gecko_enum_prefix="StyleEmptyCells",
    animation_type="discrete",
    spec="https://drafts.csswg.org/css-tables/#propdef-empty-cells",
    servo_restyle_damage="rebuild_and_reflow",
    affects="paint",
)}

${helpers.predefined_type(
    "caption-side",
    "table::CaptionSide",
    "computed::table::CaptionSide::Top",
    engines="gecko servo",
    animation_type="discrete",
    spec="https://drafts.csswg.org/css-tables/#propdef-caption-side",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "border-spacing",
    "BorderSpacing",
    "computed::BorderSpacing::zero()",
    engines="gecko servo",
    boxed=True,
    spec="https://drafts.csswg.org/css-tables/#propdef-border-spacing",
    servo_restyle_damage="reflow",
    affects="layout",
)}
