/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

<%namespace name="helpers" file="/helpers.mako.rs" />

// TODO: collapse. Well, do tables first.
${helpers.single_keyword(
    "visibility",
    "visible hidden collapse",
    engines="gecko servo",
    gecko_ffi_name="mVisible",
    spec="https://drafts.csswg.org/css-box/#propdef-visibility",
    gecko_enum_prefix="StyleVisibility",
    affects="paint",
)}

// CSS Writing Modes Level 3
// https://drafts.csswg.org/css-writing-modes-3
${helpers.predefined_type(
    "writing-mode",
    "WritingModeProperty",
    "computed::WritingModeProperty::HorizontalTb",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-writing-modes/#propdef-writing-mode",
    servo_pref="layout.writing-mode.enabled",
    animation_type="none",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.single_keyword(
    "direction",
    "ltr rtl",
    engines="gecko servo",
    animation_type="none",
    spec="https://drafts.csswg.org/css-writing-modes/#propdef-direction",
    gecko_enum_prefix="StyleDirection",
    servo_restyle_damage="rebuild_and_reflow",
    affects="layout",
)}

${helpers.single_keyword(
    "-moz-box-collapse",
    "flex legacy",
    engines="gecko",
    gecko_enum_prefix="StyleMozBoxCollapse",
    animation_type="none",
    enabled_in="chrome",
    spec="None (internal)",
    affects="layout",
)}

${helpers.single_keyword(
    "text-orientation",
    "mixed upright sideways",
    engines="gecko",
    gecko_aliases="sideways-right=sideways",
    gecko_enum_prefix="StyleTextOrientation",
    animation_type="none",
    spec="https://drafts.csswg.org/css-writing-modes/#propdef-text-orientation",
    affects="layout",
)}

${helpers.predefined_type(
    "print-color-adjust",
    "PrintColorAdjust",
    "computed::PrintColorAdjust::Economy",
    engines="gecko",
    aliases="color-adjust",
    spec="https://drafts.csswg.org/css-color-adjust/#print-color-adjust",
    animation_type="discrete",
    affects="paint",
)}

// According to to CSS-IMAGES-3, `optimizespeed` and `optimizequality` are synonyms for `auto`
// And, firefox doesn't support `pixelated` yet (https://bugzilla.mozilla.org/show_bug.cgi?id=856337)
${helpers.predefined_type(
    "image-rendering",
    "ImageRendering",
    "computed::ImageRendering::Auto",
    engines="gecko servo",
    spec="https://drafts.csswg.org/css-images/#propdef-image-rendering",
    animation_type="discrete",
    affects="paint",
)}

${helpers.single_keyword(
    "image-orientation",
    "from-image none",
    engines="gecko",
    gecko_enum_prefix="StyleImageOrientation",
    animation_type="discrete",
    spec="https://drafts.csswg.org/css-images/#propdef-image-orientation",
    affects="layout",
)}
