/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

MARKUPMAP(math, New_HyperText, roles::MATHML_MATH)

MARKUPMAP(mi, New_HyperText, roles::MATHML_IDENTIFIER)

MARKUPMAP(mn, New_HyperText, roles::MATHML_NUMBER)

MARKUPMAP(mo, New_HyperText, roles::MATHML_OPERATOR,
          AttrFromDOM(accent, accent), AttrFromDOM(fence, fence),
          AttrFromDOM(separator, separator), AttrFromDOM(largeop, largeop))

MARKUPMAP(mtext, New_HyperText, roles::MATHML_TEXT)

MARKUPMAP(ms, New_HyperText, roles::MATHML_STRING_LITERAL)

MARKUPMAP(mglyph, New_HyperText, roles::MATHML_GLYPH)

MARKUPMAP(mrow, New_HyperText, roles::MATHML_ROW)

MARKUPMAP(mfrac, New_HyperText, roles::MATHML_FRACTION,
          AttrFromDOM(bevelled, bevelled),
          AttrFromDOM(linethickness, linethickness))

MARKUPMAP(msqrt, New_HyperText, roles::MATHML_SQUARE_ROOT)

MARKUPMAP(mroot, New_HyperText, roles::MATHML_ROOT)

MARKUPMAP(mfenced, New_HyperText, roles::MATHML_ROW)

MARKUPMAP(menclose, New_HyperText, roles::MATHML_ENCLOSED,
          AttrFromDOM(notation, notation))

MARKUPMAP(mstyle, New_HyperText, roles::MATHML_STYLE)

MARKUPMAP(msub, New_HyperText, roles::MATHML_SUB)

MARKUPMAP(msup, New_HyperText, roles::MATHML_SUP)

MARKUPMAP(msubsup, New_HyperText, roles::MATHML_SUB_SUP)

MARKUPMAP(munder, New_HyperText, roles::MATHML_UNDER,
          AttrFromDOM(accentunder, accentunder), AttrFromDOM(align, align))

MARKUPMAP(mover, New_HyperText, roles::MATHML_OVER, AttrFromDOM(accent, accent),
          AttrFromDOM(align, align))

MARKUPMAP(munderover, New_HyperText, roles::MATHML_UNDER_OVER,
          AttrFromDOM(accent, accent), AttrFromDOM(accentunder, accentunder),
          AttrFromDOM(align, align))

MARKUPMAP(mmultiscripts, New_HyperText, roles::MATHML_MULTISCRIPTS)

MARKUPMAP(
    mtable,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLTableAccessible(aElement, aContext->Document());
    },
    roles::MATHML_TABLE, AttrFromDOM(align, align),
    AttrFromDOM(columnlines, columnlines), AttrFromDOM(rowlines, rowlines))

MARKUPMAP(
    mlabeledtr,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLTableRowAccessible(aElement, aContext->Document());
    },
    roles::MATHML_LABELED_ROW)

MARKUPMAP(
    mtr,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLTableRowAccessible(aElement, aContext->Document());
    },
    roles::MATHML_TABLE_ROW)

MARKUPMAP(
    mtd,
    [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* {
      return new HTMLTableCellAccessible(aElement, aContext->Document());
    },
    0)

MARKUPMAP(maction, New_HyperText, roles::MATHML_ACTION,
          AttrFromDOM(actiontype, actiontype),
          AttrFromDOM(selection, selection))

MARKUPMAP(merror, New_HyperText, roles::MATHML_ERROR)

MARKUPMAP(mstack, New_HyperText, roles::MATHML_STACK, AttrFromDOM(align, align),
          AttrFromDOM(position, position))

MARKUPMAP(mlongdiv, New_HyperText, roles::MATHML_LONG_DIVISION,
          AttrFromDOM(longdivstyle, longdivstyle))

MARKUPMAP(msgroup, New_HyperText, roles::MATHML_STACK_GROUP,
          AttrFromDOM(position, position), AttrFromDOM(shift, shift))

MARKUPMAP(msrow, New_HyperText, roles::MATHML_STACK_ROW,
          AttrFromDOM(position, position))

MARKUPMAP(mscarries, New_HyperText, roles::MATHML_STACK_CARRIES,
          AttrFromDOM(location, location), AttrFromDOM(position, position))

MARKUPMAP(mscarry, New_HyperText, roles::MATHML_STACK_CARRY,
          AttrFromDOM(crossout, crossout))

MARKUPMAP(msline, New_HyperText, roles::MATHML_STACK_LINE,
          AttrFromDOM(position, position))
