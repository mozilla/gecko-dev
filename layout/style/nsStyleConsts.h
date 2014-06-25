/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants used in the style struct data provided by nsStyleContext */

#ifndef nsStyleConsts_h___
#define nsStyleConsts_h___

#include "gfxRect.h"
#include "nsFont.h"

// XXX fold this into nsStyleContext and group by nsStyleXXX struct

// Indices into border/padding/margin arrays
#define NS_SIDE_TOP     mozilla::css::eSideTop
#define NS_SIDE_RIGHT   mozilla::css::eSideRight
#define NS_SIDE_BOTTOM  mozilla::css::eSideBottom
#define NS_SIDE_LEFT    mozilla::css::eSideLeft

#define NS_FOR_CSS_SIDES(var_) for (mozilla::css::Side var_ = NS_SIDE_TOP; var_ <= NS_SIDE_LEFT; var_++)
static inline mozilla::css::Side operator++(mozilla::css::Side& side, int) {
    NS_PRECONDITION(side >= NS_SIDE_TOP &&
                    side <= NS_SIDE_LEFT, "Out of range side");
    side = mozilla::css::Side(side + 1);
    return side;
}

#define NS_FOR_CSS_FULL_CORNERS(var_) for (int32_t var_ = 0; var_ < 4; ++var_)

// Indices into "half corner" arrays (nsStyleCorners e.g.)
#define NS_CORNER_TOP_LEFT_X      0
#define NS_CORNER_TOP_LEFT_Y      1
#define NS_CORNER_TOP_RIGHT_X     2
#define NS_CORNER_TOP_RIGHT_Y     3
#define NS_CORNER_BOTTOM_RIGHT_X  4
#define NS_CORNER_BOTTOM_RIGHT_Y  5
#define NS_CORNER_BOTTOM_LEFT_X   6
#define NS_CORNER_BOTTOM_LEFT_Y   7

#define NS_FOR_CSS_HALF_CORNERS(var_) for (int32_t var_ = 0; var_ < 8; ++var_)

// The results of these conversion macros are exhaustively checked in
// nsStyleCoord.cpp.
// Arguments must not have side effects.

#define NS_HALF_CORNER_IS_X(var_) (!((var_)%2))
#define NS_HALF_TO_FULL_CORNER(var_) ((var_)/2)
#define NS_FULL_TO_HALF_CORNER(var_, vert_) ((var_)*2 + !!(vert_))

#define NS_SIDE_IS_VERTICAL(side_) ((side_) % 2)
#define NS_SIDE_TO_FULL_CORNER(side_, second_) \
  (((side_) + !!(second_)) % 4)
#define NS_SIDE_TO_HALF_CORNER(side_, second_, parallel_) \
  ((((side_) + !!(second_))*2 + ((side_) + !(parallel_))%2) % 8)

// {margin,border-{width,style,color},padding}-{left,right}-{ltr,rtl}-source
#define NS_BOXPROP_SOURCE_PHYSICAL 0
#define NS_BOXPROP_SOURCE_LOGICAL  1

// box-sizing
#define NS_STYLE_BOX_SIZING_CONTENT       0
#define NS_STYLE_BOX_SIZING_PADDING       1
#define NS_STYLE_BOX_SIZING_BORDER        2

// box-shadow
#define NS_STYLE_BOX_SHADOW_INSET         0

// float-edge
#define NS_STYLE_FLOAT_EDGE_CONTENT       0
#define NS_STYLE_FLOAT_EDGE_MARGIN        1

// user-focus
#define NS_STYLE_USER_FOCUS_NONE            0
#define NS_STYLE_USER_FOCUS_IGNORE          1
#define NS_STYLE_USER_FOCUS_NORMAL          2
#define NS_STYLE_USER_FOCUS_SELECT_ALL      3
#define NS_STYLE_USER_FOCUS_SELECT_BEFORE   4
#define NS_STYLE_USER_FOCUS_SELECT_AFTER    5
#define NS_STYLE_USER_FOCUS_SELECT_SAME     6
#define NS_STYLE_USER_FOCUS_SELECT_MENU     7

// user-select
#define NS_STYLE_USER_SELECT_NONE       0
#define NS_STYLE_USER_SELECT_TEXT       1
#define NS_STYLE_USER_SELECT_ELEMENT    2
#define NS_STYLE_USER_SELECT_ELEMENTS   3
#define NS_STYLE_USER_SELECT_ALL        4
#define NS_STYLE_USER_SELECT_TOGGLE     5
#define NS_STYLE_USER_SELECT_TRI_STATE  6
#define NS_STYLE_USER_SELECT_AUTO       7 // internal value - please use nsFrame::IsSelectable()
#define NS_STYLE_USER_SELECT_MOZ_ALL    8 // force selection of all children, unless an ancestor has NONE set - bug 48096
#define NS_STYLE_USER_SELECT_MOZ_NONE   9 // Like NONE, but doesn't change selection behavior for descendants whose user-select is not AUTO.

// user-input
#define NS_STYLE_USER_INPUT_NONE      0
#define NS_STYLE_USER_INPUT_ENABLED   1
#define NS_STYLE_USER_INPUT_DISABLED  2
#define NS_STYLE_USER_INPUT_AUTO      3

// user-modify
#define NS_STYLE_USER_MODIFY_READ_ONLY   0
#define NS_STYLE_USER_MODIFY_READ_WRITE  1
#define NS_STYLE_USER_MODIFY_WRITE_ONLY  2

// box-align
#define NS_STYLE_BOX_ALIGN_STRETCH     0
#define NS_STYLE_BOX_ALIGN_START       1
#define NS_STYLE_BOX_ALIGN_CENTER      2
#define NS_STYLE_BOX_ALIGN_BASELINE    3
#define NS_STYLE_BOX_ALIGN_END         4

// box-pack
#define NS_STYLE_BOX_PACK_START        0
#define NS_STYLE_BOX_PACK_CENTER       1
#define NS_STYLE_BOX_PACK_END          2
#define NS_STYLE_BOX_PACK_JUSTIFY      3

// box-decoration-break
#define NS_STYLE_BOX_DECORATION_BREAK_SLICE  0
#define NS_STYLE_BOX_DECORATION_BREAK_CLONE  1

// box-direction
#define NS_STYLE_BOX_DIRECTION_NORMAL    0
#define NS_STYLE_BOX_DIRECTION_REVERSE   1

// box-orient
#define NS_STYLE_BOX_ORIENT_HORIZONTAL 0
#define NS_STYLE_BOX_ORIENT_VERTICAL   1

// orient
#define NS_STYLE_ORIENT_HORIZONTAL 0
#define NS_STYLE_ORIENT_VERTICAL   1
#define NS_STYLE_ORIENT_AUTO       2

// stack-sizing
#define NS_STYLE_STACK_SIZING_IGNORE         0
#define NS_STYLE_STACK_SIZING_STRETCH_TO_FIT 1

// Azimuth - See nsStyleAural
#define NS_STYLE_AZIMUTH_LEFT_SIDE        0x00
#define NS_STYLE_AZIMUTH_FAR_LEFT         0x01
#define NS_STYLE_AZIMUTH_LEFT             0x02
#define NS_STYLE_AZIMUTH_CENTER_LEFT      0x03
#define NS_STYLE_AZIMUTH_CENTER           0x04
#define NS_STYLE_AZIMUTH_CENTER_RIGHT     0x05
#define NS_STYLE_AZIMUTH_RIGHT            0x06
#define NS_STYLE_AZIMUTH_FAR_RIGHT        0x07
#define NS_STYLE_AZIMUTH_RIGHT_SIDE       0x08
#define NS_STYLE_AZIMUTH_BEHIND           0x80  // bits
#define NS_STYLE_AZIMUTH_LEFTWARDS        0x10  // bits
#define NS_STYLE_AZIMUTH_RIGHTWARDS       0x20  // bits

// See nsStyleAural
#define NS_STYLE_ELEVATION_BELOW          1
#define NS_STYLE_ELEVATION_LEVEL          2
#define NS_STYLE_ELEVATION_ABOVE          3
#define NS_STYLE_ELEVATION_HIGHER         4
#define NS_STYLE_ELEVATION_LOWER          5

// See nsStyleAural
#define NS_STYLE_PITCH_X_LOW              1
#define NS_STYLE_PITCH_LOW                2
#define NS_STYLE_PITCH_MEDIUM             3
#define NS_STYLE_PITCH_HIGH               4
#define NS_STYLE_PITCH_X_HIGH             5

// See nsStyleAural
#define NS_STYLE_SPEAK_NONE               0
#define NS_STYLE_SPEAK_NORMAL             1
#define NS_STYLE_SPEAK_SPELL_OUT          2

// See nsStyleAural
#define NS_STYLE_SPEAK_HEADER_ONCE        0
#define NS_STYLE_SPEAK_HEADER_ALWAYS      1

// See nsStyleAural
#define NS_STYLE_SPEAK_NUMERAL_DIGITS     0
#define NS_STYLE_SPEAK_NUMERAL_CONTINUOUS 1

// See nsStyleAural
#define NS_STYLE_SPEAK_PUNCTUATION_NONE   0
#define NS_STYLE_SPEAK_PUNCTUATION_CODE   1

// See nsStyleAural
#define NS_STYLE_SPEECH_RATE_X_SLOW       0
#define NS_STYLE_SPEECH_RATE_SLOW         1
#define NS_STYLE_SPEECH_RATE_MEDIUM       2
#define NS_STYLE_SPEECH_RATE_FAST         3
#define NS_STYLE_SPEECH_RATE_X_FAST       4
#define NS_STYLE_SPEECH_RATE_FASTER       10
#define NS_STYLE_SPEECH_RATE_SLOWER       11

// See nsStyleAural
#define NS_STYLE_VOLUME_SILENT            0
#define NS_STYLE_VOLUME_X_SOFT            1
#define NS_STYLE_VOLUME_SOFT              2
#define NS_STYLE_VOLUME_MEDIUM            3
#define NS_STYLE_VOLUME_LOUD              4
#define NS_STYLE_VOLUME_X_LOUD            5

// See nsStyleColor
#define NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR 1
#define NS_STYLE_COLOR_INHERIT_FROM_BODY  2  /* Can't come from CSS directly */

// See nsStyleColor
#define NS_COLOR_CURRENTCOLOR                   -1
#define NS_COLOR_MOZ_DEFAULT_COLOR              -2
#define NS_COLOR_MOZ_DEFAULT_BACKGROUND_COLOR   -3
#define NS_COLOR_MOZ_HYPERLINKTEXT              -4
#define NS_COLOR_MOZ_VISITEDHYPERLINKTEXT       -5
#define NS_COLOR_MOZ_ACTIVEHYPERLINKTEXT        -6
// Only valid as paints in SVG glyphs
#define NS_COLOR_CONTEXT_FILL                   -7
#define NS_COLOR_CONTEXT_STROKE                 -8

// See nsStyleDisplay
#define NS_STYLE_WILL_CHANGE_STACKING_CONTEXT   (1<<0)
#define NS_STYLE_WILL_CHANGE_TRANSFORM          (1<<1)
#define NS_STYLE_WILL_CHANGE_SCROLL             (1<<2)
#define NS_STYLE_WILL_CHANGE_OPACITY            (1<<3)

// See nsStyleDisplay
#define NS_STYLE_ANIMATION_DIRECTION_NORMAL       0
#define NS_STYLE_ANIMATION_DIRECTION_REVERSE      1
#define NS_STYLE_ANIMATION_DIRECTION_ALTERNATE    2
#define NS_STYLE_ANIMATION_DIRECTION_ALTERNATE_REVERSE    3

// See nsStyleDisplay
#define NS_STYLE_ANIMATION_FILL_MODE_NONE         0
#define NS_STYLE_ANIMATION_FILL_MODE_FORWARDS     1
#define NS_STYLE_ANIMATION_FILL_MODE_BACKWARDS    2
#define NS_STYLE_ANIMATION_FILL_MODE_BOTH         3

// See nsStyleDisplay
#define NS_STYLE_ANIMATION_ITERATION_COUNT_INFINITE 0

// See nsStyleDisplay
#define NS_STYLE_ANIMATION_PLAY_STATE_RUNNING     0
#define NS_STYLE_ANIMATION_PLAY_STATE_PAUSED      1

// See nsStyleBackground
#define NS_STYLE_BG_ATTACHMENT_SCROLL     0
#define NS_STYLE_BG_ATTACHMENT_FIXED      1
#define NS_STYLE_BG_ATTACHMENT_LOCAL      2

// See nsStyleBackground
// Code depends on these constants having the same values as BG_ORIGIN_*
#define NS_STYLE_BG_CLIP_BORDER           0
#define NS_STYLE_BG_CLIP_PADDING          1
#define NS_STYLE_BG_CLIP_CONTENT          2
// A magic value that we use for our "pretend that background-clip is
// 'padding' when we have a solid border" optimization.  This isn't
// actually equal to NS_STYLE_BG_CLIP_PADDING because using that
// causes antialiasing seams between the background and border.  This
// is a backend-only value.
#define NS_STYLE_BG_CLIP_MOZ_ALMOST_PADDING 127

// See nsStyleBackground
#define NS_STYLE_BG_INLINE_POLICY_EACH_BOX      0
#define NS_STYLE_BG_INLINE_POLICY_CONTINUOUS    1
#define NS_STYLE_BG_INLINE_POLICY_BOUNDING_BOX  2

// See nsStyleBackground
// Code depends on these constants having the same values as BG_CLIP_*
#define NS_STYLE_BG_ORIGIN_BORDER         0
#define NS_STYLE_BG_ORIGIN_PADDING        1
#define NS_STYLE_BG_ORIGIN_CONTENT        2

// See nsStyleBackground
// The parser code depends on |ing these values together.
#define NS_STYLE_BG_POSITION_CENTER  (1<<0)
#define NS_STYLE_BG_POSITION_TOP     (1<<1)
#define NS_STYLE_BG_POSITION_BOTTOM  (1<<2)
#define NS_STYLE_BG_POSITION_LEFT    (1<<3)
#define NS_STYLE_BG_POSITION_RIGHT   (1<<4)

// See nsStyleBackground
#define NS_STYLE_BG_REPEAT_NO_REPEAT                0x00
#define NS_STYLE_BG_REPEAT_REPEAT_X                 0x01
#define NS_STYLE_BG_REPEAT_REPEAT_Y                 0x02
#define NS_STYLE_BG_REPEAT_REPEAT                   0x03

// See nsStyleBackground
#define NS_STYLE_BG_SIZE_CONTAIN  0
#define NS_STYLE_BG_SIZE_COVER    1

// See nsStyleTable
#define NS_STYLE_BORDER_COLLAPSE                0
#define NS_STYLE_BORDER_SEPARATE                1

// Possible enumerated specified values of border-*-width, used by nsCSSMargin
#define NS_STYLE_BORDER_WIDTH_THIN              0
#define NS_STYLE_BORDER_WIDTH_MEDIUM            1
#define NS_STYLE_BORDER_WIDTH_THICK             2
// XXX chopping block #define NS_STYLE_BORDER_WIDTH_LENGTH_VALUE      3

// See nsStyleBorder mBorderStyle
#define NS_STYLE_BORDER_STYLE_NONE              0
#define NS_STYLE_BORDER_STYLE_GROOVE            1
#define NS_STYLE_BORDER_STYLE_RIDGE             2
#define NS_STYLE_BORDER_STYLE_DOTTED            3
#define NS_STYLE_BORDER_STYLE_DASHED            4
#define NS_STYLE_BORDER_STYLE_SOLID             5
#define NS_STYLE_BORDER_STYLE_DOUBLE            6
#define NS_STYLE_BORDER_STYLE_INSET             7
#define NS_STYLE_BORDER_STYLE_OUTSET            8
#define NS_STYLE_BORDER_STYLE_HIDDEN            9
#define NS_STYLE_BORDER_STYLE_AUTO              10 // for outline-style only

// See nsStyleBorder mBorderImage
#define NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH    0
#define NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT     1
#define NS_STYLE_BORDER_IMAGE_REPEAT_ROUND      2

#define NS_STYLE_BORDER_IMAGE_SLICE_NOFILL      0
#define NS_STYLE_BORDER_IMAGE_SLICE_FILL        1

// See nsStyleDisplay
#define NS_STYLE_CLEAR_NONE                     0
#define NS_STYLE_CLEAR_LEFT                     1
#define NS_STYLE_CLEAR_RIGHT                    2
#define NS_STYLE_CLEAR_BOTH                     3
#define NS_STYLE_CLEAR_LINE                     4
// @note NS_STYLE_CLEAR_LINE can be added to one of the other values in layout
// so it needs to use a bit value that none of the other values can have.
#define NS_STYLE_CLEAR_MAX (NS_STYLE_CLEAR_LINE | NS_STYLE_CLEAR_BOTH)

// See nsStyleContent
#define NS_STYLE_CONTENT_OPEN_QUOTE             0
#define NS_STYLE_CONTENT_CLOSE_QUOTE            1
#define NS_STYLE_CONTENT_NO_OPEN_QUOTE          2
#define NS_STYLE_CONTENT_NO_CLOSE_QUOTE         3
#define NS_STYLE_CONTENT_ALT_CONTENT            4

// See nsStyleColor
#define NS_STYLE_CURSOR_AUTO                    1
#define NS_STYLE_CURSOR_CROSSHAIR               2
#define NS_STYLE_CURSOR_DEFAULT                 3    // ie: an arrow
#define NS_STYLE_CURSOR_POINTER                 4    // for links
#define NS_STYLE_CURSOR_MOVE                    5
#define NS_STYLE_CURSOR_E_RESIZE                6
#define NS_STYLE_CURSOR_NE_RESIZE               7
#define NS_STYLE_CURSOR_NW_RESIZE               8
#define NS_STYLE_CURSOR_N_RESIZE                9
#define NS_STYLE_CURSOR_SE_RESIZE               10
#define NS_STYLE_CURSOR_SW_RESIZE               11
#define NS_STYLE_CURSOR_S_RESIZE                12
#define NS_STYLE_CURSOR_W_RESIZE                13
#define NS_STYLE_CURSOR_TEXT                    14   // ie: i-beam
#define NS_STYLE_CURSOR_WAIT                    15
#define NS_STYLE_CURSOR_HELP                    16
#define NS_STYLE_CURSOR_COPY                    17   // CSS3
#define NS_STYLE_CURSOR_ALIAS                   18
#define NS_STYLE_CURSOR_CONTEXT_MENU            19
#define NS_STYLE_CURSOR_CELL                    20
#define NS_STYLE_CURSOR_GRAB                    21
#define NS_STYLE_CURSOR_GRABBING                22
#define NS_STYLE_CURSOR_SPINNING                23
#define NS_STYLE_CURSOR_ZOOM_IN                 24
#define NS_STYLE_CURSOR_ZOOM_OUT                25
#define NS_STYLE_CURSOR_NOT_ALLOWED             26
#define NS_STYLE_CURSOR_COL_RESIZE              27
#define NS_STYLE_CURSOR_ROW_RESIZE              28
#define NS_STYLE_CURSOR_NO_DROP                 29
#define NS_STYLE_CURSOR_VERTICAL_TEXT           30
#define NS_STYLE_CURSOR_ALL_SCROLL              31
#define NS_STYLE_CURSOR_NESW_RESIZE             32
#define NS_STYLE_CURSOR_NWSE_RESIZE             33
#define NS_STYLE_CURSOR_NS_RESIZE               34
#define NS_STYLE_CURSOR_EW_RESIZE               35
#define NS_STYLE_CURSOR_NONE                    36

// See nsStyleVisibility
#define NS_STYLE_DIRECTION_LTR                  0
#define NS_STYLE_DIRECTION_RTL                  1
#define NS_STYLE_DIRECTION_INHERIT              2

// See nsStyleVisibility
#define NS_STYLE_WRITING_MODE_HORIZONTAL_TB     0
#define NS_STYLE_WRITING_MODE_VERTICAL_LR       1
#define NS_STYLE_WRITING_MODE_VERTICAL_RL       2

// See nsStyleDisplay
#define NS_STYLE_DISPLAY_NONE                   0
#define NS_STYLE_DISPLAY_BLOCK                  1
#define NS_STYLE_DISPLAY_INLINE                 2
#define NS_STYLE_DISPLAY_INLINE_BLOCK           3
#define NS_STYLE_DISPLAY_LIST_ITEM              4
#define NS_STYLE_DISPLAY_TABLE                  8
#define NS_STYLE_DISPLAY_INLINE_TABLE           9
#define NS_STYLE_DISPLAY_TABLE_ROW_GROUP        10
#define NS_STYLE_DISPLAY_TABLE_COLUMN           11
#define NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP     12
#define NS_STYLE_DISPLAY_TABLE_HEADER_GROUP     13
#define NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP     14
#define NS_STYLE_DISPLAY_TABLE_ROW              15
#define NS_STYLE_DISPLAY_TABLE_CELL             16
#define NS_STYLE_DISPLAY_TABLE_CAPTION          17
#define NS_STYLE_DISPLAY_BOX                    18
#define NS_STYLE_DISPLAY_INLINE_BOX             19
#ifdef MOZ_XUL
#define NS_STYLE_DISPLAY_XUL_GRID               20
#define NS_STYLE_DISPLAY_INLINE_XUL_GRID        21
#define NS_STYLE_DISPLAY_XUL_GRID_GROUP         22
#define NS_STYLE_DISPLAY_XUL_GRID_LINE          23
#define NS_STYLE_DISPLAY_STACK                  24
#define NS_STYLE_DISPLAY_INLINE_STACK           25
#define NS_STYLE_DISPLAY_DECK                   26
#define NS_STYLE_DISPLAY_POPUP                  27
#define NS_STYLE_DISPLAY_GROUPBOX               28
#endif
#define NS_STYLE_DISPLAY_FLEX                   29
#define NS_STYLE_DISPLAY_INLINE_FLEX            30
#define NS_STYLE_DISPLAY_GRID                   31
#define NS_STYLE_DISPLAY_INLINE_GRID            32

// See nsStylePosition
#define NS_STYLE_ALIGN_CONTENT_FLEX_START       0
#define NS_STYLE_ALIGN_CONTENT_FLEX_END         1
#define NS_STYLE_ALIGN_CONTENT_CENTER           2
#define NS_STYLE_ALIGN_CONTENT_SPACE_BETWEEN    3
#define NS_STYLE_ALIGN_CONTENT_SPACE_AROUND     4
#define NS_STYLE_ALIGN_CONTENT_STRETCH          5

// See nsStylePosition
#define NS_STYLE_ALIGN_ITEMS_FLEX_START         0
#define NS_STYLE_ALIGN_ITEMS_FLEX_END           1
#define NS_STYLE_ALIGN_ITEMS_CENTER             2
#define NS_STYLE_ALIGN_ITEMS_BASELINE           3
#define NS_STYLE_ALIGN_ITEMS_STRETCH            4

// For convenience/clarity (since we use this default value in multiple places)
#define NS_STYLE_ALIGN_ITEMS_INITIAL_VALUE      NS_STYLE_ALIGN_ITEMS_STRETCH

// The "align-self" property accepts all of the normal "align-items" values
// (above) plus a special 'auto' value that computes to the parent's
// "align-items" value. Our computed style code internally represents 'auto'
// with this enum until we actually evaluate it:
#define NS_STYLE_ALIGN_SELF_AUTO                5

// See nsStylePosition
#define NS_STYLE_FLEX_DIRECTION_ROW             0
#define NS_STYLE_FLEX_DIRECTION_ROW_REVERSE     1
#define NS_STYLE_FLEX_DIRECTION_COLUMN          2
#define NS_STYLE_FLEX_DIRECTION_COLUMN_REVERSE  3

// See nsStylePosition
#define NS_STYLE_FLEX_WRAP_NOWRAP               0
#define NS_STYLE_FLEX_WRAP_WRAP                 1
#define NS_STYLE_FLEX_WRAP_WRAP_REVERSE         2

// See nsStylePosition
// NOTE: This is the initial value of the integer-valued 'order' property
// (rather than an internal numerical representation of some keyword).
#define NS_STYLE_ORDER_INITIAL                  0

// See nsStylePosition
#define NS_STYLE_JUSTIFY_CONTENT_FLEX_START     0
#define NS_STYLE_JUSTIFY_CONTENT_FLEX_END       1
#define NS_STYLE_JUSTIFY_CONTENT_CENTER         2
#define NS_STYLE_JUSTIFY_CONTENT_SPACE_BETWEEN  3
#define NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND   4

// See nsStyleDisplay
#define NS_STYLE_FLOAT_NONE                     0
#define NS_STYLE_FLOAT_LEFT                     1
#define NS_STYLE_FLOAT_RIGHT                    2

// See nsStyleFilter
#define NS_STYLE_FILTER_NONE                    0
#define NS_STYLE_FILTER_URL                     1
#define NS_STYLE_FILTER_BLUR                    2
#define NS_STYLE_FILTER_BRIGHTNESS              3
#define NS_STYLE_FILTER_CONTRAST                4
#define NS_STYLE_FILTER_GRAYSCALE               5
#define NS_STYLE_FILTER_INVERT                  6
#define NS_STYLE_FILTER_OPACITY                 7
#define NS_STYLE_FILTER_SATURATE                8
#define NS_STYLE_FILTER_SEPIA                   9
#define NS_STYLE_FILTER_HUE_ROTATE              10
#define NS_STYLE_FILTER_DROP_SHADOW             11

// See nsStyleFont
// We should eventually stop using the NS_STYLE_* variants here.
#define NS_STYLE_FONT_STYLE_NORMAL              NS_FONT_STYLE_NORMAL
#define NS_STYLE_FONT_STYLE_ITALIC              NS_FONT_STYLE_ITALIC
#define NS_STYLE_FONT_STYLE_OBLIQUE             NS_FONT_STYLE_OBLIQUE

// See nsStyleFont
// We should eventually stop using the NS_STYLE_* variants here.
#define NS_STYLE_FONT_VARIANT_NORMAL            NS_FONT_VARIANT_NORMAL
#define NS_STYLE_FONT_VARIANT_SMALL_CAPS        NS_FONT_VARIANT_SMALL_CAPS

// See nsStyleFont
// We should eventually stop using the NS_STYLE_* variants here.
#define NS_STYLE_FONT_WEIGHT_NORMAL             NS_FONT_WEIGHT_NORMAL
#define NS_STYLE_FONT_WEIGHT_BOLD               NS_FONT_WEIGHT_BOLD
// The constants below appear only in style sheets and not computed style.
#define NS_STYLE_FONT_WEIGHT_BOLDER             (-1)
#define NS_STYLE_FONT_WEIGHT_LIGHTER            (-2)

// See nsStyleFont
#define NS_STYLE_FONT_SIZE_XXSMALL              0
#define NS_STYLE_FONT_SIZE_XSMALL               1
#define NS_STYLE_FONT_SIZE_SMALL                2
#define NS_STYLE_FONT_SIZE_MEDIUM               3
#define NS_STYLE_FONT_SIZE_LARGE                4
#define NS_STYLE_FONT_SIZE_XLARGE               5
#define NS_STYLE_FONT_SIZE_XXLARGE              6
#define NS_STYLE_FONT_SIZE_XXXLARGE             7  // Only used by <font size="7">. Not specifiable in CSS.
#define NS_STYLE_FONT_SIZE_LARGER               8
#define NS_STYLE_FONT_SIZE_SMALLER              9

// See nsStyleFont
// We should eventually stop using the NS_STYLE_* variants here.
#define NS_STYLE_FONT_STRETCH_ULTRA_CONDENSED   NS_FONT_STRETCH_ULTRA_CONDENSED
#define NS_STYLE_FONT_STRETCH_EXTRA_CONDENSED   NS_FONT_STRETCH_EXTRA_CONDENSED
#define NS_STYLE_FONT_STRETCH_CONDENSED         NS_FONT_STRETCH_CONDENSED
#define NS_STYLE_FONT_STRETCH_SEMI_CONDENSED    NS_FONT_STRETCH_SEMI_CONDENSED
#define NS_STYLE_FONT_STRETCH_NORMAL            NS_FONT_STRETCH_NORMAL
#define NS_STYLE_FONT_STRETCH_SEMI_EXPANDED     NS_FONT_STRETCH_SEMI_EXPANDED
#define NS_STYLE_FONT_STRETCH_EXPANDED          NS_FONT_STRETCH_EXPANDED
#define NS_STYLE_FONT_STRETCH_EXTRA_EXPANDED    NS_FONT_STRETCH_EXTRA_EXPANDED
#define NS_STYLE_FONT_STRETCH_ULTRA_EXPANDED    NS_FONT_STRETCH_ULTRA_EXPANDED

// See nsStyleFont - system fonts
#define NS_STYLE_FONT_CAPTION                   1   // css2
#define NS_STYLE_FONT_ICON                      2
#define NS_STYLE_FONT_MENU                      3
#define NS_STYLE_FONT_MESSAGE_BOX               4
#define NS_STYLE_FONT_SMALL_CAPTION             5
#define NS_STYLE_FONT_STATUS_BAR                6
#define NS_STYLE_FONT_WINDOW                    7   // css3
#define NS_STYLE_FONT_DOCUMENT                  8
#define NS_STYLE_FONT_WORKSPACE                 9
#define NS_STYLE_FONT_DESKTOP                   10
#define NS_STYLE_FONT_INFO                      11
#define NS_STYLE_FONT_DIALOG                    12
#define NS_STYLE_FONT_BUTTON                    13
#define NS_STYLE_FONT_PULL_DOWN_MENU            14
#define NS_STYLE_FONT_LIST                      15
#define NS_STYLE_FONT_FIELD                     16

// grid-auto-flow keywords
#define NS_STYLE_GRID_AUTO_FLOW_STACK           (1 << 0)
#define NS_STYLE_GRID_AUTO_FLOW_ROW             (1 << 1)
#define NS_STYLE_GRID_AUTO_FLOW_COLUMN          (1 << 2)
#define NS_STYLE_GRID_AUTO_FLOW_DENSE           (1 << 3)

// 'subgrid' keyword in grid-template-{columns,rows}
#define NS_STYLE_GRID_TEMPLATE_SUBGRID          0

// CSS Grid <track-breadth> keywords
// Should not overlap with NS_STYLE_GRID_TEMPLATE_SUBGRID
#define NS_STYLE_GRID_TRACK_BREADTH_MAX_CONTENT 1
#define NS_STYLE_GRID_TRACK_BREADTH_MIN_CONTENT 2

// defaults per MathML spec
#define NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER 0.71f
#define NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT 8

// See nsStyleFont
#define NS_MATHML_MATHVARIANT_NONE                     0
#define NS_MATHML_MATHVARIANT_NORMAL                   1
#define NS_MATHML_MATHVARIANT_BOLD                     2
#define NS_MATHML_MATHVARIANT_ITALIC                   3
#define NS_MATHML_MATHVARIANT_BOLD_ITALIC              4
#define NS_MATHML_MATHVARIANT_SCRIPT                   5
#define NS_MATHML_MATHVARIANT_BOLD_SCRIPT              6
#define NS_MATHML_MATHVARIANT_FRAKTUR                  7
#define NS_MATHML_MATHVARIANT_DOUBLE_STRUCK            8
#define NS_MATHML_MATHVARIANT_BOLD_FRAKTUR             9
#define NS_MATHML_MATHVARIANT_SANS_SERIF              10
#define NS_MATHML_MATHVARIANT_BOLD_SANS_SERIF         11
#define NS_MATHML_MATHVARIANT_SANS_SERIF_ITALIC       12
#define NS_MATHML_MATHVARIANT_SANS_SERIF_BOLD_ITALIC  13
#define NS_MATHML_MATHVARIANT_MONOSPACE               14
#define NS_MATHML_MATHVARIANT_INITIAL                 15
#define NS_MATHML_MATHVARIANT_TAILED                  16
#define NS_MATHML_MATHVARIANT_LOOPED                  17
#define NS_MATHML_MATHVARIANT_STRETCHED               18

// See nsStyleFont::mMathDisplay
#define NS_MATHML_DISPLAYSTYLE_INLINE           0
#define NS_MATHML_DISPLAYSTYLE_BLOCK            1

// See nsStylePosition::mWidth, mMinWidth, mMaxWidth
#define NS_STYLE_WIDTH_MAX_CONTENT              0
#define NS_STYLE_WIDTH_MIN_CONTENT              1
#define NS_STYLE_WIDTH_FIT_CONTENT              2
#define NS_STYLE_WIDTH_AVAILABLE                3

// See nsStyleDisplay.mPosition
#define NS_STYLE_POSITION_STATIC                0
#define NS_STYLE_POSITION_RELATIVE              1
#define NS_STYLE_POSITION_ABSOLUTE              2
#define NS_STYLE_POSITION_FIXED                 3
#define NS_STYLE_POSITION_STICKY                4

// See nsStyleDisplay.mClip
#define NS_STYLE_CLIP_AUTO                      0x00
#define NS_STYLE_CLIP_RECT                      0x01
#define NS_STYLE_CLIP_TYPE_MASK                 0x0F
#define NS_STYLE_CLIP_LEFT_AUTO                 0x10
#define NS_STYLE_CLIP_TOP_AUTO                  0x20
#define NS_STYLE_CLIP_RIGHT_AUTO                0x40
#define NS_STYLE_CLIP_BOTTOM_AUTO               0x80

// FRAME/FRAMESET/IFRAME specific values including backward compatibility. Boolean values with
// the same meaning (e.g. 1 & yes) may need to be distinguished for correct mode processing
#define NS_STYLE_FRAME_YES                      0
#define NS_STYLE_FRAME_NO                       1
#define NS_STYLE_FRAME_0                        2
#define NS_STYLE_FRAME_1                        3
#define NS_STYLE_FRAME_ON                       4
#define NS_STYLE_FRAME_OFF                      5
#define NS_STYLE_FRAME_AUTO                     6
#define NS_STYLE_FRAME_SCROLL                   7
#define NS_STYLE_FRAME_NOSCROLL                 8

// See nsStyleDisplay.mOverflow
#define NS_STYLE_OVERFLOW_VISIBLE               0
#define NS_STYLE_OVERFLOW_HIDDEN                1
#define NS_STYLE_OVERFLOW_SCROLL                2
#define NS_STYLE_OVERFLOW_AUTO                  3
#define NS_STYLE_OVERFLOW_CLIP                  4
#define NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL 5
#define NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL   6

// See nsStyleDisplay.mOverflowClipBox
#define NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX  0
#define NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX  1

// See nsStyleList
#define NS_STYLE_LIST_STYLE_CUSTOM                -1 // for @counter-style
#define NS_STYLE_LIST_STYLE_NONE                  0
#define NS_STYLE_LIST_STYLE_DISC                  1
#define NS_STYLE_LIST_STYLE_CIRCLE                2
#define NS_STYLE_LIST_STYLE_SQUARE                3
#define NS_STYLE_LIST_STYLE_DECIMAL               4
#define NS_STYLE_LIST_STYLE_HEBREW                5
#define NS_STYLE_LIST_STYLE_JAPANESE_INFORMAL     6
#define NS_STYLE_LIST_STYLE_JAPANESE_FORMAL       7
#define NS_STYLE_LIST_STYLE_KOREAN_HANGUL_FORMAL  8
#define NS_STYLE_LIST_STYLE_KOREAN_HANJA_INFORMAL 9
#define NS_STYLE_LIST_STYLE_KOREAN_HANJA_FORMAL   10
#define NS_STYLE_LIST_STYLE_SIMP_CHINESE_INFORMAL 11
#define NS_STYLE_LIST_STYLE_SIMP_CHINESE_FORMAL   12
#define NS_STYLE_LIST_STYLE_TRAD_CHINESE_INFORMAL 13
#define NS_STYLE_LIST_STYLE_TRAD_CHINESE_FORMAL   14
#define NS_STYLE_LIST_STYLE_ETHIOPIC_NUMERIC      15
#define NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED     16
#define NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN       17
#define NS_STYLE_LIST_STYLE__MAX                  18
// These styles are handled as custom styles defined in counterstyles.css.
// They are preserved here only for html attribute map.
#define NS_STYLE_LIST_STYLE_LOWER_ROMAN           100
#define NS_STYLE_LIST_STYLE_UPPER_ROMAN           101
#define NS_STYLE_LIST_STYLE_LOWER_ALPHA           102
#define NS_STYLE_LIST_STYLE_UPPER_ALPHA           103

// See nsStyleList
#define NS_STYLE_LIST_STYLE_POSITION_INSIDE     0
#define NS_STYLE_LIST_STYLE_POSITION_OUTSIDE    1

// See nsStyleMargin
#define NS_STYLE_MARGIN_SIZE_AUTO               0

// See nsStyleVisibility
#define NS_STYLE_POINTER_EVENTS_NONE            0
#define NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED  1
#define NS_STYLE_POINTER_EVENTS_VISIBLEFILL     2
#define NS_STYLE_POINTER_EVENTS_VISIBLESTROKE   3
#define NS_STYLE_POINTER_EVENTS_VISIBLE         4
#define NS_STYLE_POINTER_EVENTS_PAINTED         5
#define NS_STYLE_POINTER_EVENTS_FILL            6
#define NS_STYLE_POINTER_EVENTS_STROKE          7
#define NS_STYLE_POINTER_EVENTS_ALL             8
#define NS_STYLE_POINTER_EVENTS_AUTO            9

// See nsStyleVisibility.mImageOrientationType
#define NS_STYLE_IMAGE_ORIENTATION_FLIP         0
#define NS_STYLE_IMAGE_ORIENTATION_FROM_IMAGE   1

// See nsStyleDisplay
#define NS_STYLE_RESIZE_NONE                    0
#define NS_STYLE_RESIZE_BOTH                    1
#define NS_STYLE_RESIZE_HORIZONTAL              2
#define NS_STYLE_RESIZE_VERTICAL                3

// See nsStyleText
#define NS_STYLE_TEXT_ALIGN_DEFAULT               0
#define NS_STYLE_TEXT_ALIGN_LEFT                  1
#define NS_STYLE_TEXT_ALIGN_RIGHT                 2
#define NS_STYLE_TEXT_ALIGN_CENTER                3
#define NS_STYLE_TEXT_ALIGN_JUSTIFY               4
#define NS_STYLE_TEXT_ALIGN_CHAR                  5   //align based on a certain character, for table cell
#define NS_STYLE_TEXT_ALIGN_END                   6
#define NS_STYLE_TEXT_ALIGN_AUTO                  7
#define NS_STYLE_TEXT_ALIGN_MOZ_CENTER            8
#define NS_STYLE_TEXT_ALIGN_MOZ_RIGHT             9
#define NS_STYLE_TEXT_ALIGN_MOZ_LEFT             10
// NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT is only used in data structs; it
// is never present in stylesheets or computed data.
#define NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT 11
#define NS_STYLE_TEXT_ALIGN_TRUE                  12
// Note: make sure that the largest NS_STYLE_TEXT_ALIGN_* value is smaller than
// the smallest NS_STYLE_VERTICAL_ALIGN_* value below!

// See nsStyleText, nsStyleFont
#define NS_STYLE_TEXT_DECORATION_LINE_NONE         0
#define NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE    NS_FONT_DECORATION_UNDERLINE
#define NS_STYLE_TEXT_DECORATION_LINE_OVERLINE     NS_FONT_DECORATION_OVERLINE
#define NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH NS_FONT_DECORATION_LINE_THROUGH
#define NS_STYLE_TEXT_DECORATION_LINE_BLINK        0x08
#define NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS 0x10
// OVERRIDE_ALL does not occur in stylesheets; it only comes from HTML
// attribute mapping (and thus appears in computed data)
#define NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL 0x20
#define NS_STYLE_TEXT_DECORATION_LINE_LINES_MASK   (NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE | NS_STYLE_TEXT_DECORATION_LINE_OVERLINE | NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH)

// See nsStyleText
#define NS_STYLE_TEXT_DECORATION_STYLE_NONE     0 // not in CSS spec, mapped to -moz-none
#define NS_STYLE_TEXT_DECORATION_STYLE_DOTTED   1
#define NS_STYLE_TEXT_DECORATION_STYLE_DASHED   2
#define NS_STYLE_TEXT_DECORATION_STYLE_SOLID    3
#define NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE   4
#define NS_STYLE_TEXT_DECORATION_STYLE_WAVY     5
#define NS_STYLE_TEXT_DECORATION_STYLE_MAX      NS_STYLE_TEXT_DECORATION_STYLE_WAVY

// See nsStyleTextOverflow
#define NS_STYLE_TEXT_OVERFLOW_CLIP     0
#define NS_STYLE_TEXT_OVERFLOW_ELLIPSIS 1
#define NS_STYLE_TEXT_OVERFLOW_STRING   2

// See nsStyleText
#define NS_STYLE_TEXT_TRANSFORM_NONE            0
#define NS_STYLE_TEXT_TRANSFORM_CAPITALIZE      1
#define NS_STYLE_TEXT_TRANSFORM_LOWERCASE       2
#define NS_STYLE_TEXT_TRANSFORM_UPPERCASE       3
#define NS_STYLE_TEXT_TRANSFORM_FULLWIDTH       4

// See nsStyleDisplay
#define NS_STYLE_TOUCH_ACTION_NONE            (1 << 0)
#define NS_STYLE_TOUCH_ACTION_AUTO            (1 << 1)
#define NS_STYLE_TOUCH_ACTION_PAN_X           (1 << 2)
#define NS_STYLE_TOUCH_ACTION_PAN_Y           (1 << 3)
#define NS_STYLE_TOUCH_ACTION_MANIPULATION    (1 << 4)

// See nsStyleDisplay
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE         0
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR       1
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN      2
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_OUT     3
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN_OUT  4
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START   5
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END     6

// See nsStyleText
// Note: these values pickup after the text-align values because there
// are a few html cases where an object can have both types of
// alignment applied with a single attribute
#define NS_STYLE_VERTICAL_ALIGN_BASELINE             13
#define NS_STYLE_VERTICAL_ALIGN_SUB                  14
#define NS_STYLE_VERTICAL_ALIGN_SUPER                15
#define NS_STYLE_VERTICAL_ALIGN_TOP                  16
#define NS_STYLE_VERTICAL_ALIGN_TEXT_TOP             17
#define NS_STYLE_VERTICAL_ALIGN_MIDDLE               18
#define NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM          19
#define NS_STYLE_VERTICAL_ALIGN_BOTTOM               20
#define NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE 21

// See nsStyleVisibility
#define NS_STYLE_VISIBILITY_HIDDEN              0
#define NS_STYLE_VISIBILITY_VISIBLE             1
#define NS_STYLE_VISIBILITY_COLLAPSE            2

// See nsStyleText
#define NS_STYLE_TABSIZE_INITIAL                8

// See nsStyleText
#define NS_STYLE_WHITESPACE_NORMAL               0
#define NS_STYLE_WHITESPACE_PRE                  1
#define NS_STYLE_WHITESPACE_NOWRAP               2
#define NS_STYLE_WHITESPACE_PRE_WRAP             3
#define NS_STYLE_WHITESPACE_PRE_LINE             4
#define NS_STYLE_WHITESPACE_PRE_SPACE            5

// See nsStyleText
#define NS_STYLE_WORDBREAK_NORMAL               0
#define NS_STYLE_WORDBREAK_BREAK_ALL            1
#define NS_STYLE_WORDBREAK_KEEP_ALL             2

// See nsStyleText
#define NS_STYLE_WORDWRAP_NORMAL                0
#define NS_STYLE_WORDWRAP_BREAK_WORD            1

// See nsStyleText
#define NS_STYLE_HYPHENS_NONE                   0
#define NS_STYLE_HYPHENS_MANUAL                 1
#define NS_STYLE_HYPHENS_AUTO                   2

// See nsStyleText
#define NS_STYLE_TEXT_SIZE_ADJUST_NONE          0
#define NS_STYLE_TEXT_SIZE_ADJUST_AUTO          1

// See nsStyleText
#define NS_STYLE_TEXT_ORIENTATION_AUTO          0
#define NS_STYLE_TEXT_ORIENTATION_UPRIGHT       1
#define NS_STYLE_TEXT_ORIENTATION_SIDEWAYS      2

// See nsStyleText
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE        0
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL         1
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_2    2
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_3    3
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_4    4

// See nsStyleText
#define NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT       0

// See nsStyleText
#define NS_STYLE_UNICODE_BIDI_NORMAL            0x0
#define NS_STYLE_UNICODE_BIDI_EMBED             0x1
#define NS_STYLE_UNICODE_BIDI_ISOLATE           0x2
#define NS_STYLE_UNICODE_BIDI_OVERRIDE          0x4
#define NS_STYLE_UNICODE_BIDI_ISOLATE_OVERRIDE  0x6
#define NS_STYLE_UNICODE_BIDI_PLAINTEXT         0x8

// See nsStyleTable (here for HTML 4.0 for now, should probably change to side flags)
#define NS_STYLE_TABLE_FRAME_NONE               0
#define NS_STYLE_TABLE_FRAME_ABOVE              1
#define NS_STYLE_TABLE_FRAME_BELOW              2
#define NS_STYLE_TABLE_FRAME_HSIDES             3
#define NS_STYLE_TABLE_FRAME_VSIDES             4
#define NS_STYLE_TABLE_FRAME_LEFT               5
#define NS_STYLE_TABLE_FRAME_RIGHT              6
#define NS_STYLE_TABLE_FRAME_BOX                7
#define NS_STYLE_TABLE_FRAME_BORDER             8

// See nsStyleTable
#define NS_STYLE_TABLE_RULES_NONE               0
#define NS_STYLE_TABLE_RULES_GROUPS             1
#define NS_STYLE_TABLE_RULES_ROWS               2
#define NS_STYLE_TABLE_RULES_COLS               3
#define NS_STYLE_TABLE_RULES_ALL                4

#define NS_STYLE_TABLE_LAYOUT_AUTO              0
#define NS_STYLE_TABLE_LAYOUT_FIXED             1

#define NS_STYLE_TABLE_EMPTY_CELLS_HIDE            0
#define NS_STYLE_TABLE_EMPTY_CELLS_SHOW            1
#define NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND 2

#define NS_STYLE_CAPTION_SIDE_TOP               0
#define NS_STYLE_CAPTION_SIDE_RIGHT             1
#define NS_STYLE_CAPTION_SIDE_BOTTOM            2
#define NS_STYLE_CAPTION_SIDE_LEFT              3
#define NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE       4
#define NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE    5

// constants for cell "scope" attribute
#define NS_STYLE_CELL_SCOPE_ROW                 0
#define NS_STYLE_CELL_SCOPE_COL                 1
#define NS_STYLE_CELL_SCOPE_ROWGROUP            2
#define NS_STYLE_CELL_SCOPE_COLGROUP            3

// See nsStylePage
#define NS_STYLE_PAGE_MARKS_NONE                0x00
#define NS_STYLE_PAGE_MARKS_CROP                0x01
#define NS_STYLE_PAGE_MARKS_REGISTER            0x02

// See nsStylePage
#define NS_STYLE_PAGE_SIZE_AUTO                 0
#define NS_STYLE_PAGE_SIZE_PORTRAIT             1
#define NS_STYLE_PAGE_SIZE_LANDSCAPE            2

// See nsStyleBreaks
#define NS_STYLE_PAGE_BREAK_AUTO                0
#define NS_STYLE_PAGE_BREAK_ALWAYS              1
#define NS_STYLE_PAGE_BREAK_AVOID               2
#define NS_STYLE_PAGE_BREAK_LEFT                3
#define NS_STYLE_PAGE_BREAK_RIGHT               4

// See nsStyleColumn
#define NS_STYLE_COLUMN_COUNT_AUTO              0
#define NS_STYLE_COLUMN_COUNT_UNLIMITED         (-1)

#define NS_STYLE_COLUMN_FILL_AUTO               0
#define NS_STYLE_COLUMN_FILL_BALANCE            1

// See nsStyleUIReset
#define NS_STYLE_IME_MODE_AUTO                  0
#define NS_STYLE_IME_MODE_NORMAL                1
#define NS_STYLE_IME_MODE_ACTIVE                2
#define NS_STYLE_IME_MODE_DISABLED              3
#define NS_STYLE_IME_MODE_INACTIVE              4

// See nsStyleGradient
#define NS_STYLE_GRADIENT_SHAPE_LINEAR          0
#define NS_STYLE_GRADIENT_SHAPE_ELLIPTICAL      1
#define NS_STYLE_GRADIENT_SHAPE_CIRCULAR        2

#define NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE     0
#define NS_STYLE_GRADIENT_SIZE_CLOSEST_CORNER   1
#define NS_STYLE_GRADIENT_SIZE_FARTHEST_SIDE    2
#define NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER  3
#define NS_STYLE_GRADIENT_SIZE_EXPLICIT_SIZE    4

// See nsStyleSVG

// dominant-baseline
#define NS_STYLE_DOMINANT_BASELINE_AUTO              0
#define NS_STYLE_DOMINANT_BASELINE_USE_SCRIPT        1
#define NS_STYLE_DOMINANT_BASELINE_NO_CHANGE         2
#define NS_STYLE_DOMINANT_BASELINE_RESET_SIZE        3
#define NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC       4
#define NS_STYLE_DOMINANT_BASELINE_ALPHABETIC        5
#define NS_STYLE_DOMINANT_BASELINE_HANGING           6
#define NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL      7
#define NS_STYLE_DOMINANT_BASELINE_CENTRAL           8
#define NS_STYLE_DOMINANT_BASELINE_MIDDLE            9
#define NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE  10
#define NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE 11

// fill-rule
#define NS_STYLE_FILL_RULE_NONZERO              0
#define NS_STYLE_FILL_RULE_EVENODD              1

// image-rendering
#define NS_STYLE_IMAGE_RENDERING_AUTO             0
#define NS_STYLE_IMAGE_RENDERING_OPTIMIZESPEED    1
#define NS_STYLE_IMAGE_RENDERING_OPTIMIZEQUALITY  2
#define NS_STYLE_IMAGE_RENDERING_CRISPEDGES       3

// mask-type
#define NS_STYLE_MASK_TYPE_LUMINANCE            0
#define NS_STYLE_MASK_TYPE_ALPHA                1

// paint-order
#define NS_STYLE_PAINT_ORDER_NORMAL             0
#define NS_STYLE_PAINT_ORDER_FILL               1
#define NS_STYLE_PAINT_ORDER_STROKE             2
#define NS_STYLE_PAINT_ORDER_MARKERS            3
#define NS_STYLE_PAINT_ORDER_LAST_VALUE NS_STYLE_PAINT_ORDER_MARKERS
// NS_STYLE_PAINT_ORDER_BITWIDTH is the number of bits required to store
// a single paint-order component value.
#define NS_STYLE_PAINT_ORDER_BITWIDTH           2

// shape-rendering
#define NS_STYLE_SHAPE_RENDERING_AUTO               0
#define NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED      1
#define NS_STYLE_SHAPE_RENDERING_CRISPEDGES         2
#define NS_STYLE_SHAPE_RENDERING_GEOMETRICPRECISION 3

// stroke-linecap
#define NS_STYLE_STROKE_LINECAP_BUTT            0
#define NS_STYLE_STROKE_LINECAP_ROUND           1
#define NS_STYLE_STROKE_LINECAP_SQUARE          2

// stroke-linejoin
#define NS_STYLE_STROKE_LINEJOIN_MITER          0
#define NS_STYLE_STROKE_LINEJOIN_ROUND          1
#define NS_STYLE_STROKE_LINEJOIN_BEVEL          2

// stroke-dasharray, stroke-dashoffset, stroke-width
#define NS_STYLE_STROKE_PROP_CONTEXT_VALUE      0

// text-anchor
#define NS_STYLE_TEXT_ANCHOR_START              0
#define NS_STYLE_TEXT_ANCHOR_MIDDLE             1
#define NS_STYLE_TEXT_ANCHOR_END                2

// text-rendering
#define NS_STYLE_TEXT_RENDERING_AUTO               0
#define NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED      1
#define NS_STYLE_TEXT_RENDERING_OPTIMIZELEGIBILITY 2
#define NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION 3

// color-interpolation and color-interpolation-filters
#define NS_STYLE_COLOR_INTERPOLATION_AUTO           0
#define NS_STYLE_COLOR_INTERPOLATION_SRGB           1
#define NS_STYLE_COLOR_INTERPOLATION_LINEARRGB      2

// vector-effect
#define NS_STYLE_VECTOR_EFFECT_NONE                 0
#define NS_STYLE_VECTOR_EFFECT_NON_SCALING_STROKE   1

// 3d Transforms - Backface visibility
#define NS_STYLE_BACKFACE_VISIBILITY_VISIBLE        1
#define NS_STYLE_BACKFACE_VISIBILITY_HIDDEN         0

#define NS_STYLE_TRANSFORM_STYLE_FLAT               0
#define NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D        1

// object {fill,stroke}-opacity inherited from context for SVG glyphs
#define NS_STYLE_CONTEXT_FILL_OPACITY               0
#define NS_STYLE_CONTEXT_STROKE_OPACITY             1

// blending
#define NS_STYLE_BLEND_NORMAL                       0
#define NS_STYLE_BLEND_MULTIPLY                     1
#define NS_STYLE_BLEND_SCREEN                       2
#define NS_STYLE_BLEND_OVERLAY                      3
#define NS_STYLE_BLEND_DARKEN                       4
#define NS_STYLE_BLEND_LIGHTEN                      5
#define NS_STYLE_BLEND_COLOR_DODGE                  6
#define NS_STYLE_BLEND_COLOR_BURN                   7
#define NS_STYLE_BLEND_HARD_LIGHT                   8
#define NS_STYLE_BLEND_SOFT_LIGHT                   9
#define NS_STYLE_BLEND_DIFFERENCE                   10
#define NS_STYLE_BLEND_EXCLUSION                    11
#define NS_STYLE_BLEND_HUE                          12
#define NS_STYLE_BLEND_SATURATION                   13
#define NS_STYLE_BLEND_COLOR                        14
#define NS_STYLE_BLEND_LUMINOSITY                   15

// See nsStyleText::mControlCharacterVisibility
#define NS_STYLE_CONTROL_CHARACTER_VISIBILITY_HIDDEN  0
#define NS_STYLE_CONTROL_CHARACTER_VISIBILITY_VISIBLE 1

// counter system
#define NS_STYLE_COUNTER_SYSTEM_CYCLIC      0
#define NS_STYLE_COUNTER_SYSTEM_NUMERIC     1
#define NS_STYLE_COUNTER_SYSTEM_ALPHABETIC  2
#define NS_STYLE_COUNTER_SYSTEM_SYMBOLIC    3
#define NS_STYLE_COUNTER_SYSTEM_ADDITIVE    4
#define NS_STYLE_COUNTER_SYSTEM_FIXED       5
#define NS_STYLE_COUNTER_SYSTEM_EXTENDS     6

#define NS_STYLE_COUNTER_RANGE_INFINITE     0

#define NS_STYLE_COUNTER_SPEAKAS_BULLETS    0
#define NS_STYLE_COUNTER_SPEAKAS_NUMBERS    1
#define NS_STYLE_COUNTER_SPEAKAS_WORDS      2
#define NS_STYLE_COUNTER_SPEAKAS_SPELL_OUT  3
#define NS_STYLE_COUNTER_SPEAKAS_OTHER      255 // refer to another style

/*****************************************************************************
 * Constants for media features.                                             *
 *****************************************************************************/

// orientation
#define NS_STYLE_ORIENTATION_PORTRAIT           0
#define NS_STYLE_ORIENTATION_LANDSCAPE          1

// scan
#define NS_STYLE_SCAN_PROGRESSIVE               0
#define NS_STYLE_SCAN_INTERLACE                 1

#endif /* nsStyleConsts_h___ */
