/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "nsAccessibilityService.h"
#include "LocalAccessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "nsMai.h"
#include "RemoteAccessible.h"
#include "AccAttributes.h"

#include "nsIAccessibleTypes.h"
#include "nsISimpleEnumerator.h"
#include "nsUTF8Utils.h"

#include "mozilla/Likely.h"

#include "DOMtoATK.h"

using namespace mozilla;
using namespace mozilla::a11y;

static const char* sAtkTextAttrNames[ATK_TEXT_ATTR_LAST_DEFINED];

static AtkAttributeSet* ConvertToAtkTextAttributeSet(
    AccAttributes* aAttributes) {
  if (!aAttributes) {
    // This can happen if an Accessible dies in the content process, but the
    // parent hasn't been udpated yet.
    return nullptr;
  }

  AtkAttributeSet* atkAttributeSet = nullptr;

  for (auto iter : *aAttributes) {
    AtkAttribute* atkAttr = (AtkAttribute*)g_malloc(sizeof(AtkAttribute));
    nsAutoString value;
    // We set atkAttr->name directly for each case. For the value, we set the
    // value string for each case. atkAttr->value is set at the end based on the
    // value string.

    // Set atkAttr->name to a specific ATK attribute name.
    auto atkName = [&atkAttr](AtkTextAttribute aAttrNum) {
      atkAttr->name = g_strdup(sAtkTextAttrNames[aAttrNum]);
    };
    // Set value to a formatted ATK color value.
    auto colorValue = [&iter, &value] {
      // The format of the atk attribute is r,g,b and the gecko one is
      // rgb(r, g, b).
      auto color = iter.Value<Color>();
      MOZ_ASSERT(color);
      value.AppendInt(NS_GET_R(color->mValue));
      value.Append(',');
      value.AppendInt(NS_GET_G(color->mValue));
      value.Append(',');
      value.AppendInt(NS_GET_B(color->mValue));
    };

    nsAtom* name = iter.Name();
    if (name == nsGkAtoms::color) {
      atkName(ATK_TEXT_ATTR_FG_COLOR);
      colorValue();
    } else if (name == nsGkAtoms::background_color) {
      atkName(ATK_TEXT_ATTR_BG_COLOR);
      colorValue();
    } else if (name == nsGkAtoms::font_family) {
      atkName(ATK_TEXT_ATTR_FAMILY_NAME);
      iter.ValueAsString(value);
    } else if (name == nsGkAtoms::font_size) {
      atkName(ATK_TEXT_ATTR_SIZE);
      // ATK wants the number of points without pt at the end.
      auto fontSize = iter.Value<FontSize>();
      MOZ_ASSERT(fontSize);
      value.AppendInt(fontSize->mValue);
    } else if (name == nsGkAtoms::font_weight) {
      atkName(ATK_TEXT_ATTR_WEIGHT);
      iter.ValueAsString(value);
    } else if (name == nsGkAtoms::invalid) {
      atkName(ATK_TEXT_ATTR_INVALID);
      iter.ValueAsString(value);
    } else {
      nsAutoString nameStr;
      iter.NameAsString(nameStr);
      atkAttr->name = g_strdup(NS_ConvertUTF16toUTF8(nameStr).get());
      iter.ValueAsString(value);
    }

    atkAttr->value = g_strdup(NS_ConvertUTF16toUTF8(value).get());
    atkAttributeSet = g_slist_prepend(atkAttributeSet, atkAttr);
  }

  // libatk-adaptor will free it
  return atkAttributeSet;
}

extern "C" {

static gchar* getTextCB(AtkText* aText, gint aStartOffset, gint aEndOffset) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc || !acc->IsTextRole()) {
    return nullptr;
  }
  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text) {
    return nullptr;
  }
  return DOMtoATK::NewATKString(text, aStartOffset, aEndOffset);
}

static gint getCharacterCountCB(AtkText* aText);

// Note: this does not support magic offsets, which is fine for its callers
// which do not implement any.
static gchar* getCharTextAtOffset(AtkText* aText, gint aOffset,
                                  gint* aStartOffset, gint* aEndOffset) {
  gint end = aOffset + 1;
  gint count = getCharacterCountCB(aText);

  if (aOffset > count) {
    aOffset = count;
  }
  if (end > count) {
    end = count;
  }
  if (aOffset < 0) {
    aOffset = 0;
  }
  if (end < 0) {
    end = 0;
  }
  *aStartOffset = aOffset;
  *aEndOffset = end;

  return getTextCB(aText, aOffset, end);
}

static gchar* getTextAfterOffsetCB(AtkText* aText, gint aOffset,
                                   AtkTextBoundary aBoundaryType,
                                   gint* aStartOffset, gint* aEndOffset) {
  if (aBoundaryType == ATK_TEXT_BOUNDARY_CHAR) {
    return getCharTextAtOffset(aText, aOffset + 1, aStartOffset, aEndOffset);
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return nullptr;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return nullptr;
  }

  nsAutoString autoStr;
  int32_t startOffset = 0, endOffset = 0;
  text->TextAfterOffset(aOffset, aBoundaryType, &startOffset, &endOffset,
                        autoStr);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  // libspi will free it.
  return DOMtoATK::Convert(autoStr);
}

static gchar* getTextAtOffsetCB(AtkText* aText, gint aOffset,
                                AtkTextBoundary aBoundaryType,
                                gint* aStartOffset, gint* aEndOffset) {
  if (aBoundaryType == ATK_TEXT_BOUNDARY_CHAR) {
    return getCharTextAtOffset(aText, aOffset, aStartOffset, aEndOffset);
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return nullptr;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return nullptr;
  }

  nsAutoString autoStr;
  int32_t startOffset = 0, endOffset = 0;
  text->TextAtOffset(aOffset, aBoundaryType, &startOffset, &endOffset, autoStr);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  // libspi will free it.
  return DOMtoATK::Convert(autoStr);
}

static gunichar getCharacterAtOffsetCB(AtkText* aText, gint aOffset) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return 0;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (text) {
    return DOMtoATK::ATKCharacter(text, aOffset);
  }

  return 0;
}

static gchar* getTextBeforeOffsetCB(AtkText* aText, gint aOffset,
                                    AtkTextBoundary aBoundaryType,
                                    gint* aStartOffset, gint* aEndOffset) {
  if (aBoundaryType == ATK_TEXT_BOUNDARY_CHAR) {
    return getCharTextAtOffset(aText, aOffset - 1, aStartOffset, aEndOffset);
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return nullptr;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return nullptr;
  }

  nsAutoString autoStr;
  int32_t startOffset = 0, endOffset = 0;
  text->TextBeforeOffset(aOffset, aBoundaryType, &startOffset, &endOffset,
                         autoStr);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  // libspi will free it.
  return DOMtoATK::Convert(autoStr);
}

static gint getCaretOffsetCB(AtkText* aText) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return -1;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return -1;
  }

  return static_cast<gint>(text->CaretOffset());
}

static AtkAttributeSet* getRunAttributesCB(AtkText* aText, gint aOffset,
                                           gint* aStartOffset,
                                           gint* aEndOffset) {
  *aStartOffset = -1;
  *aEndOffset = -1;
  int32_t startOffset = 0, endOffset = 0;

  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return nullptr;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return nullptr;
  }

  RefPtr<AccAttributes> attributes =
      text->TextAttributes(false, aOffset, &startOffset, &endOffset);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  return ConvertToAtkTextAttributeSet(attributes);
}

static AtkAttributeSet* getDefaultAttributesCB(AtkText* aText) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return nullptr;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return nullptr;
  }

  RefPtr<AccAttributes> attributes = text->DefaultTextAttributes();
  return ConvertToAtkTextAttributeSet(attributes);
}

static void getCharacterExtentsCB(AtkText* aText, gint aOffset, gint* aX,
                                  gint* aY, gint* aWidth, gint* aHeight,
                                  AtkCoordType aCoords) {
  if (!aX || !aY || !aWidth || !aHeight) {
    return;
  }
  *aX = *aY = *aWidth = *aHeight = -1;

  uint32_t geckoCoordType;
  if (aCoords == ATK_XY_SCREEN) {
    geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
  } else {
    geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return;
  }

  LayoutDeviceIntRect rect = text->CharBounds(aOffset, geckoCoordType);

  *aX = rect.x;
  *aY = rect.y;
  *aWidth = rect.width;
  *aHeight = rect.height;
}

static void getRangeExtentsCB(AtkText* aText, gint aStartOffset,
                              gint aEndOffset, AtkCoordType aCoords,
                              AtkTextRectangle* aRect) {
  if (!aRect) {
    return;
  }
  aRect->x = aRect->y = aRect->width = aRect->height = -1;

  uint32_t geckoCoordType;
  if (aCoords == ATK_XY_SCREEN) {
    geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
  } else {
    geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return;
  }

  LayoutDeviceIntRect rect =
      text->TextBounds(aStartOffset, aEndOffset, geckoCoordType);

  aRect->x = rect.x;
  aRect->y = rect.y;
  aRect->width = rect.width;
  aRect->height = rect.height;
}

static gint getCharacterCountCB(AtkText* aText) {
  if (Accessible* acc = GetInternalObj(ATK_OBJECT(aText))) {
    if (HyperTextAccessibleBase* text = acc->AsHyperTextBase()) {
      return static_cast<gint>(text->CharacterCount());
    }
  }
  return 0;
}

static gint getOffsetAtPointCB(AtkText* aText, gint aX, gint aY,
                               AtkCoordType aCoords) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return -1;
  }
  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return -1;
  }
  return static_cast<gint>(text->OffsetAtPoint(
      aX, aY,
      (aCoords == ATK_XY_SCREEN
           ? nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE
           : nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE)));
}

static gint getTextSelectionCountCB(AtkText* aText) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return 0;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return 0;
  }

  return text->SelectionCount();
}

static gchar* getTextSelectionCB(AtkText* aText, gint aSelectionNum,
                                 gint* aStartOffset, gint* aEndOffset) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return nullptr;
  }

  int32_t startOffset = 0, endOffset = 0;
  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return nullptr;
  }

  text->SelectionBoundsAt(aSelectionNum, &startOffset, &endOffset);
  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  return getTextCB(aText, *aStartOffset, *aEndOffset);
}

// set methods
static gboolean addTextSelectionCB(AtkText* aText, gint aStartOffset,
                                   gint aEndOffset) {
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* text = accWrap->AsHyperText();
    if (!text || !text->IsTextRole()) {
      return FALSE;
    }

    return text->AddToSelection(aStartOffset, aEndOffset);
  }
  if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    return proxy->AddToSelection(aStartOffset, aEndOffset);
  }

  return FALSE;
}

static gboolean removeTextSelectionCB(AtkText* aText, gint aSelectionNum) {
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* text = accWrap->AsHyperText();
    if (!text || !text->IsTextRole()) {
      return FALSE;
    }

    return text->RemoveFromSelection(aSelectionNum);
  }
  if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    return proxy->RemoveFromSelection(aSelectionNum);
  }

  return FALSE;
}

static gboolean setTextSelectionCB(AtkText* aText, gint aSelectionNum,
                                   gint aStartOffset, gint aEndOffset) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc || !acc->IsTextRole()) {
    return FALSE;
  }
  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text) {
    return FALSE;
  }
  return text->SetSelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset);
}

static gboolean setCaretOffsetCB(AtkText* aText, gint aOffset) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return FALSE;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text || !acc->IsTextRole()) {
    return FALSE;
  }

  text->SetCaretOffset(aOffset);
  return TRUE;
}

static gboolean scrollSubstringToCB(AtkText* aText, gint aStartOffset,
                                    gint aEndOffset, AtkScrollType aType) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return FALSE;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text) {
    return FALSE;
  }

  text->ScrollSubstringTo(aStartOffset, aEndOffset, aType);

  return TRUE;
}

static gboolean scrollSubstringToPointCB(AtkText* aText, gint aStartOffset,
                                         gint aEndOffset, AtkCoordType aCoords,
                                         gint aX, gint aY) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aText));
  if (!acc) {
    return FALSE;
  }

  HyperTextAccessibleBase* text = acc->AsHyperTextBase();
  if (!text) {
    return FALSE;
  }

  text->ScrollSubstringToPoint(aStartOffset, aEndOffset, aCoords, aX, aY);
  return TRUE;
}
}

void textInterfaceInitCB(AtkTextIface* aIface) {
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface)) return;

  aIface->get_text = getTextCB;
  aIface->get_text_after_offset = getTextAfterOffsetCB;
  aIface->get_text_at_offset = getTextAtOffsetCB;
  aIface->get_character_at_offset = getCharacterAtOffsetCB;
  aIface->get_text_before_offset = getTextBeforeOffsetCB;
  aIface->get_caret_offset = getCaretOffsetCB;
  aIface->get_run_attributes = getRunAttributesCB;
  aIface->get_default_attributes = getDefaultAttributesCB;
  aIface->get_character_extents = getCharacterExtentsCB;
  aIface->get_range_extents = getRangeExtentsCB;
  aIface->get_character_count = getCharacterCountCB;
  aIface->get_offset_at_point = getOffsetAtPointCB;
  aIface->get_n_selections = getTextSelectionCountCB;
  aIface->get_selection = getTextSelectionCB;

  // set methods
  aIface->add_selection = addTextSelectionCB;
  aIface->remove_selection = removeTextSelectionCB;
  aIface->set_selection = setTextSelectionCB;
  aIface->set_caret_offset = setCaretOffsetCB;

  if (IsAtkVersionAtLeast(2, 32)) {
    aIface->scroll_substring_to = scrollSubstringToCB;
    aIface->scroll_substring_to_point = scrollSubstringToPointCB;
  }

  // Cache the string values of the atk text attribute names.
  for (uint32_t i = 0; i < std::size(sAtkTextAttrNames); i++) {
    sAtkTextAttrNames[i] =
        atk_text_attribute_get_name(static_cast<AtkTextAttribute>(i));
  }
}
