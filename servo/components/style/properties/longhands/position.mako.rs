/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />
<% from data import ALL_SIZES, PHYSICAL_SIDES, LOGICAL_SIDES, DEFAULT_RULES_AND_POSITION_TRY %>

// "top" / "left" / "bottom" / "right"
% for index, side in enumerate(PHYSICAL_SIDES):
    ${helpers.predefined_type(
        side,
        "Inset",
        "computed::Inset::auto()",
        engines="gecko servo",
        spec="https://www.w3.org/TR/CSS2/visuren.html#propdef-%s" % side,
        allow_quirks="Yes",
        rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
        gecko_ffi_name="mOffset.{}".format(index),
        servo_restyle_damage="reflow_out_of_flow",
        logical_group="inset",
        affects="layout",
    )}
% endfor
// inset-* logical properties, map to "top" / "left" / "bottom" / "right"
% for side in LOGICAL_SIDES:
    ${helpers.predefined_type(
        "inset-%s" % side,
        "Inset",
        "computed::Inset::auto()",
        engines="gecko servo",
        spec="https://drafts.csswg.org/css-logical-props/#propdef-inset-%s" % side,
        rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
        logical=True,
        logical_group="inset",
        affects="layout",
    )}
% endfor

${helpers.predefined_type(
    "z-index",
    "ZIndex",
    "computed::ZIndex::auto()",
    engines="gecko servo",
    spec="https://www.w3.org/TR/CSS2/visuren.html#z-index",
    affects="paint",
)}

// CSS Flexible Box Layout Module Level 1
// http://www.w3.org/TR/css3-flexbox/

// Flex container properties
${helpers.single_keyword(
    "flex-direction",
    "row row-reverse column column-reverse",
    engines="gecko servo",
    servo_pref="layout.flexbox.enabled",
    spec="https://drafts.csswg.org/css-flexbox/#flex-direction-property",
    extra_prefixes="webkit",
    animation_type="discrete",
    servo_restyle_damage = "reflow",
    gecko_enum_prefix = "StyleFlexDirection",
    affects="layout",
)}

${helpers.single_keyword(
    "flex-wrap",
    "nowrap wrap wrap-reverse",
    engines="gecko servo",
    servo_pref="layout.flexbox.enabled",
    spec="https://drafts.csswg.org/css-flexbox/#flex-wrap-property",
    extra_prefixes="webkit",
    animation_type="discrete",
    servo_restyle_damage = "reflow",
    gecko_enum_prefix = "StyleFlexWrap",
    affects="layout",
)}

${helpers.predefined_type(
    "justify-content",
    "JustifyContent",
    "specified::JustifyContent(specified::ContentDistribution::normal())",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-align/#propdef-justify-content",
    extra_prefixes="webkit",
    animation_type="discrete",
    servo_restyle_damage="reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "align-content",
    "AlignContent",
    "specified::AlignContent(specified::ContentDistribution::normal())",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-align/#propdef-align-content",
    extra_prefixes="webkit",
    animation_type="discrete",
    servo_restyle_damage="reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "align-items",
    "AlignItems",
    "specified::AlignItems::normal()",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-align/#propdef-align-items",
    extra_prefixes="webkit",
    animation_type="discrete",
    servo_restyle_damage="reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "justify-items",
    "JustifyItems",
    "computed::JustifyItems::legacy()",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-align/#propdef-justify-items",
    animation_type="discrete",
    affects="layout",
)}

// Flex item properties
${helpers.predefined_type(
    "flex-grow",
    "NonNegativeNumber",
    "From::from(0.0)",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-flexbox/#flex-grow-property",
    extra_prefixes="webkit",
    servo_restyle_damage="reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "flex-shrink",
    "NonNegativeNumber",
    "From::from(1.0)",
    engines="gecko servo",
    servo_pref="layout.flexbox.enabled",
    spec="https://drafts.csswg.org/css-flexbox/#flex-shrink-property",
    extra_prefixes="webkit",
    servo_restyle_damage = "reflow",
    affects="layout",
)}

// https://drafts.csswg.org/css-align/#align-self-property
${helpers.predefined_type(
    "align-self",
    "AlignSelf",
    "specified::AlignSelf(specified::SelfAlignment::auto())",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-align/#align-self-property",
    extra_prefixes="webkit",
    animation_type="discrete",
    rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
    affects="layout",
)}
${helpers.predefined_type(
    "justify-self",
    "JustifySelf",
    "specified::JustifySelf(specified::SelfAlignment::auto())",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-align/#justify-self-property",
    animation_type="discrete",
    rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
    affects="layout",
)}

// https://drafts.csswg.org/css-flexbox/#propdef-order
${helpers.predefined_type(
    "order",
    "Integer",
    "0",
    engines="gecko servo",
    servo_pref="layout.flexbox.enabled",
    extra_prefixes="webkit",
    spec="https://drafts.csswg.org/css-flexbox/#order-property",
    servo_restyle_damage="reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "flex-basis",
    "FlexBasis",
    "computed::FlexBasis::auto()",
    engines="gecko servo",
    servo_pref="layout.flexbox.enabled",
    spec="https://drafts.csswg.org/css-flexbox/#flex-basis-property",
    extra_prefixes="webkit",
    servo_restyle_damage="reflow",
    boxed=True,
    affects="layout",
)}

% for (size, logical) in ALL_SIZES:
    <%
        spec = "https://drafts.csswg.org/css-box/#propdef-%s"
        if logical:
            spec = "https://drafts.csswg.org/css-logical-props/#propdef-%s"
    %>
    // width, height, block-size, inline-size
    ${helpers.predefined_type(
        size,
        "Size",
        "computed::Size::auto()",
        engines="gecko servo",
        logical=logical,
        logical_group="size",
        allow_quirks="No" if logical else "Yes",
        spec=spec % size,
        rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
        servo_restyle_damage="reflow",
        affects="layout",
    )}
    // min-width, min-height, min-block-size, min-inline-size
    ${helpers.predefined_type(
        "min-%s" % size,
        "Size",
        "computed::Size::auto()",
        engines="gecko servo",
        logical=logical,
        logical_group="min-size",
        allow_quirks="No" if logical else "Yes",
        spec=spec % size,
        rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
        servo_restyle_damage="reflow",
        affects="layout",
    )}
    ${helpers.predefined_type(
        "max-%s" % size,
        "MaxSize",
        "computed::MaxSize::none()",
        engines="gecko servo",
        logical=logical,
        logical_group="max-size",
        allow_quirks="No" if logical else "Yes",
        spec=spec % size,
        rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
        servo_restyle_damage="reflow",
        affects="layout",
    )}
% endfor

${helpers.predefined_type(
    "position-anchor",
    "PositionAnchor",
    "computed::PositionAnchor::auto()",
    engines="gecko",
    animation_type="discrete",
    rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
    gecko_pref="layout.css.anchor-positioning.enabled",
    spec="https://drafts.csswg.org/css-anchor-position-1/#propdef-position-anchor",
    affects="layout",
)}

${helpers.predefined_type(
    "position-area",
    "PositionArea",
    "computed::PositionArea::none()",
    engines="gecko",
    initial_specified_value="specified::PositionArea::none()",
    animation_type="discrete",
    rule_types_allowed=DEFAULT_RULES_AND_POSITION_TRY,
    gecko_pref="layout.css.anchor-positioning.enabled",
    spec="https://drafts.csswg.org/css-anchor-position-1/#typedef-position-area",
    affects="layout",
)}

${helpers.predefined_type(
    "position-visibility",
    "PositionVisibility",
    "computed::PositionVisibility::ALWAYS",
    engines="gecko",
    initial_specified_value="specified::PositionVisibility::ALWAYS",
    animation_type="discrete",
    gecko_pref="layout.css.anchor-positioning.enabled",
    spec="https://drafts.csswg.org/css-anchor-position-1/#propdef-position-visibility",
    affects="layout",
)}

${helpers.predefined_type(
    "position-try-fallbacks",
    "PositionTryFallbacks",
    "computed::PositionTryFallbacks::none()",
    engines="gecko",
    initial_specified_value="specified::PositionTryFallbacks::none()",
    animation_type="discrete",
    gecko_pref="layout.css.anchor-positioning.enabled",
    spec="https://drafts.csswg.org/css-anchor-position-1/#position-try-fallbacks",
    affects="layout",
)}

${helpers.predefined_type(
    "position-try-order",
    "PositionTryOrder",
    "computed::PositionTryOrder::normal()",
    engines="gecko",
    initial_specified_value="specified::PositionTryOrder::normal()",
    animation_type="discrete",
    gecko_pref="layout.css.anchor-positioning.enabled",
    spec="https://drafts.csswg.org/css-anchor-position-1/#position-try-order-property",
    affects="layout",
)}

${helpers.single_keyword(
    "box-sizing",
    "content-box border-box",
    engines="gecko servo",
    extra_prefixes="moz:layout.css.prefixes.box-sizing webkit",
    spec="https://drafts.csswg.org/css-ui/#propdef-box-sizing",
    gecko_enum_prefix="StyleBoxSizing",
    custom_consts={ "content-box": "Content", "border-box": "Border" },
    animation_type="discrete",
    servo_restyle_damage = "reflow",
    affects="layout",
)}

${helpers.single_keyword(
    "object-fit",
    "fill contain cover none scale-down",
    engines="gecko",
    animation_type="discrete",
    spec="https://drafts.csswg.org/css-images/#propdef-object-fit",
    gecko_enum_prefix = "StyleObjectFit",
    affects="layout",
)}

${helpers.predefined_type(
    "object-position",
    "Position",
    "computed::Position::center()",
    engines="gecko",
    boxed=True,
    spec="https://drafts.csswg.org/css-images-3/#the-object-position",
    affects="layout",
)}

% for kind in ["row", "column"]:
    % for range in ["start", "end"]:
        ${helpers.predefined_type(
            "grid-%s-%s" % (kind, range),
            "GridLine",
            "Default::default()",
            engines="gecko servo",
            servo_pref="layout.grid.enabled",
            animation_type="discrete",
            spec="https://drafts.csswg.org/css-grid/#propdef-grid-%s-%s" % (kind, range),
            affects="layout",
        )}
    % endfor

    ${helpers.predefined_type(
        "grid-auto-%ss" % kind,
        "ImplicitGridTracks",
        "Default::default()",
        engines="gecko servo",
        servo_pref="layout.grid.enabled",
        animation_type="discrete",
        spec="https://drafts.csswg.org/css-grid/#propdef-grid-auto-%ss" % kind,
        affects="layout",
    )}

    ${helpers.predefined_type(
        "grid-template-%ss" % kind,
        "GridTemplateComponent",
        "specified::GenericGridTemplateComponent::None",
        engines="gecko servo",
        servo_pref="layout.grid.enabled",
        spec="https://drafts.csswg.org/css-grid/#propdef-grid-template-%ss" % kind,
        affects="layout",
    )}

% endfor

${helpers.predefined_type(
    "masonry-auto-flow",
    "MasonryAutoFlow",
    "computed::MasonryAutoFlow::initial()",
    engines="gecko",
    gecko_pref="layout.css.grid-template-masonry-value.enabled",
    animation_type="discrete",
    spec="https://github.com/w3c/csswg-drafts/issues/4650",
    affects="layout",
)}

${helpers.predefined_type(
    "grid-auto-flow",
    "GridAutoFlow",
    "computed::GridAutoFlow::ROW",
    engines="gecko servo",
    servo_pref="layout.grid.enabled",
    animation_type="discrete",
    spec="https://drafts.csswg.org/css-grid/#propdef-grid-auto-flow",
    affects="layout",
)}

${helpers.predefined_type(
    "grid-template-areas",
    "GridTemplateAreas",
    "computed::GridTemplateAreas::none()",
    engines="gecko servo",
    servo_pref="layout.grid.enabled",
    animation_type="discrete",
    spec="https://drafts.csswg.org/css-grid/#propdef-grid-template-areas",
    affects="layout",
)}

${helpers.predefined_type(
    "column-gap",
    "length::NonNegativeLengthPercentageOrNormal",
    "computed::length::NonNegativeLengthPercentageOrNormal::normal()",
    engines="gecko servo",
    aliases="grid-column-gap",
    servo_pref="layout.flexbox.enabled",
    spec="https://drafts.csswg.org/css-align-3/#propdef-column-gap",
    servo_restyle_damage="reflow",
    affects="layout",
)}

// no need for -moz- prefixed alias for this property
${helpers.predefined_type(
    "row-gap",
    "length::NonNegativeLengthPercentageOrNormal",
    "computed::length::NonNegativeLengthPercentageOrNormal::normal()",
    engines="gecko servo",
    aliases="grid-row-gap",
    servo_pref="layout.flexbox.enabled",
    spec="https://drafts.csswg.org/css-align-3/#propdef-row-gap",
    servo_restyle_damage="reflow",
    affects="layout",
)}

${helpers.predefined_type(
    "aspect-ratio",
    "AspectRatio",
    "computed::AspectRatio::auto()",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-sizing-4/#aspect-ratio",
    servo_restyle_damage="reflow",
    affects="layout",
)}

% for (size, logical) in ALL_SIZES:
    ${helpers.predefined_type(
        "contain-intrinsic-" + size,
        "ContainIntrinsicSize",
        "computed::ContainIntrinsicSize::None",
        engines="gecko",
        logical_group="contain-intrinsic-size",
        logical=logical,
        gecko_pref="layout.css.contain-intrinsic-size.enabled",
        spec="https://drafts.csswg.org/css-sizing-4/#intrinsic-size-override",
        affects="layout",
    )}
% endfor
