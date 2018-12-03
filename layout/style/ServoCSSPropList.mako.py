# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

def _assign_slots(obj, args):
    for i, attr in enumerate(obj.__slots__):
        setattr(obj, attr, args[i])


class Longhand(object):
    __slots__ = ["name", "method", "id", "flags", "pref"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "longhand"


class Shorthand(object):
    __slots__ = ["name", "method", "id", "flags", "pref", "subprops"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "shorthand"


class Alias(object):
    __slots__ = ["name", "method", "alias_id", "prop_id", "flags", "pref"]

    def __init__(self, *args):
        _assign_slots(self, args)

    @staticmethod
    def type():
        return "alias"

<%!
# See bug 1454823 for situation of internal flag.
def is_internal(prop):
    # A property which is not controlled by pref and not enabled in
    # content by default is an internal property.
    if not prop.gecko_pref and not prop.enabled_in_content():
        return True
    # There are some special cases we may want to remove eventually.
    OTHER_INTERNALS = [
        "-moz-context-properties",
        "-moz-control-character-visibility",
    ]
    return prop.name in OTHER_INTERNALS

def method(prop):
    if prop.name == "float":
        return "CssFloat"
    if prop.name.startswith("-x-"):
        return prop.camel_case[1:]
    return prop.camel_case

# Colors, integers and lengths are easy as well.
#
# TODO(emilio): This will go away once the rest of the longhands have been
# moved or perhaps using a blacklist for the ones with non-layout-dependence
# but other non-trivial dependence like scrollbar colors.
SERIALIZED_PREDEFINED_TYPES = [
    "Appearance",
    "BackgroundRepeat",
    "BackgroundSize",
    "BorderImageRepeat",
    "BorderStyle",
    "BreakBetween",
    "BreakWithin",
    "Clear",
    "ClipRectOrAuto",
    "Color",
    "Content",
    "CounterIncrement",
    "CounterReset",
    "FillRule",
    "Float",
    "FontFamily",
    "FontFeatureSettings",
    "FontLanguageOverride",
    "FontSize",
    "FontSizeAdjust",
    "FontStretch",
    "FontStyle",
    "FontSynthesis",
    "FontVariant",
    "FontVariantAlternates",
    "FontVariantEastAsian",
    "FontVariantLigatures",
    "FontVariantNumeric",
    "FontVariationSettings",
    "FontWeight",
    "Integer",
    "ImageLayer",
    "Length",
    "LengthOrPercentage",
    "NonNegativeLength",
    "NonNegativeLengthOrPercentage",
    "ListStyleType",
    "OffsetPath",
    "Opacity",
    "OutlineStyle",
    "OverflowWrap",
    "Position",
    "Quotes",
    "Resize",
    "Rotate",
    "Scale",
    "TextAlign",
    "Translate",
    "TimingFunction",
    "TransformStyle",
    "UserSelect",
    "background::BackgroundSize",
    "basic_shape::ClippingShape",
    "basic_shape::FloatAreaShape",
    "position::HorizontalPosition",
    "position::VerticalPosition",
    "url::ImageUrlOrNone",
]

def serialized_by_servo(prop):
    # If the property requires layout information, no such luck.
    if "GETCS_NEEDS_LAYOUT_FLUSH" in prop.flags:
        return False
    if prop.type() == "shorthand":
        # FIXME: Need to serialize a value interpolated with currentcolor
        # properly to be able to use text-decoration, and figure out what to do
        # with relative mask urls.
        return prop.name != "text-decoration" and prop.name != "mask"
    # Keywords are all fine, except -moz-osx-font-smoothing, which does
    # resistfingerprinting stuff.
    if prop.keyword and prop.name != "-moz-osx-font-smoothing":
        return True
    if prop.predefined_type in SERIALIZED_PREDEFINED_TYPES:
        return True
    # TODO(emilio): Enable the rest of the longhands.
    return False

def exposed_on_getcs(prop):
    if prop.type() == "longhand":
        return not is_internal(prop)
    # TODO: bug 137688 / https://github.com/w3c/csswg-drafts/issues/2529
    if prop.type() == "shorthand":
        return "SHORTHAND_IN_GETCS" in prop.flags

def flags(prop):
    result = []
    if prop.explicitly_enabled_in_chrome():
        result.append("EnabledInUASheetsAndChrome")
    elif prop.explicitly_enabled_in_ua_sheets():
        result.append("EnabledInUASheets")
    if is_internal(prop):
        result.append("Internal")
    if prop.enabled_in == "":
        result.append("Inaccessible")
    if "GETCS_NEEDS_LAYOUT_FLUSH" in prop.flags:
        result.append("GetCSNeedsLayoutFlush")
    if "CAN_ANIMATE_ON_COMPOSITOR" in prop.flags:
        result.append("CanAnimateOnCompositor")
    if exposed_on_getcs(prop):
        result.append("ExposedOnGetCS")
        if serialized_by_servo(prop):
            result.append("SerializedByServo")
    if prop.type() == "longhand" and prop.logical:
        result.append("IsLogical")
    return ", ".join('"{}"'.format(flag) for flag in result)

def pref(prop):
    if prop.gecko_pref:
        return '"' + prop.gecko_pref + '"'
    return '""'

def sub_properties(prop):
    return ", ".join('"{}"'.format(p.ident) for p in prop.sub_properties)
%>

data = [
    % for prop in data.longhands:
    Longhand("${prop.name}", "${method(prop)}", "${prop.ident}", [${flags(prop)}], ${pref(prop)}),
    % endfor

    % for prop in data.shorthands:
    Shorthand("${prop.name}", "${prop.camel_case}", "${prop.ident}", [${flags(prop)}], ${pref(prop)},
              [${sub_properties(prop)}]),
    % endfor

    % for prop in data.all_aliases():
    Alias("${prop.name}", "${prop.camel_case}", "${prop.ident}", "${prop.original.ident}", [], ${pref(prop)}),
    % endfor
]
