/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessible-inl.h"

#include "nsAccessibilityService.h"
#include "nsIAccessibleTypes.h"
#include "AccAttributes.h"
#include "HTMLListAccessible.h"
#include "LocalAccessible-inl.h"
#include "Relation.h"
#include "mozilla/a11y/Role.h"
#include "States.h"
#include "TextAttrs.h"
#include "TextLeafRange.h"
#include "TextRange.h"
#include "TreeWalker.h"

#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsFocusManager.h"
#include "nsIEditingSession.h"
#include "nsContainerFrame.h"
#include "nsFrameSelection.h"
#include "nsILineIterator.h"
#include "nsIMathMLFrame.h"
#include "nsLayoutUtils.h"
#include "nsRange.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditorBase.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScrollContainerFrame.h"
#include "mozilla/SelectionMovementUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "gfxSkipChars.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible
////////////////////////////////////////////////////////////////////////////////

HyperTextAccessible::HyperTextAccessible(nsIContent* aNode, DocAccessible* aDoc)
    : AccessibleWrap(aNode, aDoc) {
  mType = eHyperTextType;
  mGenericTypes |= eHyperText;
}

role HyperTextAccessible::NativeRole() const {
  a11y::role r = GetAccService()->MarkupRole(mContent);
  if (r != roles::NOTHING) return r;

  nsIFrame* frame = GetFrame();
  if (frame && frame->IsInlineFrame()) return roles::TEXT;

  return roles::TEXT_CONTAINER;
}

uint64_t HyperTextAccessible::NativeState() const {
  uint64_t states = AccessibleWrap::NativeState();

  if (IsEditable()) {
    states |= states::EDITABLE;

  } else if (mContent->IsHTMLElement(nsGkAtoms::article)) {
    // We want <article> to behave like a document in terms of readonly state.
    states |= states::READONLY;
  }

  nsIFrame* frame = GetFrame();
  if ((states & states::EDITABLE) || (frame && frame->IsSelectable(nullptr))) {
    // If the accessible is editable the layout selectable state only disables
    // mouse selection, but keyboard (shift+arrow) selection is still possible.
    states |= states::SELECTABLE_TEXT;
  }

  return states;
}

bool HyperTextAccessible::IsEditable() const {
  if (!mContent) {
    return false;
  }
  return mContent->AsElement()->State().HasState(dom::ElementState::READWRITE);
}

uint32_t HyperTextAccessible::DOMPointToOffset(nsINode* aNode,
                                               int32_t aNodeOffset,
                                               bool aIsEndOffset) const {
  if (!aNode) return 0;

  uint32_t offset = 0;
  nsINode* findNode = nullptr;

  if (aNodeOffset == -1) {
    findNode = aNode;

  } else if (aNode->IsText()) {
    // For text nodes, aNodeOffset comes in as a character offset
    // Text offset will be added at the end, if we find the offset in this
    // hypertext We want the "skipped" offset into the text (rendered text
    // without the extra whitespace)
    nsIFrame* frame = aNode->AsContent()->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame, 0);

    nsresult rv = ContentToRenderedOffset(frame, aNodeOffset, &offset);
    NS_ENSURE_SUCCESS(rv, 0);

    findNode = aNode;

  } else {
    // findNode could be null if aNodeOffset == # of child nodes, which means
    // one of two things:
    // 1) there are no children, and the passed-in node is not mContent -- use
    //    parentContent for the node to find
    // 2) there are no children and the passed-in node is mContent, which means
    //    we're an empty nsIAccessibleText
    // 3) there are children and we're at the end of the children

    findNode = aNode->GetChildAt_Deprecated(aNodeOffset);
    if (!findNode) {
      if (aNodeOffset == 0) {
        if (aNode == GetNode()) {
          // Case #1: this accessible has no children and thus has empty text,
          // we can only be at hypertext offset 0.
          return 0;
        }

        // Case #2: there are no children, we're at this node.
        findNode = aNode;
      } else if (aNodeOffset == static_cast<int32_t>(aNode->GetChildCount())) {
        // Case #3: we're after the last child, get next node to this one.
        for (nsINode* tmpNode = aNode;
             !findNode && tmpNode && tmpNode != mContent;
             tmpNode = tmpNode->GetParent()) {
          findNode = tmpNode->GetNextSibling();
        }
      }
    }
  }

  // Get accessible for this findNode, or if that node isn't accessible, use the
  // accessible for the next DOM node which has one (based on forward depth
  // first search)
  LocalAccessible* descendant = nullptr;
  if (findNode) {
    dom::HTMLBRElement* brElement = dom::HTMLBRElement::FromNode(findNode);
    if (brElement && brElement->IsPaddingForEmptyEditor()) {
      // This <br> is the hacky "padding <br> element" used when there is no
      // text in the editor.
      return 0;
    }

    descendant = mDoc->GetAccessible(findNode);
    if (!descendant && findNode->IsContent()) {
      LocalAccessible* container = mDoc->GetContainerAccessible(findNode);
      if (container) {
        TreeWalker walker(container, findNode->AsContent(),
                          TreeWalker::eWalkContextTree);
        descendant = walker.Next();
        if (!descendant) descendant = container;
      }
    }
  }

  if (descendant && descendant->IsTextLeaf()) {
    uint32_t length = nsAccUtils::TextLength(descendant);
    if (offset > length) {
      // This can happen if text in the accessibility tree is out of date with
      // DOM, since the accessibility engine updates text asynchronously. This
      // should only be the case for a very short time, so it shouldn't be a
      // real problem.
      NS_WARNING("Offset too large for text leaf");
      offset = length;
    }
  }

  return TransformOffset(descendant, offset, aIsEndOffset);
}

uint32_t HyperTextAccessible::TransformOffset(LocalAccessible* aDescendant,
                                              uint32_t aOffset,
                                              bool aIsEndOffset) const {
  // From the descendant, go up and get the immediate child of this hypertext.
  uint32_t offset = aOffset;
  LocalAccessible* descendant = aDescendant;
  while (descendant) {
    LocalAccessible* parent = descendant->LocalParent();
    if (parent == this) return GetChildOffset(descendant) + offset;

    // This offset no longer applies because the passed-in text object is not
    // a child of the hypertext. This happens when there are nested hypertexts,
    // e.g. <div>abc<h1>def</h1>ghi</div>. Thus we need to adjust the offset
    // to make it relative the hypertext.
    // If the end offset is not supposed to be inclusive and the original point
    // is not at 0 offset then the returned offset should be after an embedded
    // character the original point belongs to.
    if (aIsEndOffset) {
      // Similar to our special casing in FindOffset, we add handling for
      // bulleted lists here because PeekOffset returns the inner text node
      // for a list when it should return the list bullet.
      // We manually set the offset so the error doesn't propagate up.
      if (offset == 0 && parent && parent->IsHTMLListItem() &&
          descendant->LocalPrevSibling() &&
          descendant->LocalPrevSibling() ==
              parent->AsHTMLListItem()->Bullet()) {
        offset = 0;
      } else {
        offset = (offset > 0 || descendant->IndexInParent() > 0) ? 1 : 0;
      }
    } else {
      offset = 0;
    }

    descendant = parent;
  }

  // If the given a11y point cannot be mapped into offset relative this
  // hypertext offset then return length as fallback value.
  return CharacterCount();
}

DOMPoint HyperTextAccessible::OffsetToDOMPoint(int32_t aOffset) const {
  // 0 offset is valid even if no children. In this case the associated editor
  // is empty so return a DOM point for editor root element.
  if (aOffset == 0) {
    RefPtr<EditorBase> editorBase = GetEditor();
    if (editorBase) {
      if (editorBase->IsEmpty()) {
        return DOMPoint(editorBase->GetRoot(), 0);
      }
    }
  }

  int32_t childIdx = GetChildIndexAtOffset(aOffset);
  if (childIdx == -1) return DOMPoint();

  LocalAccessible* child = LocalChildAt(childIdx);
  int32_t innerOffset = aOffset - GetChildOffset(childIdx);

  // A text leaf case.
  if (child->IsTextLeaf()) {
    // The point is inside the text node. This is always true for any text leaf
    // except a last child one. See assertion below.
    if (aOffset < GetChildOffset(childIdx + 1)) {
      nsIContent* content = child->GetContent();
      int32_t idx = 0;
      if (NS_FAILED(RenderedToContentOffset(content->GetPrimaryFrame(),
                                            innerOffset, &idx))) {
        return DOMPoint();
      }

      return DOMPoint(content, idx);
    }

    // Set the DOM point right after the text node.
    MOZ_ASSERT(static_cast<uint32_t>(aOffset) == CharacterCount());
    innerOffset = 1;
  }

  // Case of embedded object. The point is either before or after the element.
  NS_ASSERTION(innerOffset == 0 || innerOffset == 1, "A wrong inner offset!");
  nsINode* node = child->GetNode();
  nsINode* parentNode = node->GetParentNode();
  return parentNode ? DOMPoint(parentNode,
                               parentNode->ComputeIndexOf_Deprecated(node) +
                                   innerOffset)
                    : DOMPoint();
}

already_AddRefed<AccAttributes> HyperTextAccessible::DefaultTextAttributes() {
  RefPtr<AccAttributes> attributes = new AccAttributes();

  TextAttrsMgr textAttrsMgr(this);
  textAttrsMgr.GetAttributes(attributes);
  return attributes.forget();
}

void HyperTextAccessible::SetMathMLXMLRoles(AccAttributes* aAttributes) {
  // Add MathML xmlroles based on the position inside the parent.
  LocalAccessible* parent = LocalParent();
  if (parent) {
    switch (parent->Role()) {
      case roles::MATHML_CELL:
      case roles::MATHML_ENCLOSED:
      case roles::MATHML_ERROR:
      case roles::MATHML_MATH:
      case roles::MATHML_ROW:
      case roles::MATHML_SQUARE_ROOT:
      case roles::MATHML_STYLE:
        if (Role() == roles::MATHML_OPERATOR) {
          // This is an operator inside an <mrow> (or an inferred <mrow>).
          // See http://www.w3.org/TR/MathML3/chapter3.html#presm.inferredmrow
          // XXX We should probably do something similar for MATHML_FENCED, but
          // operators do not appear in the accessible tree. See bug 1175747.
          nsIMathMLFrame* mathMLFrame = do_QueryFrame(GetFrame());
          if (mathMLFrame) {
            nsEmbellishData embellishData;
            mathMLFrame->GetEmbellishData(embellishData);
            if (NS_MATHML_EMBELLISH_IS_FENCE(embellishData.flags)) {
              if (!LocalPrevSibling()) {
                aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                          nsGkAtoms::open_fence);
              } else if (!LocalNextSibling()) {
                aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                          nsGkAtoms::close_fence);
              }
            }
            if (NS_MATHML_EMBELLISH_IS_SEPARATOR(embellishData.flags)) {
              aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                        nsGkAtoms::separator);
            }
          }
        }
        break;
      case roles::MATHML_FRACTION:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles, IndexInParent() == 0 ? nsGkAtoms::numerator
                                                      : nsGkAtoms::denominator);
        break;
      case roles::MATHML_ROOT:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::root_index);
        break;
      case roles::MATHML_SUB:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::subscript);
        break;
      case roles::MATHML_SUP:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::superscript);
        break;
      case roles::MATHML_SUB_SUP: {
        int32_t index = IndexInParent();
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            index == 0
                ? nsGkAtoms::base
                : (index == 1 ? nsGkAtoms::subscript : nsGkAtoms::superscript));
      } break;
      case roles::MATHML_UNDER:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::underscript);
        break;
      case roles::MATHML_OVER:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::overscript);
        break;
      case roles::MATHML_UNDER_OVER: {
        int32_t index = IndexInParent();
        aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                  index == 0
                                      ? nsGkAtoms::base
                                      : (index == 1 ? nsGkAtoms::underscript
                                                    : nsGkAtoms::overscript));
      } break;
      case roles::MATHML_MULTISCRIPTS: {
        // Get the <multiscripts> base.
        nsIContent* child;
        bool baseFound = false;
        for (child = parent->GetContent()->GetFirstChild(); child;
             child = child->GetNextSibling()) {
          if (child->IsMathMLElement()) {
            baseFound = true;
            break;
          }
        }
        if (baseFound) {
          nsIContent* content = GetContent();
          if (child == content) {
            // We are the base.
            aAttributes->SetAttribute(nsGkAtoms::xmlroles, nsGkAtoms::base);
          } else {
            // Browse the list of scripts to find us and determine our type.
            bool postscript = true;
            bool subscript = true;
            for (child = child->GetNextSibling(); child;
                 child = child->GetNextSibling()) {
              if (!child->IsMathMLElement()) continue;
              if (child->IsMathMLElement(nsGkAtoms::mprescripts)) {
                postscript = false;
                subscript = true;
                continue;
              }
              if (child == content) {
                if (postscript) {
                  aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                            subscript ? nsGkAtoms::subscript
                                                      : nsGkAtoms::superscript);
                } else {
                  aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                            subscript
                                                ? nsGkAtoms::presubscript
                                                : nsGkAtoms::presuperscript);
                }
                break;
              }
              subscript = !subscript;
            }
          }
        }
      } break;
      default:
        break;
    }
  }
}

already_AddRefed<AccAttributes> HyperTextAccessible::NativeAttributes() {
  RefPtr<AccAttributes> attributes = AccessibleWrap::NativeAttributes();

  // 'formatting' attribute is deprecated, 'display' attribute should be
  // instead.
  nsIFrame* frame = GetFrame();
  if (frame && frame->IsBlockFrame()) {
    attributes->SetAttribute(nsGkAtoms::formatting, nsGkAtoms::block);
  }

  if (FocusMgr()->IsFocused(this)) {
    int32_t lineNumber = CaretLineNumber();
    if (lineNumber >= 1) {
      attributes->SetAttribute(nsGkAtoms::lineNumber, lineNumber);
    }
  }

  if (HasOwnContent()) {
    GetAccService()->MarkupAttributes(this, attributes);
    if (mContent->IsMathMLElement()) SetMathMLXMLRoles(attributes);
  }

  return attributes.forget();
}

int32_t HyperTextAccessible::OffsetAtPoint(int32_t aX, int32_t aY,
                                           uint32_t aCoordType) {
  nsIFrame* hyperFrame = GetFrame();
  if (!hyperFrame) return -1;

  LayoutDeviceIntPoint coords =
      nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordType, this);

  nsPresContext* presContext = mDoc->PresContext();
  nsPoint coordsInAppUnits = LayoutDeviceIntPoint::ToAppUnits(
      coords, presContext->AppUnitsPerDevPixel());

  nsRect frameScreenRect = hyperFrame->GetScreenRectInAppUnits();
  if (!frameScreenRect.Contains(coordsInAppUnits.x, coordsInAppUnits.y)) {
    return -1;  // Not found
  }

  nsPoint pointInHyperText(coordsInAppUnits.x - frameScreenRect.X(),
                           coordsInAppUnits.y - frameScreenRect.Y());

  // Go through the frames to check if each one has the point.
  // When one does, add up the character offsets until we have a match

  // We have an point in an accessible child of this, now we need to add up the
  // offsets before it to what we already have
  int32_t offset = 0;
  uint32_t childCount = ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    LocalAccessible* childAcc = mChildren[childIdx];

    nsIFrame* primaryFrame = childAcc->GetFrame();
    NS_ENSURE_TRUE(primaryFrame, -1);

    nsIFrame* frame = primaryFrame;
    while (frame) {
      nsIContent* content = frame->GetContent();
      NS_ENSURE_TRUE(content, -1);
      nsPoint pointInFrame = pointInHyperText - frame->GetOffsetTo(hyperFrame);
      nsSize frameSize = frame->GetSize();
      if (pointInFrame.x < frameSize.width &&
          pointInFrame.y < frameSize.height) {
        // Finished
        if (frame->IsTextFrame()) {
          nsIFrame::ContentOffsets contentOffsets =
              frame->GetContentOffsetsFromPointExternal(
                  pointInFrame, nsIFrame::IGNORE_SELECTION_STYLE);
          if (contentOffsets.IsNull() || contentOffsets.content != content) {
            return -1;  // Not found
          }
          uint32_t addToOffset;
          nsresult rv = ContentToRenderedOffset(
              primaryFrame, contentOffsets.offset, &addToOffset);
          NS_ENSURE_SUCCESS(rv, -1);
          offset += addToOffset;
        }
        return offset;
      }
      frame = frame->GetNextContinuation();
    }

    offset += nsAccUtils::TextLength(childAcc);
  }

  return -1;  // Not found
}

already_AddRefed<EditorBase> HyperTextAccessible::GetEditor() const {
  if (!mContent->HasFlag(NODE_IS_EDITABLE)) {
    // If we're inside an editable container, then return that container's
    // editor
    LocalAccessible* ancestor = LocalParent();
    while (ancestor) {
      HyperTextAccessible* hyperText = ancestor->AsHyperText();
      if (hyperText) {
        // Recursion will stop at container doc because it has its own impl
        // of GetEditor()
        return hyperText->GetEditor();
      }

      ancestor = ancestor->LocalParent();
    }

    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(mContent);
  nsCOMPtr<nsIEditingSession> editingSession;
  docShell->GetEditingSession(getter_AddRefs(editingSession));
  if (!editingSession) return nullptr;  // No editing session interface

  dom::Document* docNode = mDoc->DocumentNode();
  RefPtr<HTMLEditor> htmlEditor =
      editingSession->GetHTMLEditorForWindow(docNode->GetWindow());
  return htmlEditor.forget();
}

/**
 * =================== Caret & Selection ======================
 */

int32_t HyperTextAccessible::CaretOffset() const {
  // Not focused focusable accessible except document accessible doesn't have
  // a caret.
  if (!IsDoc() && !FocusMgr()->IsFocused(this) &&
      (InteractiveState() & states::FOCUSABLE)) {
    return -1;
  }

  // Check cached value.
  int32_t caretOffset = -1;
  HyperTextAccessible* text = SelectionMgr()->AccessibleWithCaret(&caretOffset);

  // Use cached value if it corresponds to this accessible.
  if (caretOffset != -1) {
    if (text == this) return caretOffset;

    nsINode* textNode = text->GetNode();
    // Ignore offset if cached accessible isn't a text leaf.
    if (nsCoreUtils::IsAncestorOf(GetNode(), textNode)) {
      return TransformOffset(text, textNode->IsText() ? caretOffset : 0, false);
    }
  }

  // No caret if the focused node is not inside this DOM node and this DOM node
  // is not inside of focused node.
  FocusManager::FocusDisposition focusDisp =
      FocusMgr()->IsInOrContainsFocus(this);
  if (focusDisp == FocusManager::eNone) return -1;

  // Turn the focus node and offset of the selection into caret hypretext
  // offset.
  dom::Selection* domSel = DOMSelection();
  NS_ENSURE_TRUE(domSel, -1);

  nsINode* focusNode = domSel->GetFocusNode();
  uint32_t focusOffset = domSel->FocusOffset();

  // No caret if this DOM node is inside of focused node but the selection's
  // focus point is not inside of this DOM node.
  if (focusDisp == FocusManager::eContainedByFocus) {
    nsINode* resultNode =
        nsCoreUtils::GetDOMNodeFromDOMPoint(focusNode, focusOffset);

    nsINode* thisNode = GetNode();
    if (resultNode != thisNode &&
        !nsCoreUtils::IsAncestorOf(thisNode, resultNode)) {
      return -1;
    }
  }

  return DOMPointToOffset(focusNode, focusOffset);
}

LayoutDeviceIntRect HyperTextAccessible::GetCaretRect(nsIWidget** aWidget) {
  *aWidget = nullptr;

  RefPtr<nsCaret> caret = mDoc->PresShellPtr()->GetCaret();
  NS_ENSURE_TRUE(caret, LayoutDeviceIntRect());

  bool isVisible = caret->IsVisible();
  if (!isVisible) return LayoutDeviceIntRect();

  nsRect rect;
  nsIFrame* frame = caret->GetGeometry(&rect);
  if (!frame || rect.IsEmpty()) return LayoutDeviceIntRect();

  PresShell* presShell = mDoc->PresShellPtr();
  // Transform rect to be relative to the root frame.
  nsIFrame* rootFrame = presShell->GetRootFrame();
  rect = nsLayoutUtils::TransformFrameRectToAncestor(frame, rect, rootFrame);
  // We need to inverse translate with the offset of the edge of the visual
  // viewport from top edge of the layout viewport.
  nsPoint viewportOffset = presShell->GetVisualViewportOffset() -
                           presShell->GetLayoutViewportOffset();
  rect.MoveBy(-viewportOffset);
  // We need to take into account a non-1 resolution set on the presshell.
  // This happens with async pinch zooming. Here we scale the bounds before
  // adding the screen-relative offset.
  rect.ScaleRoundOut(presShell->GetResolution());
  // Now we need to put the rect in absolute screen coords.
  nsRect rootScreenRect = rootFrame->GetScreenRectInAppUnits();
  rect.MoveBy(rootScreenRect.TopLeft());
  // Finally, convert from app units.
  auto caretRect = LayoutDeviceIntRect::FromAppUnitsToNearest(
      rect, presShell->GetPresContext()->AppUnitsPerDevPixel());

  // Correct for character size, so that caret always matches the size of
  // the character. This is important for font size transitions, and is
  // necessary because the Gecko caret uses the previous character's size as
  // the user moves forward in the text by character.
  int32_t caretOffset = CaretOffset();
  if (NS_WARN_IF(caretOffset == -1)) {
    // The caret offset will be -1 if this Accessible isn't focused. Note that
    // the DOM node contaning the caret might be focused, but the Accessible
    // might not be; e.g. due to an autocomplete popup suggestion having a11y
    // focus.
    return LayoutDeviceIntRect();
  }
  LayoutDeviceIntRect charRect = CharBounds(
      caretOffset, nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE);
  if (!charRect.IsEmpty()) {
    caretRect.SetTopEdge(charRect.Y());
  }

  *aWidget = frame->GetNearestWidget();
  return caretRect;
}

void HyperTextAccessible::GetSelectionDOMRanges(SelectionType aSelectionType,
                                                nsTArray<nsRange*>* aRanges) {
  if (IsDoc() && !AsDoc()->HasLoadState(DocAccessible::eTreeConstructed)) {
    // Rarely, a client query can be handled after a DocAccessible is created
    // but before the initial tree is constructed, since DoInitialUpdate happens
    // during a refresh tick. In that case, there might be a DOM selection, but
    // we can't use it. We will crash if we try due to mContent being null, etc.
    // This should only happen in the parent process because we should never
    // try to push the cache in a content process before the initial tree is
    // constructed.
    MOZ_ASSERT(XRE_IsParentProcess(), "Query before DoInitialUpdate");
    return;
  }
  // Ignore selection if it is not visible.
  RefPtr<nsFrameSelection> frameSelection = FrameSelection();
  if (!frameSelection || frameSelection->GetDisplaySelection() <=
                             nsISelectionController::SELECTION_HIDDEN) {
    return;
  }

  dom::Selection* domSel = frameSelection->GetSelection(aSelectionType);
  if (!domSel) return;

  nsINode* startNode = GetNode();

  RefPtr<EditorBase> editorBase = GetEditor();
  if (editorBase) {
    startNode = editorBase->GetRoot();
  }

  if (!startNode) return;

  uint32_t childCount = startNode->GetChildCount();
  nsresult rv = domSel->GetDynamicRangesForIntervalArray(
      startNode, 0, startNode, childCount, true, aRanges);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Remove collapsed ranges
  aRanges->RemoveElementsBy(
      [](const auto& range) { return range->Collapsed(); });
}

int32_t HyperTextAccessible::SelectionCount() {
  nsTArray<nsRange*> ranges;
  GetSelectionDOMRanges(SelectionType::eNormal, &ranges);
  return ranges.Length();
}

bool HyperTextAccessible::SelectionBoundsAt(int32_t aSelectionNum,
                                            int32_t* aStartOffset,
                                            int32_t* aEndOffset) {
  *aStartOffset = *aEndOffset = 0;

  nsTArray<nsRange*> ranges;
  GetSelectionDOMRanges(SelectionType::eNormal, &ranges);

  uint32_t rangeCount = ranges.Length();
  if (aSelectionNum < 0 || aSelectionNum >= static_cast<int32_t>(rangeCount)) {
    return false;
  }

  nsRange* range = ranges[aSelectionNum];

  // Make sure start is before end, by swapping DOM points.  This occurs when
  // the user selects backwards in the text.
  const Maybe<int32_t> order =
      nsContentUtils::ComparePoints(range->EndRef(), range->StartRef());

  if (!order) {
    MOZ_ASSERT_UNREACHABLE();
    return false;
  }

  const RangeBoundary& precedingBoundary =
      *order < 0 ? range->EndRef() : range->StartRef();
  const RangeBoundary& followingBoundary =
      *order < 0 ? range->StartRef() : range->EndRef();

  if (!precedingBoundary.GetContainer()->IsInclusiveDescendantOf(mContent)) {
    *aStartOffset = 0;
  } else {
    *aStartOffset = DOMPointToOffset(
        precedingBoundary.GetContainer(),
        AssertedCast<int32_t>(*precedingBoundary.Offset(
            RangeBoundary::OffsetFilter::kValidOrInvalidOffsets)));
  }

  if (!followingBoundary.GetContainer()->IsInclusiveDescendantOf(mContent)) {
    *aEndOffset = CharacterCount();
  } else {
    *aEndOffset = DOMPointToOffset(
        followingBoundary.GetContainer(),
        AssertedCast<int32_t>(*followingBoundary.Offset(
            RangeBoundary::OffsetFilter::kValidOrInvalidOffsets)),
        true);
  }
  return true;
}

bool HyperTextAccessible::RemoveFromSelection(int32_t aSelectionNum) {
  RefPtr<dom::Selection> domSel = DOMSelection();
  if (!domSel) return false;

  if (aSelectionNum < 0 ||
      aSelectionNum >= static_cast<int32_t>(domSel->RangeCount())) {
    return false;
  }

  const RefPtr<nsRange> range{
      domSel->GetRangeAt(static_cast<uint32_t>(aSelectionNum))};
  domSel->RemoveRangeAndUnselectFramesAndNotifyListeners(*range,
                                                         IgnoreErrors());
  return true;
}

void HyperTextAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                                 int32_t aEndOffset,
                                                 uint32_t aCoordinateType,
                                                 int32_t aX, int32_t aY) {
  nsIFrame* frame = GetFrame();
  if (!frame) return;

  LayoutDeviceIntPoint coords =
      nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordinateType, this);

  RefPtr<nsRange> domRange = nsRange::Create(mContent);
  TextRange range(this, this, aStartOffset, this, aEndOffset);
  if (!range.AssignDOMRange(domRange)) {
    return;
  }

  nsPresContext* presContext = frame->PresContext();
  nsPoint coordsInAppUnits = LayoutDeviceIntPoint::ToAppUnits(
      coords, presContext->AppUnitsPerDevPixel());

  bool initialScrolled = false;
  nsIFrame* parentFrame = frame;
  while ((parentFrame = parentFrame->GetParent())) {
    if (parentFrame->IsScrollContainerOrSubclass()) {
      if (!initialScrolled) {
        // Scroll substring to the given point. Turn the point into percents
        // relative scrollable area to use nsCoreUtils::ScrollSubstringTo.
        nsRect frameRect = parentFrame->GetScreenRectInAppUnits();
        nscoord offsetPointX = coordsInAppUnits.x - frameRect.X();
        nscoord offsetPointY = coordsInAppUnits.y - frameRect.Y();

        nsSize size(parentFrame->GetSize());

        // avoid divide by zero
        size.width = size.width ? size.width : 1;
        size.height = size.height ? size.height : 1;

        int16_t hPercent = offsetPointX * 100 / size.width;
        int16_t vPercent = offsetPointY * 100 / size.height;

        nsresult rv = nsCoreUtils::ScrollSubstringTo(
            frame, domRange,
            ScrollAxis(WhereToScroll(vPercent), WhenToScroll::Always),
            ScrollAxis(WhereToScroll(hPercent), WhenToScroll::Always));
        if (NS_FAILED(rv)) return;

        initialScrolled = true;
      } else {
        // Substring was scrolled to the given point already inside its closest
        // scrollable area. If there are nested scrollable areas then make
        // sure we scroll lower areas to the given point inside currently
        // traversed scrollable area.
        nsCoreUtils::ScrollFrameToPoint(parentFrame, frame, coords);
      }
    }
    frame = parentFrame;
  }
}

void HyperTextAccessible::SelectionRanges(
    nsTArray<a11y::TextRange>* aRanges) const {
  dom::Selection* sel = DOMSelection();
  if (!sel) {
    return;
  }

  TextRange::TextRangesFromSelection(sel, aRanges);
}

void HyperTextAccessible::ReplaceText(const nsAString& aText) {
  if (aText.Length() == 0) {
    DeleteText(0, CharacterCount());
    return;
  }

  SetSelectionBoundsAt(TextLeafRange::kRemoveAllExistingSelectedRanges, 0,
                       CharacterCount());

  RefPtr<EditorBase> editorBase = GetEditor();
  if (!editorBase) {
    return;
  }

  DebugOnly<nsresult> rv = editorBase->InsertTextAsAction(aText);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert the new text");
}

void HyperTextAccessible::InsertText(const nsAString& aText,
                                     int32_t aPosition) {
  RefPtr<EditorBase> editorBase = GetEditor();
  if (editorBase) {
    SetSelectionBoundsAt(TextLeafRange::kRemoveAllExistingSelectedRanges,
                         aPosition, aPosition);
    DebugOnly<nsresult> rv = editorBase->InsertTextAsAction(aText);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert the text");
  }
}

void HyperTextAccessible::CopyText(int32_t aStartPos, int32_t aEndPos) {
  RefPtr<EditorBase> editorBase = GetEditor();
  if (editorBase) {
    SetSelectionBoundsAt(TextLeafRange::kRemoveAllExistingSelectedRanges,
                         aStartPos, aEndPos);
    editorBase->Copy();
  }
}

void HyperTextAccessible::CutText(int32_t aStartPos, int32_t aEndPos) {
  RefPtr<EditorBase> editorBase = GetEditor();
  if (editorBase) {
    SetSelectionBoundsAt(TextLeafRange::kRemoveAllExistingSelectedRanges,
                         aStartPos, aEndPos);
    editorBase->Cut();
  }
}

void HyperTextAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos) {
  RefPtr<EditorBase> editorBase = GetEditor();
  if (!editorBase) {
    return;
  }
  SetSelectionBoundsAt(TextLeafRange::kRemoveAllExistingSelectedRanges,
                       aStartPos, aEndPos);
  DebugOnly<nsresult> rv =
      editorBase->DeleteSelectionAsAction(nsIEditor::eNone, nsIEditor::eStrip);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to delete text");
}

void HyperTextAccessible::PasteText(int32_t aPosition) {
  RefPtr<EditorBase> editorBase = GetEditor();
  if (editorBase) {
    // If the caller wants to paste at the caret, we don't need to set the
    // selection. If there is text already selected, this also allows the caller
    // to replace it, just as would happen when pasting using the keyboard or
    // GUI.
    if (aPosition != nsIAccessibleText::TEXT_OFFSET_CARET) {
      SetSelectionBoundsAt(TextLeafRange::kRemoveAllExistingSelectedRanges,
                           aPosition, aPosition);
    }
    editorBase->PasteAsAction(nsIClipboard::kGlobalClipboard,
                              EditorBase::DispatchPasteEvent::Yes);
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible public

// LocalAccessible protected
ENameValueFlag HyperTextAccessible::NativeName(nsString& aName) const {
  // Check @alt attribute for invalid img elements.
  if (mContent->IsHTMLElement(nsGkAtoms::img)) {
    mContent->AsElement()->GetAttr(nsGkAtoms::alt, aName);
    if (!aName.IsEmpty()) return eNameOK;
  }

  ENameValueFlag nameFlag = AccessibleWrap::NativeName(aName);
  if (!aName.IsEmpty()) return nameFlag;

  // Get name from title attribute for HTML abbr and acronym elements making it
  // a valid name from markup. Otherwise their name isn't picked up by recursive
  // name computation algorithm. See NS_OK_NAME_FROM_TOOLTIP.
  if (IsAbbreviation() && mContent->AsElement()->GetAttr(
                              kNameSpaceID_None, nsGkAtoms::title, aName)) {
    aName.CompressWhitespace();
  }

  return eNameOK;
}

void HyperTextAccessible::Shutdown() {
  mOffsets.Clear();
  AccessibleWrap::Shutdown();
}

bool HyperTextAccessible::RemoveChild(LocalAccessible* aAccessible) {
  const int32_t childIndex = aAccessible->IndexInParent();
  if (childIndex < static_cast<int32_t>(mOffsets.Length())) {
    mOffsets.RemoveLastElements(mOffsets.Length() - childIndex);
  }

  return AccessibleWrap::RemoveChild(aAccessible);
}

bool HyperTextAccessible::InsertChildAt(uint32_t aIndex,
                                        LocalAccessible* aChild) {
  if (aIndex < mOffsets.Length()) {
    mOffsets.RemoveLastElements(mOffsets.Length() - aIndex);
  }

  return AccessibleWrap::InsertChildAt(aIndex, aChild);
}

Relation HyperTextAccessible::RelationByType(RelationType aType) const {
  Relation rel = LocalAccessible::RelationByType(aType);

  switch (aType) {
    case RelationType::NODE_CHILD_OF:
      if (HasOwnContent() && mContent->IsMathMLElement()) {
        LocalAccessible* parent = LocalParent();
        if (parent) {
          nsIContent* parentContent = parent->GetContent();
          if (parentContent &&
              parentContent->IsMathMLElement(nsGkAtoms::mroot)) {
            // Add a relation pointing to the parent <mroot>.
            rel.AppendTarget(parent);
          }
        }
      }
      break;
    case RelationType::NODE_PARENT_OF:
      if (HasOwnContent() && mContent->IsMathMLElement(nsGkAtoms::mroot)) {
        LocalAccessible* base = LocalChildAt(0);
        LocalAccessible* index = LocalChildAt(1);
        if (base && index) {
          // Append the <mroot> children in the order index, base.
          rel.AppendTarget(index);
          rel.AppendTarget(base);
        }
      }
      break;
    default:
      break;
  }

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible public static

nsresult HyperTextAccessible::ContentToRenderedOffset(
    nsIFrame* aFrame, int32_t aContentOffset, uint32_t* aRenderedOffset) const {
  if (!aFrame) {
    // Current frame not rendered -- this can happen if text is set on
    // something with display: none
    *aRenderedOffset = 0;
    return NS_OK;
  }

  if (IsTextField()) {
    *aRenderedOffset = aContentOffset;
    return NS_OK;
  }

  NS_ASSERTION(aFrame->IsTextFrame(), "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nullptr,
               "Call on primary frame only");

  nsIFrame::RenderedText text =
      aFrame->GetRenderedText(aContentOffset, aContentOffset + 1,
                              nsIFrame::TextOffsetType::OffsetsInContentText,
                              nsIFrame::TrailingWhitespace::DontTrim);
  *aRenderedOffset = text.mOffsetWithinNodeRenderedText;

  return NS_OK;
}

nsresult HyperTextAccessible::RenderedToContentOffset(
    nsIFrame* aFrame, uint32_t aRenderedOffset, int32_t* aContentOffset) const {
  if (IsTextField()) {
    *aContentOffset = aRenderedOffset;
    return NS_OK;
  }

  *aContentOffset = 0;
  NS_ENSURE_TRUE(aFrame, NS_ERROR_FAILURE);

  NS_ASSERTION(aFrame->IsTextFrame(), "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nullptr,
               "Call on primary frame only");

  nsIFrame::RenderedText text =
      aFrame->GetRenderedText(aRenderedOffset, aRenderedOffset + 1,
                              nsIFrame::TextOffsetType::OffsetsInRenderedText,
                              nsIFrame::TrailingWhitespace::DontTrim);
  *aContentOffset = text.mOffsetWithinNodeText;

  return NS_OK;
}
