/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCoreUtils.h"

#include "nsIAccessibleTypes.h"

#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsRange.h"
#include "nsIBoxObject.h"
#include "nsIDOMXULElement.h"
#include "nsIDocShell.h"
#include "nsEventListenerManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIScrollableFrame.h"
#include "nsEventStateManager.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionController.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"
#include "nsView.h"
#include "nsGkAtoms.h"
#include "nsDOMTouchEvent.h"

#include "nsComponentManagerUtils.h"

#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;

////////////////////////////////////////////////////////////////////////////////
// nsCoreUtils
////////////////////////////////////////////////////////////////////////////////

bool
nsCoreUtils::HasClickListener(nsIContent *aContent)
{
  NS_ENSURE_TRUE(aContent, false);
  nsEventListenerManager* listenerManager =
    aContent->GetExistingListenerManager();

  return listenerManager &&
    (listenerManager->HasListenersFor(nsGkAtoms::onclick) ||
     listenerManager->HasListenersFor(nsGkAtoms::onmousedown) ||
     listenerManager->HasListenersFor(nsGkAtoms::onmouseup));
}

void
nsCoreUtils::DispatchClickEvent(nsITreeBoxObject *aTreeBoxObj,
                                int32_t aRowIndex, nsITreeColumn *aColumn,
                                const nsCString& aPseudoElt)
{
  nsCOMPtr<nsIDOMElement> tcElm;
  aTreeBoxObj->GetTreeBody(getter_AddRefs(tcElm));
  if (!tcElm)
    return;

  nsCOMPtr<nsIContent> tcContent(do_QueryInterface(tcElm));
  nsIDocument *document = tcContent->GetCurrentDoc();
  if (!document)
    return;

  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (!presShell)
    return;

  // Ensure row is visible.
  aTreeBoxObj->EnsureRowIsVisible(aRowIndex);

  // Calculate x and y coordinates.
  int32_t x = 0, y = 0, width = 0, height = 0;
  nsresult rv = aTreeBoxObj->GetCoordsForCellItem(aRowIndex, aColumn,
                                                  aPseudoElt,
                                                  &x, &y, &width, &height);
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIDOMXULElement> tcXULElm(do_QueryInterface(tcElm));
  nsCOMPtr<nsIBoxObject> tcBoxObj;
  tcXULElm->GetBoxObject(getter_AddRefs(tcBoxObj));

  int32_t tcX = 0;
  tcBoxObj->GetX(&tcX);

  int32_t tcY = 0;
  tcBoxObj->GetY(&tcY);

  // Dispatch mouse events.
  nsWeakFrame tcFrame = tcContent->GetPrimaryFrame();
  nsIFrame* rootFrame = presShell->GetRootFrame();

  nsPoint offset;
  nsIWidget *rootWidget =
    rootFrame->GetViewExternal()->GetNearestWidget(&offset);

  nsRefPtr<nsPresContext> presContext = presShell->GetPresContext();

  int32_t cnvdX = presContext->CSSPixelsToDevPixels(tcX + x + 1) +
    presContext->AppUnitsToDevPixels(offset.x);
  int32_t cnvdY = presContext->CSSPixelsToDevPixels(tcY + y + 1) +
    presContext->AppUnitsToDevPixels(offset.y);

  // XUL is just desktop, so there is no real reason for senfing touch events.
  DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, cnvdX, cnvdY,
                     tcContent, tcFrame, presShell, rootWidget);

  DispatchMouseEvent(NS_MOUSE_BUTTON_UP, cnvdX, cnvdY,
                     tcContent, tcFrame, presShell, rootWidget);
}

void
nsCoreUtils::DispatchMouseEvent(uint32_t aEventType, int32_t aX, int32_t aY,
                                nsIContent *aContent, nsIFrame *aFrame,
                                nsIPresShell *aPresShell, nsIWidget *aRootWidget)
{
  WidgetMouseEvent event(true, aEventType, aRootWidget,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

  event.refPoint = LayoutDeviceIntPoint(aX, aY);

  event.clickCount = 1;
  event.button = WidgetMouseEvent::eLeftButton;
  event.time = PR_IntervalNow();
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;

  nsEventStatus status = nsEventStatus_eIgnore;
  aPresShell->HandleEventWithTarget(&event, aFrame, aContent, &status);
}

void
nsCoreUtils::DispatchTouchEvent(uint32_t aEventType, int32_t aX, int32_t aY,
                                nsIContent* aContent, nsIFrame* aFrame,
                                nsIPresShell* aPresShell, nsIWidget* aRootWidget)
{
  if (!nsDOMTouchEvent::PrefEnabled())
    return;

  WidgetTouchEvent event(true, aEventType, aRootWidget);

  event.time = PR_IntervalNow();

  // XXX: Touch has an identifier of -1 to hint that it is synthesized.
  nsRefPtr<mozilla::dom::Touch> t =
    new mozilla::dom::Touch(-1, nsIntPoint(aX, aY),
                            nsIntPoint(1, 1), 0.0f, 1.0f);
  t->SetTarget(aContent);
  event.touches.AppendElement(t);
  nsEventStatus status = nsEventStatus_eIgnore;
  aPresShell->HandleEventWithTarget(&event, aFrame, aContent, &status);
}

uint32_t
nsCoreUtils::GetAccessKeyFor(nsIContent* aContent)
{
  // Accesskeys are registered by @accesskey attribute only. At first check
  // whether it is presented on the given element to avoid the slow
  // nsEventStateManager::GetRegisteredAccessKey() method.
  if (!aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::accesskey))
    return 0;

  nsIPresShell* presShell = aContent->OwnerDoc()->GetShell();
  if (!presShell)
    return 0;

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext)
    return 0;

  nsEventStateManager *esm = presContext->EventStateManager();
  if (!esm)
    return 0;

  return esm->GetRegisteredAccessKey(aContent);
}

nsIContent *
nsCoreUtils::GetDOMElementFor(nsIContent *aContent)
{
  if (aContent->IsElement())
    return aContent;

  if (aContent->IsNodeOfType(nsINode::eTEXT))
    return aContent->GetFlattenedTreeParent();

  return nullptr;
}

nsINode *
nsCoreUtils::GetDOMNodeFromDOMPoint(nsINode *aNode, uint32_t aOffset)
{
  if (aNode && aNode->IsElement()) {
    uint32_t childCount = aNode->GetChildCount();
    NS_ASSERTION(aOffset <= childCount, "Wrong offset of the DOM point!");

    // The offset can be after last child of container node that means DOM point
    // is placed immediately after the last child. In this case use the DOM node
    // from the given DOM point is used as result node.
    if (aOffset != childCount)
      return aNode->GetChildAt(aOffset);
  }

  return aNode;
}

nsIContent*
nsCoreUtils::GetRoleContent(nsINode *aNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aNode));
    if (doc) {
      nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(aNode));
      if (htmlDoc) {
        nsCOMPtr<nsIDOMHTMLElement> bodyElement;
        htmlDoc->GetBody(getter_AddRefs(bodyElement));
        content = do_QueryInterface(bodyElement);
      }
      else {
        return doc->GetDocumentElement();
      }
    }
  }

  return content;
}

bool
nsCoreUtils::IsAncestorOf(nsINode *aPossibleAncestorNode,
                          nsINode *aPossibleDescendantNode,
                          nsINode *aRootNode)
{
  NS_ENSURE_TRUE(aPossibleAncestorNode && aPossibleDescendantNode, false);

  nsINode *parentNode = aPossibleDescendantNode;
  while ((parentNode = parentNode->GetParentNode()) &&
         parentNode != aRootNode) {
    if (parentNode == aPossibleAncestorNode)
      return true;
  }

  return false;
}

nsresult
nsCoreUtils::ScrollSubstringTo(nsIFrame* aFrame, nsRange* aRange,
                               uint32_t aScrollType)
{
  nsIPresShell::ScrollAxis vertical, horizontal;
  ConvertScrollTypeToPercents(aScrollType, &vertical, &horizontal);

  return ScrollSubstringTo(aFrame, aRange, vertical, horizontal);
}

nsresult
nsCoreUtils::ScrollSubstringTo(nsIFrame* aFrame, nsRange* aRange,
                               nsIPresShell::ScrollAxis aVertical,
                               nsIPresShell::ScrollAxis aHorizontal)
{
  if (!aFrame)
    return NS_ERROR_FAILURE;

  nsPresContext *presContext = aFrame->PresContext();

  nsCOMPtr<nsISelectionController> selCon;
  aFrame->GetSelectionController(presContext, getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelection> selection;
  selCon->GetSelection(nsISelectionController::SELECTION_ACCESSIBILITY,
                       getter_AddRefs(selection));

  nsCOMPtr<nsISelectionPrivate> privSel(do_QueryInterface(selection));
  selection->RemoveAllRanges();
  selection->AddRange(aRange);

  privSel->ScrollIntoViewInternal(
    nsISelectionController::SELECTION_ANCHOR_REGION,
    true, aVertical, aHorizontal);

  selection->CollapseToStart();

  return NS_OK;
}

void
nsCoreUtils::ScrollFrameToPoint(nsIFrame *aScrollableFrame,
                                nsIFrame *aFrame,
                                const nsIntPoint& aPoint)
{
  nsIScrollableFrame* scrollableFrame = do_QueryFrame(aScrollableFrame);
  if (!scrollableFrame)
    return;

  nsPoint point =
    aPoint.ToAppUnits(aFrame->PresContext()->AppUnitsPerDevPixel());
  nsRect frameRect = aFrame->GetScreenRectInAppUnits();
  nsPoint deltaPoint(point.x - frameRect.x, point.y - frameRect.y);

  nsPoint scrollPoint = scrollableFrame->GetScrollPosition();
  scrollPoint -= deltaPoint;

  scrollableFrame->ScrollTo(scrollPoint, nsIScrollableFrame::INSTANT);
}

void
nsCoreUtils::ConvertScrollTypeToPercents(uint32_t aScrollType,
                                         nsIPresShell::ScrollAxis *aVertical,
                                         nsIPresShell::ScrollAxis *aHorizontal)
{
  int16_t whereY, whereX;
  nsIPresShell::WhenToScroll whenY, whenX;
  switch (aScrollType)
  {
    case nsIAccessibleScrollType::SCROLL_TYPE_TOP_LEFT:
      whereY = nsIPresShell::SCROLL_TOP;
      whenY  = nsIPresShell::SCROLL_ALWAYS;
      whereX = nsIPresShell::SCROLL_LEFT;
      whenX  = nsIPresShell::SCROLL_ALWAYS;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_RIGHT:
      whereY = nsIPresShell::SCROLL_BOTTOM;
      whenY  = nsIPresShell::SCROLL_ALWAYS;
      whereX = nsIPresShell::SCROLL_RIGHT;
      whenX  = nsIPresShell::SCROLL_ALWAYS;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE:
      whereY = nsIPresShell::SCROLL_TOP;
      whenY  = nsIPresShell::SCROLL_ALWAYS;
      whereX = nsIPresShell::SCROLL_MINIMUM;
      whenX  = nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_EDGE:
      whereY = nsIPresShell::SCROLL_BOTTOM;
      whenY  = nsIPresShell::SCROLL_ALWAYS;
      whereX = nsIPresShell::SCROLL_MINIMUM;
      whenX  = nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_LEFT_EDGE:
      whereY = nsIPresShell::SCROLL_MINIMUM;
      whenY  = nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE;
      whereX = nsIPresShell::SCROLL_LEFT;
      whenX  = nsIPresShell::SCROLL_ALWAYS;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_RIGHT_EDGE:
      whereY = nsIPresShell::SCROLL_MINIMUM;
      whenY  = nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE;
      whereX = nsIPresShell::SCROLL_RIGHT;
      whenX  = nsIPresShell::SCROLL_ALWAYS;
      break;
    default:
      whereY = nsIPresShell::SCROLL_MINIMUM;
      whenY  = nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE;
      whereX = nsIPresShell::SCROLL_MINIMUM;
      whenX  = nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE;
  }
  *aVertical = nsIPresShell::ScrollAxis(whereY, whenY);
  *aHorizontal = nsIPresShell::ScrollAxis(whereX, whenX);
}

nsIntPoint
nsCoreUtils::GetScreenCoordsForWindow(nsINode *aNode)
{
  nsIntPoint coords(0, 0);
  nsCOMPtr<nsIDocShellTreeItem> treeItem(GetDocShellFor(aNode));
  if (!treeItem)
    return coords;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner)
    return coords;

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(treeOwner);
  if (baseWindow)
    baseWindow->GetPosition(&coords.x, &coords.y); // in device pixels

  return coords;
}

already_AddRefed<nsIDocShell>
nsCoreUtils::GetDocShellFor(nsINode *aNode)
{
  if (!aNode)
    return nullptr;

  nsCOMPtr<nsIDocShell> docShell = aNode->OwnerDoc()->GetDocShell();
  return docShell.forget();
}

bool
nsCoreUtils::IsRootDocument(nsIDocument *aDocument)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = aDocument->GetDocShell();
  NS_ASSERTION(docShellTreeItem, "No document shell for document!");

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  docShellTreeItem->GetParent(getter_AddRefs(parentTreeItem));

  return !parentTreeItem;
}

bool
nsCoreUtils::IsContentDocument(nsIDocument *aDocument)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = aDocument->GetDocShell();
  NS_ASSERTION(docShellTreeItem, "No document shell tree item for document!");

  return (docShellTreeItem->ItemType() == nsIDocShellTreeItem::typeContent);
}

bool
nsCoreUtils::IsTabDocument(nsIDocument* aDocumentNode)
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem(aDocumentNode->GetDocShell());

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  treeItem->GetParent(getter_AddRefs(parentTreeItem));

  // Tab document running in own process doesn't have parent.
  if (XRE_GetProcessType() == GeckoProcessType_Content)
    return !parentTreeItem;

  // Parent of docshell for tab document running in chrome process is root.
  nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootTreeItem));

  return parentTreeItem == rootTreeItem;
}

bool
nsCoreUtils::IsErrorPage(nsIDocument *aDocument)
{
  nsIURI *uri = aDocument->GetDocumentURI();
  bool isAboutScheme = false;
  uri->SchemeIs("about", &isAboutScheme);
  if (!isAboutScheme)
    return false;

  nsAutoCString path;
  uri->GetPath(path);

  NS_NAMED_LITERAL_CSTRING(neterror, "neterror");
  NS_NAMED_LITERAL_CSTRING(certerror, "certerror");

  return StringBeginsWith(path, neterror) || StringBeginsWith(path, certerror);
}

bool
nsCoreUtils::GetID(nsIContent *aContent, nsAString& aID)
{
  nsIAtom *idAttribute = aContent->GetIDAttributeName();
  return idAttribute ? aContent->GetAttr(kNameSpaceID_None, idAttribute, aID) : false;
}

bool
nsCoreUtils::GetUIntAttr(nsIContent *aContent, nsIAtom *aAttr, int32_t *aUInt)
{
  nsAutoString value;
  aContent->GetAttr(kNameSpaceID_None, aAttr, value);
  if (!value.IsEmpty()) {
    nsresult error = NS_OK;
    int32_t integer = value.ToInteger(&error);
    if (NS_SUCCEEDED(error) && integer > 0) {
      *aUInt = integer;
      return true;
    }
  }

  return false;
}

void
nsCoreUtils::GetLanguageFor(nsIContent *aContent, nsIContent *aRootContent,
                            nsAString& aLanguage)
{
  aLanguage.Truncate();

  nsIContent *walkUp = aContent;
  while (walkUp && walkUp != aRootContent &&
         !walkUp->GetAttr(kNameSpaceID_None, nsGkAtoms::lang, aLanguage))
    walkUp = walkUp->GetParent();
}

already_AddRefed<nsIBoxObject>
nsCoreUtils::GetTreeBodyBoxObject(nsITreeBoxObject *aTreeBoxObj)
{
  nsCOMPtr<nsIDOMElement> tcElm;
  aTreeBoxObj->GetTreeBody(getter_AddRefs(tcElm));
  nsCOMPtr<nsIDOMXULElement> tcXULElm(do_QueryInterface(tcElm));
  if (!tcXULElm)
    return nullptr;

  nsCOMPtr<nsIBoxObject> boxObj;
  tcXULElm->GetBoxObject(getter_AddRefs(boxObj));
  return boxObj.forget();
}

already_AddRefed<nsITreeBoxObject>
nsCoreUtils::GetTreeBoxObject(nsIContent *aContent)
{
  // Find DOMNode's parents recursively until reach the <tree> tag
  nsIContent* currentContent = aContent;
  while (currentContent) {
    if (currentContent->NodeInfo()->Equals(nsGkAtoms::tree,
                                           kNameSpaceID_XUL)) {
      // We will get the nsITreeBoxObject from the tree node
      nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(currentContent));
      if (xulElement) {
        nsCOMPtr<nsIBoxObject> box;
        xulElement->GetBoxObject(getter_AddRefs(box));
        nsCOMPtr<nsITreeBoxObject> treeBox(do_QueryInterface(box));
        if (treeBox)
          return treeBox.forget();
      }
    }
    currentContent = currentContent->GetFlattenedTreeParent();
  }

  return nullptr;
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetFirstSensibleColumn(nsITreeBoxObject *aTree)
{
  nsCOMPtr<nsITreeColumns> cols;
  aTree->GetColumns(getter_AddRefs(cols));
  if (!cols)
    return nullptr;

  nsCOMPtr<nsITreeColumn> column;
  cols->GetFirstColumn(getter_AddRefs(column));
  if (column && IsColumnHidden(column))
    return GetNextSensibleColumn(column);

  return column.forget();
}

uint32_t
nsCoreUtils::GetSensibleColumnCount(nsITreeBoxObject *aTree)
{
  uint32_t count = 0;

  nsCOMPtr<nsITreeColumns> cols;
  aTree->GetColumns(getter_AddRefs(cols));
  if (!cols)
    return count;

  nsCOMPtr<nsITreeColumn> column;
  cols->GetFirstColumn(getter_AddRefs(column));

  while (column) {
    if (!IsColumnHidden(column))
      count++;

    nsCOMPtr<nsITreeColumn> nextColumn;
    column->GetNext(getter_AddRefs(nextColumn));
    column.swap(nextColumn);
  }

  return count;
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetSensibleColumnAt(nsITreeBoxObject *aTree, uint32_t aIndex)
{
  uint32_t idx = aIndex;

  nsCOMPtr<nsITreeColumn> column = GetFirstSensibleColumn(aTree);
  while (column) {
    if (idx == 0)
      return column.forget();

    idx--;
    column = GetNextSensibleColumn(column);
  }

  return nullptr;
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetNextSensibleColumn(nsITreeColumn *aColumn)
{
  nsCOMPtr<nsITreeColumn> nextColumn;
  aColumn->GetNext(getter_AddRefs(nextColumn));

  while (nextColumn && IsColumnHidden(nextColumn)) {
    nsCOMPtr<nsITreeColumn> tempColumn;
    nextColumn->GetNext(getter_AddRefs(tempColumn));
    nextColumn.swap(tempColumn);
  }

  return nextColumn.forget();
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetPreviousSensibleColumn(nsITreeColumn *aColumn)
{
  nsCOMPtr<nsITreeColumn> prevColumn;
  aColumn->GetPrevious(getter_AddRefs(prevColumn));

  while (prevColumn && IsColumnHidden(prevColumn)) {
    nsCOMPtr<nsITreeColumn> tempColumn;
    prevColumn->GetPrevious(getter_AddRefs(tempColumn));
    prevColumn.swap(tempColumn);
  }

  return prevColumn.forget();
}

bool
nsCoreUtils::IsColumnHidden(nsITreeColumn *aColumn)
{
  nsCOMPtr<nsIDOMElement> element;
  aColumn->GetElement(getter_AddRefs(element));
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);
  return content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                              nsGkAtoms::_true, eCaseMatters);
}

void
nsCoreUtils::ScrollTo(nsIPresShell* aPresShell, nsIContent* aContent,
                      uint32_t aScrollType)
{
  nsIPresShell::ScrollAxis vertical, horizontal;
  ConvertScrollTypeToPercents(aScrollType, &vertical, &horizontal);
  aPresShell->ScrollContentIntoView(aContent, vertical, horizontal,
                                    nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}

bool
nsCoreUtils::IsWhitespaceString(const nsSubstring& aString)
{
  nsSubstring::const_char_iterator iterBegin, iterEnd;

  aString.BeginReading(iterBegin);
  aString.EndReading(iterEnd);

  while (iterBegin != iterEnd && IsWhitespace(*iterBegin))
    ++iterBegin;

  return iterBegin == iterEnd;
}


////////////////////////////////////////////////////////////////////////////////
// nsAccessibleDOMStringList
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsAccessibleDOMStringList, nsIDOMDOMStringList)

NS_IMETHODIMP
nsAccessibleDOMStringList::Item(uint32_t aIndex, nsAString& aResult)
{
  if (aIndex >= mNames.Length())
    SetDOMStringToNull(aResult);
  else
    aResult = mNames.ElementAt(aIndex);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibleDOMStringList::GetLength(uint32_t* aLength)
{
  *aLength = mNames.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibleDOMStringList::Contains(const nsAString& aString, bool* aResult)
{
  *aResult = mNames.Contains(aString);

  return NS_OK;
}

