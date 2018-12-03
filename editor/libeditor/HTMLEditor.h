/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HTMLEditor_h
#define mozilla_HTMLEditor_h

#include "mozilla/Attributes.h"
#include "mozilla/ComposerCommandsUpdater.h"
#include "mozilla/CSSEditUtils.h"
#include "mozilla/ManualNAC.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/TextEditor.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/File.h"

#include "nsAttrName.h"
#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMEventListener.h"
#include "nsIEditorMailSupport.h"
#include "nsIEditorStyleSheets.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLInlineTableEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsITableEditor.h"
#include "nsPoint.h"
#include "nsStubMutationObserver.h"
#include "nsTArray.h"

class nsDocumentFragment;
class nsHTMLDocument;
class nsITransferable;
class nsIClipboard;
class nsILinkHandler;
class nsTableWrapperFrame;
class nsRange;

namespace mozilla {
class AutoSelectionSetterAfterTableEdit;
class AutoSetTemporaryAncestorLimiter;
class EmptyEditableFunctor;
class ResizerSelectionListener;
enum class EditSubAction : int32_t;
struct PropItem;
template <class T>
class OwningNonNull;
namespace dom {
class Blob;
class DocumentFragment;
class Event;
class MouseEvent;
}  // namespace dom
namespace widget {
struct IMEState;
}  // namespace widget

enum class ParagraphSeparator { div, p, br };

/**
 * The HTML editor implementation.<br>
 * Use to edit HTML document represented as a DOM tree.
 */
class HTMLEditor final : public TextEditor,
                         public nsIHTMLEditor,
                         public nsIHTMLObjectResizer,
                         public nsIHTMLAbsPosEditor,
                         public nsITableEditor,
                         public nsIHTMLInlineTableEditor,
                         public nsIEditorStyleSheets,
                         public nsStubMutationObserver,
                         public nsIEditorMailSupport {
 public:
  /****************************************************************************
   * NOTE: DO NOT MAKE YOUR NEW METHODS PUBLIC IF they are called by other
   *       classes under libeditor except EditorEventListener and
   *       HTMLEditorEventListener because each public method which may fire
   *       eEditorInput event will need to instantiate new stack class for
   *       managing input type value of eEditorInput and cache some objects
   *       for smarter handling.  In other words, when you add new root
   *       method to edit the DOM tree, you can make your new method public.
   ****************************************************************************/

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLEditor, TextEditor)

  // nsStubMutationObserver overrides
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  // nsIHTMLEditor methods
  NS_DECL_NSIHTMLEDITOR

  // nsIHTMLObjectResizer methods (implemented in HTMLObjectResizer.cpp)
  NS_DECL_NSIHTMLOBJECTRESIZER

  // nsIHTMLAbsPosEditor methods (implemented in HTMLAbsPositionEditor.cpp)
  NS_DECL_NSIHTMLABSPOSEDITOR

  // nsIHTMLInlineTableEditor methods (implemented in HTMLInlineTableEditor.cpp)
  NS_DECL_NSIHTMLINLINETABLEEDITOR

  // nsIEditorStyleSheets methods
  NS_DECL_NSIEDITORSTYLESHEETS

  // nsIEditorMailSupport methods
  NS_DECL_NSIEDITORMAILSUPPORT

  // nsITableEditor methods
  NS_DECL_NSITABLEEDITOR

  // nsISelectionListener overrides
  NS_DECL_NSISELECTIONLISTENER

  HTMLEditor();

  nsHTMLDocument* GetHTMLDocument() const;

  virtual void PreDestroy(bool aDestroyingFrames) override;

  bool GetReturnInParagraphCreatesNewParagraph();

  // TextEditor overrides
  virtual nsresult Init(nsIDocument& aDoc, Element* aRoot,
                        nsISelectionController* aSelCon, uint32_t aFlags,
                        const nsAString& aValue) override;
  NS_IMETHOD BeginningOfDocument() override;
  NS_IMETHOD SetFlags(uint32_t aFlags) override;

  NS_IMETHOD CanPaste(int32_t aSelectionType, bool* aCanPaste) override;

  NS_IMETHOD PasteTransferable(nsITransferable* aTransferable) override;

  NS_IMETHOD DeleteNode(nsINode* aNode) override;

  NS_IMETHOD InsertLineBreak() override;

  virtual nsresult HandleKeyPressEvent(
      WidgetKeyboardEvent* aKeyboardEvent) override;
  virtual nsIContent* GetFocusedContent() override;
  virtual already_AddRefed<nsIContent> GetFocusedContentForIME() override;
  virtual bool IsActiveInDOMWindow() override;
  virtual dom::EventTarget* GetDOMEventTarget() override;
  virtual Element* FindSelectionRoot(nsINode* aNode) const override;
  virtual bool IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent) override;
  virtual nsresult GetPreferredIMEState(widget::IMEState* aState) override;

  /**
   * PasteAsQuotationAsAction() pastes content in clipboard with newly created
   * blockquote element.  If the editor is in plaintext mode, will paste the
   * content with appending ">" to start of each line.
   *
   * @param aClipboardType      nsIClipboard::kGlobalClipboard or
   *                            nsIClipboard::kSelectionClipboard.
   * @param aDispatchPasteEvent true if this should dispatch ePaste event
   *                            before pasting.  Otherwise, false.
   */
  virtual nsresult PasteAsQuotationAsAction(int32_t aClipboardType,
                                            bool aDispatchPasteEvent) override;

  /**
   * Can we paste |aTransferable| or, if |aTransferable| is null, will a call
   * to pasteTransferable later possibly succeed if given an instance of
   * nsITransferable then? True if the doc is modifiable, and, if
   * |aTransfeable| is non-null, we have pasteable data in |aTransfeable|.
   */
  virtual bool CanPasteTransferable(nsITransferable* aTransferable) override;

  /**
   * InsertLineBreakAsAction() is called when user inputs a line break with
   * Shift + Enter or something.
   */
  virtual nsresult InsertLineBreakAsAction() override;

  /**
   * InsertParagraphSeparatorAsAction() is called when user tries to separate
   * current paragraph with Enter key press in HTMLEditor or something.
   */
  nsresult InsertParagraphSeparatorAsAction();

  /**
   * CreateElementWithDefaults() creates new element whose name is
   * aTagName with some default attributes are set.  Note that this is a
   * public utility method.  I.e., just creates element, not insert it
   * into the DOM tree.
   * NOTE: This is available for internal use too since this does not change
   *       the DOM tree nor undo transactions, and does not refer Selection,
   *       HTMLEditRules, etc.
   *
   * @param aTagName            The new element's tag name.  If the name is
   *                            one of "href", "anchor" or "namedanchor",
   *                            this creates an <a> element.
   * @return                    Newly created element.
   */
  already_AddRefed<Element> CreateElementWithDefaults(const nsAtom& aTagName);

  /**
   * Indent or outdent content around Selection.
   */
  nsresult IndentAsAction();
  nsresult OutdentAsAction();

  /**
   * event callback when a mouse button is pressed
   * @param aX      [IN] horizontal position of the pointer
   * @param aY      [IN] vertical position of the pointer
   * @param aTarget [IN] the element triggering the event
   * @param aMouseEvent [IN] the event
   */
  nsresult OnMouseDown(int32_t aX, int32_t aY, Element* aTarget,
                       dom::Event* aMouseEvent);

  /**
   * event callback when a mouse button is released
   * @param aX      [IN] horizontal position of the pointer
   * @param aY      [IN] vertical position of the pointer
   * @param aTarget [IN] the element triggering the event
   */
  nsresult OnMouseUp(int32_t aX, int32_t aY, Element* aTarget);

  /**
   * event callback when the mouse pointer is moved
   * @param aMouseEvent [IN] the event
   */
  MOZ_CAN_RUN_SCRIPT nsresult OnMouseMove(dom::MouseEvent* aMouseEvent);

  /**
   * IsCSSEnabled() returns true if this editor treats styles with style
   * attribute of HTML elements.  Otherwise, if this editor treats all styles
   * with "font style elements" like <b>, <i>, etc, and <blockquote> to indent,
   * align attribute to align contents, returns false.
   */
  bool IsCSSEnabled() const {
    // TODO: removal of mCSSAware and use only the presence of mCSSEditUtils
    return mCSSAware && mCSSEditUtils && mCSSEditUtils->IsCSSPrefChecked();
  }

  /**
   * Enable/disable object resizers for <img> elements, <table> elements,
   * absolute positioned elements (required absolute position editor enabled).
   */
  void EnableObjectResizer(bool aEnable) {
    if (mIsObjectResizingEnabled == aEnable) {
      return;
    }

    AutoEditActionDataSetter editActionData(
        *this, EditAction::eEnableOrDisableResizer);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return;
    }

    mIsObjectResizingEnabled = aEnable;
    RefereshEditingUI();
  }
  bool IsObjectResizerEnabled() const { return mIsObjectResizingEnabled; }

  /**
   * Enable/disable inline table editor, e.g., adding new row or column,
   * removing existing row or column.
   */
  void EnableInlineTableEditor(bool aEnable) {
    if (mIsInlineTableEditingEnabled == aEnable) {
      return;
    }

    AutoEditActionDataSetter editActionData(
        *this, EditAction::eEnableOrDisableInlineTableEditingUI);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return;
    }

    mIsInlineTableEditingEnabled = aEnable;
    RefereshEditingUI();
  }
  bool IsInlineTableEditorEnabled() const {
    return mIsInlineTableEditingEnabled;
  }

  /**
   * Enable/disable absolute position editor, resizing absolute positioned
   * elements (required object resizers enabled) or positioning them with
   * dragging grabber.
   */
  void EnableAbsolutePositionEditor(bool aEnable) {
    if (mIsAbsolutelyPositioningEnabled == aEnable) {
      return;
    }

    AutoEditActionDataSetter editActionData(
        *this, EditAction::eEnableOrDisableAbsolutePositionEditor);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return;
    }

    mIsAbsolutelyPositioningEnabled = aEnable;
    RefereshEditingUI();
  }
  bool IsAbsolutePositionEditorEnabled() const {
    return mIsAbsolutelyPositioningEnabled;
  }

  // non-virtual methods of interface methods

  /**
   * returns the deepest absolutely positioned container of the selection
   * if it exists or null.
   */
  already_AddRefed<Element> GetAbsolutelyPositionedSelectionContainer();

  Element* GetPositionedElement() const { return mAbsolutelyPositionedObject; }

  /**
   * extracts the selection from the normal flow of the document and
   * positions it.
   * @param aEnabled [IN] true to absolutely position the selection,
   *                      false to put it back in the normal flow
   */
  nsresult SetSelectionToAbsoluteOrStatic(bool aEnabled);

  /**
   * returns the absolute z-index of a positioned element. Never returns 'auto'
   * @return         the z-index of the element
   * @param aElement [IN] the element.
   */
  int32_t GetZIndex(Element& aElement);

  /**
   * adds aChange to the z-index of the currently positioned element.
   * @param aChange [IN] relative change to apply to current z-index
   */
  nsresult AddZIndex(int32_t aChange);

  /**
   * SetInlinePropertyAsAction() sets a property which changes inline style of
   * text.  E.g., bold, italic, super and sub.
   * This automatically removes exclusive style, however, treats all changes
   * as a transaction.
   */
  nsresult SetInlinePropertyAsAction(nsAtom& aProperty, nsAtom* aAttribute,
                                     const nsAString& aValue);

  nsresult GetInlineProperty(nsAtom* aProperty, nsAtom* aAttribute,
                             const nsAString& aValue, bool* aFirst, bool* aAny,
                             bool* aAll);
  nsresult GetInlinePropertyWithAttrValue(nsAtom* aProperty, nsAtom* aAttr,
                                          const nsAString& aValue, bool* aFirst,
                                          bool* aAny, bool* aAll,
                                          nsAString& outValue);

  /**
   * RemoveInlinePropertyAsAction() removes a property which changes inline
   * style of text.  E.g., bold, italic, super and sub.
   *
   * @param aProperty   Tag name whcih represents the inline style you want to
   *                    remove.  E.g., nsGkAtoms::strong, nsGkAtoms::b, etc.
   *                    If nsGkAtoms::href, <a> element which has href
   *                    attribute will be removed.
   *                    If nsGkAtoms::name, <a> element which has non-empty
   *                    name attribute will be removed.
   * @param aAttribute  If aProperty is nsGkAtoms::font, aAttribute should be
   *                    nsGkAtoms::fase, nsGkAtoms::size, nsGkAtoms::color or
   *                    nsGkAtoms::bgcolor.  Otherwise, set nullptr.
   *                    Must not use nsGkAtoms::_empty here.
   */
  nsresult RemoveInlinePropertyAsAction(nsAtom& aProperty, nsAtom* aAttribute);

  /**
   * GetFontColorState() returns foreground color information in first
   * range of Selection.
   * If first range of Selection is collapsed and there is a cache of style for
   * new text, aIsMixed is set to false and aColor is set to the cached color.
   * If first range of Selection is collapsed and there is no cached color,
   * this returns the color of the node, aIsMixed is set to false and aColor is
   * set to the color.
   * If first range of Selection is not collapsed, this collects colors of
   * each node in the range.  If there are two or more colors, aIsMixed is set
   * to true and aColor is truncated.  If only one color is set to all of the
   * range, aIsMixed is set to false and aColor is set to the color.
   * If there is no Selection ranges, aIsMixed is set to false and aColor is
   * truncated.
   *
   * @param aIsMixed            Must not be nullptr.  This is set to true
   *                            if there is two or more colors in first
   *                            range of Selection.
   * @param aColor              Returns the color if only one color is set to
   *                            all of first range in Selection.  Otherwise,
   *                            returns empty string.
   * @return                    Returns error only when illegal cases, e.g.,
   *                            Selection instance has gone, first range
   *                            Selection is broken.
   */
  nsresult GetFontColorState(bool* aIsMixed, nsAString& aColor);

  /**
   * SetComposerCommandsUpdater() sets or unsets mComposerCommandsUpdater.
   * This will crash in debug build if the editor already has an instance
   * but called with another instance.
   */
  void SetComposerCommandsUpdater(
      ComposerCommandsUpdater* aComposerCommandsUpdater) {
    MOZ_ASSERT(!aComposerCommandsUpdater || !mComposerCommandsUpdater ||
               aComposerCommandsUpdater == mComposerCommandsUpdater);
    mComposerCommandsUpdater = aComposerCommandsUpdater;
  }

  ParagraphSeparator GetDefaultParagraphSeparator() const {
    return mDefaultParagraphSeparator;
  }
  void SetDefaultParagraphSeparator(ParagraphSeparator aSep) {
    mDefaultParagraphSeparator = aSep;
  }

  /**
   * Modifies the table containing the selection according to the
   * activation of an inline table editing UI element
   * @param aUIAnonymousElement [IN] the inline table editing UI element
   */
  nsresult DoInlineTableEditingAction(const Element& aUIAnonymousElement);

  /**
   * GetElementOrParentByTagName() looks for an element node whose name matches
   * aTagName from aNode or anchor node of Selection to <body> element.
   *
   * @param aTagName        The tag name which you want to look for.
   *                        Must not be nsGkAtoms::_empty.
   *                        If nsGkAtoms::list, the result may be <ul>, <ol> or
   *                        <dl> element.
   *                        If nsGkAtoms::td, the result may be <td> or <th>.
   *                        If nsGkAtoms::href, the result may be <a> element
   *                        which has "href" attribute with non-empty value.
   *                        If nsGkAtoms::anchor, the result may be <a> which
   *                        has "name" attribute with non-empty value.
   * @param aNode           If non-nullptr, this starts to look for the result
   *                        from it.  Otherwise, i.e., nullptr, starts from
   *                        anchor node of Selection.
   * @return                If an element which matches aTagName, returns
   *                        an Element.  Otherwise, nullptr.
   */
  Element* GetElementOrParentByTagName(const nsAtom& aTagName,
                                       nsINode* aNode) const;

  /**
   * Get an active editor's editing host in DOM window.  If this editor isn't
   * active in the DOM window, this returns NULL.
   */
  Element* GetActiveEditingHost() const;

  /** Insert a string as quoted text
   * (whose representation is dependant on the editor type),
   * replacing the selected text (if any).
   *
   * @param aQuotedText    The actual text to be quoted
   * @parem aNodeInserted  Return the node which was inserted.
   */
  nsresult InsertAsQuotation(const nsAString& aQuotedText,
                             nsINode** aNodeInserted);

  /**
   * Inserts a plaintext string at the current location,
   * with special processing for lines beginning with ">",
   * which will be treated as mail quotes and inserted
   * as plaintext quoted blocks.
   * If the selection is not collapsed, the selection is deleted
   * and the insertion takes place at the resulting collapsed selection.
   *
   * @param aString   the string to be inserted
   */
  nsresult InsertTextWithQuotations(const nsAString& aStringToInsert);

 protected:  // May be called by friends.
  /****************************************************************************
   * Some classes like TextEditRules, HTMLEditRules, WSRunObject which are
   * part of handling edit actions are allowed to call the following protected
   * methods.  However, those methods won't prepare caches of some objects
   * which are necessary for them.  So, if you want some following methods
   * to do that for you, you need to create a wrapper method in public scope
   * and call it.
   ****************************************************************************/

  /**
   * DeleteSelectionWithTransaction() removes selected content or content
   * around caret with transactions.
   *
   * @param aDirection          How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.
   */
  virtual nsresult DeleteSelectionWithTransaction(
      EDirection aAction, EStripWrappers aStripWrappers) override;

  /**
   * DeleteNodeWithTransaction() removes aNode from the DOM tree if it's
   * modifiable.  Note that this is not an override of same method of
   * EditorBase.
   *
   * @param aNode       The node to be removed from the DOM tree.
   */
  nsresult DeleteNodeWithTransaction(nsINode& aNode);

  /**
   * DeleteTextWithTransaction() removes text in the range from aCharData if
   * it's modifiable.  Note that this not an override of same method of
   * EditorBase.
   *
   * @param aCharData           The data node which should be modified.
   * @param aOffset             Start offset of removing text in aCharData.
   * @param aLength             Length of removing text.
   */
  nsresult DeleteTextWithTransaction(dom::CharacterData& aTextNode,
                                     uint32_t aOffset, uint32_t aLength);

  /**
   * InsertTextWithTransaction() inserts aStringToInsert at aPointToInsert.
   */
  virtual nsresult InsertTextWithTransaction(
      nsIDocument& aDocument, const nsAString& aStringToInsert,
      const EditorRawDOMPoint& aPointToInsert,
      EditorRawDOMPoint* aPointAfterInsertedString = nullptr) override;

  /**
   * CopyLastEditableChildStyles() clones inline container elements into
   * aPreviousBlock to aNewBlock to keep using same style in it.
   *
   * @param aPreviousBlock      The previous block element.  All inline
   *                            elements which are last sibling of each level
   *                            are cloned to aNewBlock.
   * @param aNewBlock           New block container element.
   * @param aNewBrElement       If this method creates a new <br> element for
   *                            placeholder, this is set to the new <br>
   *                            element.
   */
  nsresult CopyLastEditableChildStylesWithTransaction(
      Element& aPreviousBlock, Element& aNewBlock,
      RefPtr<Element>* aNewBrElement);

  /**
   * RemoveBlockContainerWithTransaction() removes aElement from the DOM tree
   * but moves its all children to its parent node and if its parent needs <br>
   * element to have at least one line-height, this inserts <br> element
   * automatically.
   *
   * @param aElement            Block element to be removed.
   */
  nsresult RemoveBlockContainerWithTransaction(Element& aElement);

  virtual Element* GetEditorRoot() const override;
  using EditorBase::IsEditable;
  virtual nsresult RemoveAttributeOrEquivalent(
      Element* aElement, nsAtom* aAttribute,
      bool aSuppressTransaction) override;
  virtual nsresult SetAttributeOrEquivalent(Element* aElement,
                                            nsAtom* aAttribute,
                                            const nsAString& aValue,
                                            bool aSuppressTransaction) override;
  using EditorBase::RemoveAttributeOrEquivalent;
  using EditorBase::SetAttributeOrEquivalent;

  /**
   * GetBlockNodeParent() returns parent or nearest ancestor of aNode if
   * there is a block parent.  If aAncestorLimiter is not nullptr,
   * this stops looking for the result.
   */
  static Element* GetBlockNodeParent(nsINode* aNode,
                                     nsINode* aAncestorLimiter = nullptr);

  /**
   * GetBlock() returns aNode itself, or parent or nearest ancestor of aNode
   * if there is a block parent.  If aAncestorLimiter is not nullptr,
   * this stops looking for the result.
   */
  static Element* GetBlock(nsINode& aNode, nsINode* aAncestorLimiter = nullptr);

  /**
   * Returns container element of ranges in Selection.  If Selection is
   * collapsed, returns focus container node (or its parent element).
   * If Selection selects only one element node, returns the element node.
   * If Selection is only one range, returns common ancestor of the range.
   * XXX If there are two or more Selection ranges, this returns parent node
   *     of start container of a range which starts with different node from
   *     start container of the first range.
   */
  Element* GetSelectionContainerElement() const;

  /**
   * GetFirstSelectedTableCellElement() returns a <td> or <th> element if
   * first range of Selection (i.e., result of Selection::GetRangeAt(0))
   * selects a <td> element or <th> element.  Even if Selection is in
   * a cell element, this returns nullptr.  And even if 2nd or later
   * range of Selection selects a cell element, also returns nullptr.
   * Note that when this looks for a cell element, this resets the internal
   * index of ranges of Selection.  When you call
   * GetNextSelectedTableCellElement() after a call of this, it'll return 2nd
   * selected cell if there is.
   *
   * @param aRv                 Returns error if there is no selection or
   *                            first range of Selection is unexpected.
   * @return                    A <td> or <th> element is selected by first
   *                            range of Selection.  Note that the range must
   *                            be: startContaienr and endContainer are same
   *                            <tr> element, startOffset + 1 equals endOffset.
   */
  already_AddRefed<Element> GetFirstSelectedTableCellElement(
      ErrorResult& aRv) const;

  /**
   * GetNextSelectedTableCellElement() is a stateful method to retrieve
   * selected table cell elements which are selected by 2nd or later ranges
   * of Selection.  When you call GetFirstSelectedTableCellElement(), it
   * resets internal counter of this method.  Then, following calls of
   * GetNextSelectedTableCellElement() scans the remaining ranges of Selection.
   * If a range selects a <td> or <th> element, returns the cell element.
   * If a range selects an element but neither <td> nor <th> element, this
   * ignores the range.  If a range is in a text node, returns null without
   * throwing exception, but stops scanning the remaining ranges even you
   * call this again.
   * Note that this may cross <table> boundaries since this method just
   * scans all ranges of Selection.  Therefore, returning cells which
   * belong to different <table> elements.
   *
   * @param aRv                 Returns error if Selection doesn't have
   *                            range properly.
   * @return                    A <td> or <th> element if one of remaining
   *                            ranges selects a <td> or <th> element unless
   *                            this does not meet a range in a text node.
   */
  already_AddRefed<Element> GetNextSelectedTableCellElement(
      ErrorResult& aRv) const;

  /**
   * DeleteTableCellContentsWithTransaction() removes any contents in cell
   * elements.  If two or more cell elements are selected, this removes
   * all selected cells' contents.  Otherwise, this removes contents of
   * a cell which contains first selection range.  This does not return
   * error even if selection is not in cell element, just does nothing.
   */
  nsresult DeleteTableCellContentsWithTransaction();

  void IsNextCharInNodeWhitespace(nsIContent* aContent, int32_t aOffset,
                                  bool* outIsSpace, bool* outIsNBSP,
                                  nsIContent** outNode = nullptr,
                                  int32_t* outOffset = 0);
  void IsPrevCharInNodeWhitespace(nsIContent* aContent, int32_t aOffset,
                                  bool* outIsSpace, bool* outIsNBSP,
                                  nsIContent** outNode = nullptr,
                                  int32_t* outOffset = 0);

  /**
   * @param aElement        Must not be null.
   */
  static bool NodeIsBlockStatic(const nsINode* aElement);

  /**
   * extracts an element from the normal flow of the document and
   * positions it, and puts it back in the normal flow.
   * @param aElement [IN] the element
   * @param aEnabled [IN] true to absolutely position the element,
   *                      false to put it back in the normal flow
   */
  nsresult SetPositionToAbsoluteOrStatic(Element& aElement, bool aEnabled);

  /**
   * adds aChange to the z-index of an arbitrary element.
   * @param aElement [IN] the element
   * @param aChange  [IN] relative change to apply to current z-index of
   *                      the element
   * @param aReturn  [OUT] the new z-index of the element
   */
  nsresult RelativeChangeElementZIndex(Element& aElement, int32_t aChange,
                                       int32_t* aReturn);

  virtual bool IsBlockNode(nsINode* aNode) override;
  using EditorBase::IsBlockNode;

  /**
   * returns true if aParentTag can contain a child of type aChildTag.
   */
  virtual bool TagCanContainTag(nsAtom& aParentTag,
                                nsAtom& aChildTag) const override;

  /**
   * Returns true if aNode is a container.
   */
  virtual bool IsContainer(nsINode* aNode) override;

  /**
   * Join together any adjacent editable text nodes in the range.
   */
  nsresult CollapseAdjacentTextNodes(nsRange* aRange);

  /**
   * IsInVisibleTextFrames() returns true if all text in aText is in visible
   * text frames.  Callers have to guarantee that there is no pending reflow.
   */
  bool IsInVisibleTextFrames(dom::Text& aText);

  /**
   * IsVisibleTextNode() returns true if aText has visible text.  If it has
   * only whitespaces and they are collapsed, returns false.
   */
  bool IsVisibleTextNode(Text& aText);

  /**
   * aNode must be a non-null text node.
   * outIsEmptyNode must be non-null.
   */
  nsresult IsEmptyNode(nsINode* aNode, bool* outIsEmptyBlock,
                       bool aMozBRDoesntCount = false,
                       bool aListOrCellNotEmpty = false,
                       bool aSafeToAskFrames = false);
  nsresult IsEmptyNodeImpl(nsINode* aNode, bool* outIsEmptyBlock,
                           bool aMozBRDoesntCount, bool aListOrCellNotEmpty,
                           bool aSafeToAskFrames, bool* aSeenBR);

  static bool HasAttributes(Element* aElement) {
    MOZ_ASSERT(aElement);
    uint32_t attrCount = aElement->GetAttrCount();
    return attrCount > 1 ||
           (1 == attrCount &&
            !aElement->GetAttrNameAt(0)->Equals(nsGkAtoms::mozdirty));
  }

  /**
   * Content-based query returns true if <aProperty aAttribute=aValue> effects
   * aNode.  If <aProperty aAttribute=aValue> contains aNode, but
   * <aProperty aAttribute=SomeOtherValue> also contains aNode and the second is
   * more deeply nested than the first, then the first does not effect aNode.
   *
   * @param aNode      The target of the query
   * @param aProperty  The property that we are querying for
   * @param aAttribute The attribute of aProperty, example: color in
   *                   <FONT color="blue"> May be null.
   * @param aValue     The value of aAttribute, example: blue in
   *                   <FONT color="blue"> May be null.  Ignored if aAttribute
   *                   is null.
   * @param outValue   [OUT] the value of the attribute, if aIsSet is true
   * @return           true if <aProperty aAttribute=aValue> effects
   *                   aNode.
   *
   * The nsIContent variant returns aIsSet instead of using an out parameter.
   */
  bool IsTextPropertySetByContent(nsINode* aNode, nsAtom* aProperty,
                                  nsAtom* aAttribute, const nsAString* aValue,
                                  nsAString* outValue = nullptr);

  static dom::Element* GetLinkElement(nsINode* aNode);

  /**
   * Small utility routine to test if a break node is visible to user.
   */
  bool IsVisibleBRElement(nsINode* aNode);

  /**
   * Helper routines for font size changing.
   */
  enum class FontSize { incr, decr };
  nsresult RelativeFontChangeOnTextNode(FontSize aDir, Text& aTextNode,
                                        int32_t aStartOffset,
                                        int32_t aEndOffset);

  nsresult SetInlinePropertyOnNode(nsIContent& aNode, nsAtom& aProperty,
                                   nsAtom* aAttribute, const nsAString& aValue);

  nsresult SplitStyleAbovePoint(nsCOMPtr<nsINode>* aNode, int32_t* aOffset,
                                nsAtom* aProperty, nsAtom* aAttribute,
                                nsIContent** aOutLeftNode = nullptr,
                                nsIContent** aOutRightNode = nullptr);

  nsIContent* GetPriorHTMLSibling(nsINode* aNode);

  nsIContent* GetNextHTMLSibling(nsINode* aNode);

  /**
   * GetPreviousHTMLElementOrText*() methods are similar to
   * EditorBase::GetPreviousElementOrText*() but this won't return nodes
   * outside active editing host.
   */
  nsIContent* GetPreviousHTMLElementOrText(nsINode& aNode) {
    return GetPreviousHTMLElementOrTextInternal(aNode, false);
  }
  nsIContent* GetPreviousHTMLElementOrTextInBlock(nsINode& aNode) {
    return GetPreviousHTMLElementOrTextInternal(aNode, true);
  }
  template <typename PT, typename CT>
  nsIContent* GetPreviousHTMLElementOrText(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetPreviousHTMLElementOrTextInternal(aPoint, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetPreviousHTMLElementOrTextInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetPreviousHTMLElementOrTextInternal(aPoint, true);
  }

  /**
   * GetPreviousHTMLElementOrTextInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetPreviousHTMLElementOrTextInternal(nsINode& aNode,
                                                   bool aNoBlockCrossing);
  template <typename PT, typename CT>
  nsIContent* GetPreviousHTMLElementOrTextInternal(
      const EditorDOMPointBase<PT, CT>& aPoint, bool aNoBlockCrossing);

  /**
   * GetPreviousEditableHTMLNode*() methods are similar to
   * EditorBase::GetPreviousEditableNode() but this won't return nodes outside
   * active editing host.
   */
  nsIContent* GetPreviousEditableHTMLNode(nsINode& aNode) {
    return GetPreviousEditableHTMLNodeInternal(aNode, false);
  }
  nsIContent* GetPreviousEditableHTMLNodeInBlock(nsINode& aNode) {
    return GetPreviousEditableHTMLNodeInternal(aNode, true);
  }
  template <typename PT, typename CT>
  nsIContent* GetPreviousEditableHTMLNode(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetPreviousEditableHTMLNodeInternal(aPoint, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetPreviousEditableHTMLNodeInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetPreviousEditableHTMLNodeInternal(aPoint, true);
  }

  /**
   * GetPreviousEditableHTMLNodeInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetPreviousEditableHTMLNodeInternal(nsINode& aNode,
                                                  bool aNoBlockCrossing);
  template <typename PT, typename CT>
  nsIContent* GetPreviousEditableHTMLNodeInternal(
      const EditorDOMPointBase<PT, CT>& aPoint, bool aNoBlockCrossing);

  /**
   * GetNextHTMLElementOrText*() methods are similar to
   * EditorBase::GetNextElementOrText*() but this won't return nodes outside
   * active editing host.
   *
   * Note that same as EditorBase::GetTextEditableNode(), methods which take
   * |const EditorRawDOMPoint&| start to search from the node pointed by it.
   * On the other hand, methods which take |nsINode&| start to search from
   * next node of aNode.
   */
  nsIContent* GetNextHTMLElementOrText(nsINode& aNode) {
    return GetNextHTMLElementOrTextInternal(aNode, false);
  }
  nsIContent* GetNextHTMLElementOrTextInBlock(nsINode& aNode) {
    return GetNextHTMLElementOrTextInternal(aNode, true);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextHTMLElementOrText(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextHTMLElementOrTextInternal(aPoint, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextHTMLElementOrTextInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextHTMLElementOrTextInternal(aPoint, true);
  }

  /**
   * GetNextHTMLNodeInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetNextHTMLElementOrTextInternal(nsINode& aNode,
                                               bool aNoBlockCrossing);
  template <typename PT, typename CT>
  nsIContent* GetNextHTMLElementOrTextInternal(
      const EditorDOMPointBase<PT, CT>& aPoint, bool aNoBlockCrossing);

  /**
   * GetNextEditableHTMLNode*() methods are similar to
   * EditorBase::GetNextEditableNode() but this won't return nodes outside
   * active editing host.
   *
   * Note that same as EditorBase::GetTextEditableNode(), methods which take
   * |const EditorRawDOMPoint&| start to search from the node pointed by it.
   * On the other hand, methods which take |nsINode&| start to search from
   * next node of aNode.
   */
  nsIContent* GetNextEditableHTMLNode(nsINode& aNode) {
    return GetNextEditableHTMLNodeInternal(aNode, false);
  }
  nsIContent* GetNextEditableHTMLNodeInBlock(nsINode& aNode) {
    return GetNextEditableHTMLNodeInternal(aNode, true);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextEditableHTMLNode(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextEditableHTMLNodeInternal(aPoint, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextEditableHTMLNodeInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextEditableHTMLNodeInternal(aPoint, true);
  }

  /**
   * GetNextEditableHTMLNodeInternal() methods are common implementation
   * of above methods.  Please don't use this method directly.
   */
  nsIContent* GetNextEditableHTMLNodeInternal(nsINode& aNode,
                                              bool aNoBlockCrossing);
  template <typename PT, typename CT>
  nsIContent* GetNextEditableHTMLNodeInternal(
      const EditorDOMPointBase<PT, CT>& aPoint, bool aNoBlockCrossing);

  bool IsFirstEditableChild(nsINode* aNode);
  bool IsLastEditableChild(nsINode* aNode);
  nsIContent* GetFirstEditableChild(nsINode& aNode);
  nsIContent* GetLastEditableChild(nsINode& aNode);

  nsIContent* GetFirstEditableLeaf(nsINode& aNode);
  nsIContent* GetLastEditableLeaf(nsINode& aNode);

  nsresult GetInlinePropertyBase(nsAtom& aProperty, nsAtom* aAttribute,
                                 const nsAString* aValue, bool* aFirst,
                                 bool* aAny, bool* aAll, nsAString* outValue);

  nsresult ClearStyle(nsCOMPtr<nsINode>* aNode, int32_t* aOffset,
                      nsAtom* aProperty, nsAtom* aAttribute);

  nsresult SetPositionToAbsolute(Element& aElement);
  nsresult SetPositionToStatic(Element& aElement);

  /**
   * OnModifyDocument() is called when the editor is changed.  This should
   * be called only by HTMLEditRules::DocumentModifiedWorker() to call
   * HTMLEditRules::OnModifyDocument() with AutoEditActionDataSetter
   * instance.
   */
  MOZ_CAN_RUN_SCRIPT void OnModifyDocument();

 protected:  // Called by helper classes.
  virtual void OnStartToHandleTopLevelEditSubAction(
      EditSubAction aEditSubAction, nsIEditor::EDirection aDirection) override;
  virtual void OnEndHandlingTopLevelEditSubAction() override;

 protected:  // Shouldn't be used by friend classes
  virtual ~HTMLEditor();

  /**
   * InsertParagraphSeparatorAsSubAction() inserts a line break if it's
   * HTMLEditor and it's possible.
   */
  nsresult InsertParagraphSeparatorAsSubAction();

  virtual nsresult SelectAllInternal() override;

  /**
   * SelectContentInternal() sets Selection to aContentToSelect to
   * aContentToSelect + 1 in parent of aContentToSelect.
   *
   * @param aContentToSelect    The content which should be selected.
   */
  nsresult SelectContentInternal(nsIContent& aContentToSelect);

  /**
   * CollapseSelectionAfter() collapses Selection after aElement.
   * If aElement is an orphan node or not in editing host, returns error.
   */
  nsresult CollapseSelectionAfter(Element& aElement);

  /**
   * GetElementOrParentByTagNameAtSelection() looks for an element node whose
   * name matches aTagName from anchor node of Selection to <body> element.
   *
   * @param aTagName        The tag name which you want to look for.
   *                        Must not be nsGkAtoms::_empty.
   *                        If nsGkAtoms::list, the result may be <ul>, <ol> or
   *                        <dl> element.
   *                        If nsGkAtoms::td, the result may be <td> or <th>.
   *                        If nsGkAtoms::href, the result may be <a> element
   *                        which has "href" attribute with non-empty value.
   *                        If nsGkAtoms::anchor, the result may be <a> which
   *                        has "name" attribute with non-empty value.
   * @return                If an element which matches aTagName, returns
   *                        an Element.  Otherwise, nullptr.
   */
  Element* GetElementOrParentByTagNameAtSelection(const nsAtom& aTagName) const;

  /**
   * GetElementOrParentByTagNameInternal() looks for an element node whose
   * name matches aTagName from aNode to <body> element.
   *
   * @param aTagName        The tag name which you want to look for.
   *                        Must not be nsGkAtoms::_empty.
   *                        If nsGkAtoms::list, the result may be <ul>, <ol> or
   *                        <dl> element.
   *                        If nsGkAtoms::td, the result may be <td> or <th>.
   *                        If nsGkAtoms::href, the result may be <a> element
   *                        which has "href" attribute with non-empty value.
   *                        If nsGkAtoms::anchor, the result may be <a> which
   *                        has "name" attribute with non-empty value.
   * @param aNode           Start node to look for the element.
   * @return                If an element which matches aTagName, returns
   *                        an Element.  Otherwise, nullptr.
   */
  Element* GetElementOrParentByTagNameInternal(const nsAtom& aTagName,
                                               nsINode& aNode) const;

  /**
   * GetSelectedElement() returns a "selected" element node.  "selected" means:
   * - there is only one selection range
   * - the range starts from an element node or in an element
   * - the range ends at immediately after same element
   * - and the range does not include any other element nodes.
   * Additionally, only when aTagName is nsGkAtoms::href, this thinks that an
   * <a> element which has non-empty "href" attribute includes the range, the
   * <a> element is selected.
   *
   * NOTE: This method is implementation of nsIHTMLEditor.getSelectedElement()
   * and comm-central depends on this behavior.  Therefore, if you need to use
   * this method internally but you need to change, perhaps, you should create
   * another method for avoiding breakage of comm-central apps.
   *
   * @param aTagName    The atom of tag name in lower case.  Set this to
   *                    result  of GetLowerCaseNameAtom() if you have a tag
   *                    name with nsString.
   *                    If nullptr, this returns any element node or nullptr.
   *                    If nsGkAtoms::href, this returns an <a> element which
   *                    has non-empty "href" attribute or nullptr.
   *                    If nsGkAtoms::anchor, this returns an <a> element which
   *                    has non-empty "name" attribute or nullptr.
   *                    Otherwise, returns an element node whose name is
   *                    same as aTagName or nullptr.
   * @param aRv         Returns error code.
   * @return            A "selected" element.
   */
  already_AddRefed<Element> GetSelectedElement(const nsAtom* aTagName,
                                               ErrorResult& aRv);

  /**
   * GetFirstTableRowElement() returns the first <tr> element in the most
   * nearest ancestor of aTableOrElementInTable or itself.
   *
   * @param aTableOrElementInTable      <table> element or another element.
   *                                    If this is a <table> element, returns
   *                                    first <tr> element in it.  Otherwise,
   *                                    returns first <tr> element in nearest
   *                                    ancestor <table> element.
   * @param aRv                         Returns an error code.  When
   *                                    aTableOrElementInTable is neither
   *                                    <table> nor in a <table> element,
   *                                    returns NS_ERROR_FAILURE.
   *                                    However, if <table> does not have
   *                                    <tr> element, returns NS_OK.
   */
  Element* GetFirstTableRowElement(Element& aTableOrElementInTable,
                                   ErrorResult& aRv) const;

  /**
   * GetNextTableRowElement() returns next <tr> element of aTableRowElement.
   * This won't cross <table> element boundary but may cross table section
   * elements like <tbody>.
   *
   * @param aTableRowElement    A <tr> element.
   * @param aRv                 Returns error.  If given element is <tr> but
   *                            there is no next <tr> element, this returns
   *                            nullptr but does not return error.
   */
  Element* GetNextTableRowElement(Element& aTableRowElement,
                                  ErrorResult& aRv) const;

  struct CellAndIndexes;
  struct CellData;

  /**
   * CellIndexes store both row index and column index of a table cell.
   */
  struct MOZ_STACK_CLASS CellIndexes final {
    int32_t mRow;
    int32_t mColumn;

    /**
     * This constructor initializes mRowIndex and mColumnIndex with indexes of
     * aCellElement.
     *
     * @param aCellElement      An <td> or <th> element.
     * @param aRv               Returns error if layout information is not
     *                          available or given element is not a table cell.
     */
    CellIndexes(Element& aCellElement, ErrorResult& aRv)
        : mRow(-1), mColumn(-1) {
      MOZ_ASSERT(!aRv.Failed());
      Update(aCellElement, aRv);
    }

    /**
     * Update mRowIndex and mColumnIndex with indexes of aCellElement.
     *
     * @param                   See above.
     */
    void Update(Element& aCellElement, ErrorResult& aRv);

    /**
     * This constructor initializes mRowIndex and mColumnIndex with indexes of
     * cell element which contains anchor of Selection.
     *
     * @param aHTMLEditor       The editor which creates the instance.
     * @param aSelection        The Selection for the editor.
     * @param aRv               Returns error if there is no cell element
     *                          which contains anchor of Selection, or layout
     *                          information is not available.
     */
    CellIndexes(HTMLEditor& aHTMLEditor, Selection& aSelection,
                ErrorResult& aRv)
        : mRow(-1), mColumn(-1) {
      Update(aHTMLEditor, aSelection, aRv);
    }

    /**
     * Update mRowIndex and mColumnIndex with indexes of cell element which
     * contains anchor of Selection.
     *
     * @param                   See above.
     */
    void Update(HTMLEditor& aHTMLEditor, Selection& aSelection,
                ErrorResult& aRv);

    bool operator==(const CellIndexes& aOther) const {
      return mRow == aOther.mRow && mColumn == aOther.mColumn;
    }
    bool operator!=(const CellIndexes& aOther) const {
      return mRow != aOther.mRow || mColumn != aOther.mColumn;
    }

   private:
    CellIndexes() : mRow(-1), mColumn(-1) {}

    friend struct CellAndIndexes;
    friend struct CellData;
  };

  struct MOZ_STACK_CLASS CellAndIndexes final {
    RefPtr<Element> mElement;
    CellIndexes mIndexes;

    /**
     * This constructor initializes the members with cell element which is
     * selected by first range of the Selection.  Note that even if the
     * first range is in the cell element, this does not treat it as the
     * cell element is selected.
     */
    CellAndIndexes(HTMLEditor& aHTMLEditor, Selection& aSelection,
                   ErrorResult& aRv) {
      Update(aHTMLEditor, aSelection, aRv);
    }

    /**
     * Update mElement and mIndexes with cell element which is selected by
     * first range of the Selection.  Note that even if the first range is
     * in the cell element, this does not treat it as the cell element is
     * selected.
     */
    void Update(HTMLEditor& aHTMLEditor, Selection& aSelection,
                ErrorResult& aRv);
  };

  struct MOZ_STACK_CLASS CellData final {
    RefPtr<Element> mElement;
    // Current indexes which this is initialized with.
    CellIndexes mCurrent;
    // First column/row indexes of the cell.  When current position is spanned
    // from other column/row, this value becomes different from mCurrent.
    CellIndexes mFirst;
    // Computed rowspan/colspan values which are specified to the cell.
    // Note that if the cell has larger rowspan/colspan value than actual
    // table size, these values are the larger values.
    int32_t mRowSpan;
    int32_t mColSpan;
    // Effective rowspan/colspan value at the index.  For example, if first
    // cell element in first row has rowspan="3", then, if this is initialized
    // with 0-0 indexes, effective rowspan is 3.  However, if this is
    // initialized with 1-0 indexes, effective rowspan is 2.
    int32_t mEffectiveRowSpan;
    int32_t mEffectiveColSpan;
    // mIsSelected is set to true if mElement itself or its parent <tr> or
    // <table> is selected.  Otherwise, e.g., the cell just contains selection
    // range, this is set to false.
    bool mIsSelected;

    CellData()
        : mRowSpan(-1),
          mColSpan(-1),
          mEffectiveRowSpan(-1),
          mEffectiveColSpan(-1),
          mIsSelected(false) {}

    /**
     * Those constructors initializes the members with a <table> element and
     * both row and column index to specify a cell element.
     */
    CellData(HTMLEditor& aHTMLEditor, Element& aTableElement, int32_t aRowIndex,
             int32_t aColumnIndex, ErrorResult& aRv) {
      Update(aHTMLEditor, aTableElement, aRowIndex, aColumnIndex, aRv);
    }

    CellData(HTMLEditor& aHTMLEditor, Element& aTableElement,
             const CellIndexes& aIndexes, ErrorResult& aRv) {
      Update(aHTMLEditor, aTableElement, aIndexes, aRv);
    }

    /**
     * Those Update() methods updates the members with a <table> element and
     * both row and column index to specify a cell element.
     */
    void Update(HTMLEditor& aHTMLEditor, Element& aTableElement,
                int32_t aRowIndex, int32_t aColumnIndex, ErrorResult& aRv) {
      mCurrent.mRow = aRowIndex;
      mCurrent.mColumn = aColumnIndex;
      Update(aHTMLEditor, aTableElement, aRv);
    }

    void Update(HTMLEditor& aHTMLEditor, Element& aTableElement,
                const CellIndexes& aIndexes, ErrorResult& aRv) {
      mCurrent = aIndexes;
      Update(aHTMLEditor, aTableElement, aRv);
    }

    void Update(HTMLEditor& aHTMLEditor, Element& aTableElement,
                ErrorResult& aRv);

    /**
     * FailedOrNotFound() returns true if this failed to initialize/update
     * or succeeded but found no cell element.
     */
    bool FailedOrNotFound() const { return !mElement; }

    /**
     * IsSpannedFromOtherRowOrColumn(), IsSpannedFromOtherColumn and
     * IsSpannedFromOtherRow() return true if there is no cell element
     * at the index because of spanning from other row and/or column.
     */
    bool IsSpannedFromOtherRowOrColumn() const {
      return mElement && mCurrent != mFirst;
    }
    bool IsSpannedFromOtherColumn() const {
      return mElement && mCurrent.mColumn != mFirst.mColumn;
    }
    bool IsSpannedFromOtherRow() const {
      return mElement && mCurrent.mRow != mFirst.mRow;
    }

    /**
     * NextColumnIndex() and NextRowIndex() return column/row index of
     * next cell.  Note that this does not check whether there is next
     * cell or not actually.
     */
    int32_t NextColumnIndex() const {
      if (NS_WARN_IF(FailedOrNotFound())) {
        return -1;
      }
      return mCurrent.mColumn + mEffectiveColSpan;
    }
    int32_t NextRowIndex() const {
      if (NS_WARN_IF(FailedOrNotFound())) {
        return -1;
      }
      return mCurrent.mRow + mEffectiveRowSpan;
    }

    /**
     * LastColumnIndex() and LastRowIndex() return column/row index of
     * column/row which is spanned by the cell.
     */
    int32_t LastColumnIndex() const {
      if (NS_WARN_IF(FailedOrNotFound())) {
        return -1;
      }
      return NextColumnIndex() - 1;
    }
    int32_t LastRowIndex() const {
      if (NS_WARN_IF(FailedOrNotFound())) {
        return -1;
      }
      return NextRowIndex() - 1;
    }

    /**
     * NumberOfPrecedingColmuns() and NumberOfPrecedingRows() return number of
     * preceding columns/rows if current index is spanned from other column/row.
     * Otherwise, i.e., current point is not spanned form other column/row,
     * returns 0.
     */
    int32_t NumberOfPrecedingColmuns() const {
      if (NS_WARN_IF(FailedOrNotFound())) {
        return -1;
      }
      return mCurrent.mColumn - mFirst.mColumn;
    }
    int32_t NumberOfPrecedingRows() const {
      if (NS_WARN_IF(FailedOrNotFound())) {
        return -1;
      }
      return mCurrent.mRow - mFirst.mRow;
    }

    /**
     * NumberOfFollowingColumns() and NumberOfFollowingRows() return
     * number of remaining columns/rows if the cell spans to other
     * column/row.
     */
    int32_t NumberOfFollowingColumns() const {
      if (NS_WARN_IF(FailedOrNotFound())) {
        return -1;
      }
      return mEffectiveColSpan - 1;
    }
    int32_t NumberOfFollowingRows() const {
      if (NS_WARN_IF(FailedOrNotFound())) {
        return -1;
      }
      return mEffectiveRowSpan - 1;
    }
  };

  /**
   * TableSize stores and computes number of rows and columns of a <table>
   * element.
   */
  struct MOZ_STACK_CLASS TableSize final {
    int32_t mRowCount;
    int32_t mColumnCount;

    /**
     * @param aHTMLEditor               The editor which creates the instance.
     * @param aTableOrElementInTable    If a <table> element, computes number
     *                                  of rows and columns of it.
     *                                  If another element in a <table> element,
     *                                  computes number of rows and columns
     *                                  of nearest ancestor <table> element.
     *                                  Otherwise, i.e., non-<table> element
     *                                  not in <table>, returns error.
     * @param aRv                       Returns error if the element is not
     *                                  in <table> or layout information is
     *                                  not available.
     */
    TableSize(HTMLEditor& aHTMLEditor, Element& aTableOrElementInTable,
              ErrorResult& aRv)
        : mRowCount(-1), mColumnCount(-1) {
      MOZ_ASSERT(!aRv.Failed());
      Update(aHTMLEditor, aTableOrElementInTable, aRv);
    }

    /**
     * Update mRowCount and mColumnCount for aTableOrElementInTable.
     * See above for the detail.
     */
    void Update(HTMLEditor& aHTMLEditor, Element& aTableOrElementInTable,
                ErrorResult& aRv);

    bool IsEmpty() const { return !mRowCount || !mColumnCount; }
  };

  /**
   * GetTableCellElementAt() returns a <td> or <th> element of aTableElement
   * if there is a cell at the indexes.
   *
   * @param aTableElement       Must be a <table> element.
   * @param aCellIndexes        Indexes of cell which you want.
   *                            If rowspan and/or colspan is specified 2 or
   *                            larger, any indexes are allowed to retrieve
   *                            the cell in the area.
   * @return                    The cell element if there is in the <table>.
   *                            Returns nullptr without error if the indexes
   *                            are out of bounds.
   */
  Element* GetTableCellElementAt(Element& aTableElement,
                                 const CellIndexes& aCellIndexes) const {
    return GetTableCellElementAt(aTableElement, aCellIndexes.mRow,
                                 aCellIndexes.mColumn);
  }
  Element* GetTableCellElementAt(Element& aTableElement, int32_t aRowIndex,
                                 int32_t aColumnIndex) const;

  /**
   * GetSelectedOrParentTableElement() returns <td>, <th>, <tr> or <table>
   * element:
   *   #1 if the first selection range selects a cell, returns it.
   *   #2 if the first selection range does not select a cell and
   *      the selection anchor refers a <table>, returns it.
   *   #3 if the first selection range does not select a cell and
   *      the selection anchor refers a <tr>, returns it.
   *   #4 if the first selection range does not select a cell and
   *      the selection anchor refers a <td>, returns it.
   *   #5 otherwise, nearest ancestor <td> or <th> element of the
   *      selection anchor if there is.
   * In #1 and #4, *aIsCellSelected will be set to true (i.e,, when
   * a selection range selects a cell element).
   */
  already_AddRefed<Element> GetSelectedOrParentTableElement(
      ErrorResult& aRv, bool* aIsCellSelected = nullptr) const;

  /**
   * PasteInternal() pasts text with replacing selected content.
   * This tries to dispatch ePaste event first.  If its defaultPrevent() is
   * called, this does nothing but returns NS_OK.
   *
   * @param aClipboardType      nsIClipboard::kGlobalClipboard or
   *                            nsIClipboard::kSelectionClipboard.
   * @param aDispatchPasteEvent true if this should dispatch ePaste event
   *                            before pasting.  Otherwise, false.
   */
  nsresult PasteInternal(int32_t aClipboardType, bool aDispatchPasteEvent);

  /**
   * InsertAsCitedQuotationInternal() inserts a <blockquote> element whose
   * cite attribute is aCitation and whose content is aQuotedText.
   * Note that this shouldn't be called when IsPlaintextEditor() is true.
   *
   * @param aQuotedText     HTML source if aInsertHTML is true.  Otherwise,
   *                        plain text.  This is inserted into new <blockquote>
   *                        element.
   * @param aCitation       cite attribute value of new <blockquote> element.
   * @param aInsertHTML     true if aQuotedText should be treated as HTML
   *                        source.
   *                        false if aQuotedText should be treated as plain
   *                        text.
   * @param aNodeInserted   [OUT] The new <blockquote> element.
   */
  nsresult InsertAsCitedQuotationInternal(const nsAString& aQuotedText,
                                          const nsAString& aCitation,
                                          bool aInsertHTML,
                                          nsINode** aNodeInserted);

  /**
   * InsertNodeIntoProperAncestorWithTransaction() attempts to insert aNode
   * into the document, at aPointToInsert.  Checks with strict dtd to see if
   * containment is allowed.  If not allowed, will attempt to find a parent
   * in the parent hierarchy of aPointToInsert.GetContainer() that will accept
   * aNode as a child.  If such a parent is found, will split the document
   * tree from aPointToInsert up to parent, and then insert aNode.
   * aPointToInsert is then adjusted to point to the actual location that
   * aNode was inserted at.  aSplitAtEdges specifies if the splitting process
   * is allowed to result in empty nodes.
   *
   * @param aNode             Node to insert.
   * @param aPointToInsert    Insertion point.
   * @param aSplitAtEdges     Splitting can result in empty nodes?
   * @return                  Returns inserted point if succeeded.
   *                          Otherwise, the result is not set.
   */
  template <typename PT, typename CT>
  EditorDOMPoint InsertNodeIntoProperAncestorWithTransaction(
      nsIContent& aNode, const EditorDOMPointBase<PT, CT>& aPointToInsert,
      SplitAtEdges aSplitAtEdges);

  /**
   * InsertBrElementAtSelectionWithTransaction() inserts a new <br> element at
   * selection.  If there is non-collapsed selection ranges, the selected
   * ranges is deleted first.
   */
  nsresult InsertBrElementAtSelectionWithTransaction();

  /**
   * InsertTextWithQuotationsInternal() replaces selection with new content.
   * First, this method splits aStringToInsert to multiple chunks which start
   * with non-linebreaker except first chunk and end with a linebreaker except
   * last chunk.  Then, each chunk starting with ">" is inserted after wrapping
   * with <span _moz_quote="true">, and each chunk not starting with ">" is
   * inserted as normal text.
   */
  nsresult InsertTextWithQuotationsInternal(const nsAString& aStringToInsert);

  /**
   * IndentOrOutdentAsSubAction() indents or outdents the content around
   * Selection.  Callers have to guarantee that there is a placeholder
   * transaction.
   *
   * @param aEditSubAction      Must be EditSubAction::eIndent or
   *                            EditSubAction::eOutdent.
   */
  nsresult IndentOrOutdentAsSubAction(EditSubAction aEditSubAction);

  nsresult LoadHTML(const nsAString& aInputString);

  nsresult SetInlinePropertyInternal(nsAtom& aProperty, nsAtom* aAttribute,
                                     const nsAString& aValue);
  nsresult RemoveInlinePropertyInternal(nsAtom* aProperty, nsAtom* aAttribute);

  /**
   * ReplaceHeadContentsWithSourceWithTransaction() replaces all children of
   * <head> element with given source code.  This is undoable.
   *
   * @param aSourceToInsert     HTML source fragment to replace the children
   *                            of <head> element.
   */
  nsresult ReplaceHeadContentsWithSourceWithTransaction(
      const nsAString& aSourceToInsert);

  nsresult GetCSSBackgroundColorState(bool* aMixed, nsAString& aOutColor,
                                      bool aBlockLevel);
  nsresult GetHTMLBackgroundColorState(bool* aMixed, nsAString& outColor);

  nsresult GetLastCellInRow(nsINode* aRowNode, nsINode** aCellNode);

  static nsresult GetCellFromRange(nsRange* aRange, Element** aCell);

  /**
   * This sets background on the appropriate container element (table, cell,)
   * or calls into nsTextEditor to set the page background.
   */
  nsresult SetCSSBackgroundColorWithTransaction(const nsAString& aColor);
  nsresult SetHTMLBackgroundColorWithTransaction(const nsAString& aColor);

  virtual void InitializeSelectionAncestorLimit(
      nsIContent& aAncestorLimit) override;

  /**
   * Make the given selection span the entire document.
   */
  virtual nsresult SelectEntireDocument() override;

  /**
   * Use this to assure that selection is set after attribute nodes when
   * trying to collapse selection at begining of a block node
   * e.g., when setting at beginning of a table cell
   * This will stop at a table, however, since we don't want to
   * "drill down" into nested tables.
   */
  void CollapseSelectionToDeepestNonTableFirstChild(nsINode* aNode);

  /**
   * Returns TRUE if sheet was loaded, false if it wasn't.
   */
  bool EnableExistingStyleSheet(const nsAString& aURL);

  /**
   * GetStyleSheetForURL() returns a pointer to StyleSheet which was added
   * with AddOverrideStyleSheetInternal().  If it's not found, returns nullptr.
   *
   * @param aURL        URL to the style sheet.
   */
  StyleSheet* GetStyleSheetForURL(const nsAString& aURL);

  /**
   * Add a url + known style sheet to the internal lists.
   */
  nsresult AddNewStyleSheetToList(const nsAString& aURL,
                                  StyleSheet* aStyleSheet);

  /**
   * Removes style sheet from the internal lists.
   *
   * @param aURL        URL to the style sheet.
   * @return            If the URL is in the internal list, returns the
   *                    removed style sheet.  Otherwise, i.e., not found,
   *                    nullptr.
   */
  already_AddRefed<StyleSheet> RemoveStyleSheetFromList(const nsAString& aURL);

  /**
   * Add and apply the style sheet synchronously.
   *
   * @param aURL        URL to the style sheet.
   */
  nsresult AddOverrideStyleSheetInternal(const nsAString& aURL);

  /**
   * Remove the style sheet from this editor synchronously.
   *
   * @param aURL        URL to the style sheet.
   * @return            Even if there is no specified style sheet in the
   *                    internal lists, this returns NS_OK.
   */
  nsresult RemoveOverrideStyleSheetInternal(const nsAString& aURL);

  /**
   * Enable or disable the style sheet synchronously.
   * aURL is just a key to specify a style sheet in the internal array.
   * I.e., the style sheet has already been registered with
   * AddOverrideStyleSheetInternal().
   *
   * @param aURL        URL to the style sheet.
   * @param aEnable     true if enable the style sheet.  false if disable it.
   */
  void EnableStyleSheetInternal(const nsAString& aURL, bool aEnable);

  /**
   * MaybeCollapseSelectionAtFirstEditableNode() may collapse selection at
   * proper position to staring to edit.  If there is a non-editable node
   * before any editable text nodes or inline elements which can have text
   * nodes as their children, collapse selection at start of the editing
   * host.  If there is an editable text node which is not collapsed, collapses
   * selection at the start of the text node.  If there is an editable inline
   * element which cannot have text nodes as its child, collapses selection at
   * before the element node.  Otherwise, collapses selection at start of the
   * editing host.
   *
   * @param aIgnoreIfSelectionInEditingHost
   *                        This method does nothing if selection is in the
   *                        editing host except if it's collapsed at start of
   *                        the editing host.
   *                        Note that if selection ranges were outside of
   *                        current selection limiter, selection was collapsed
   *                        at the start of the editing host therefore, if
   *                        you call this with setting this to true, you can
   *                        keep selection ranges if user has already been
   *                        changed.
   */
  nsresult MaybeCollapseSelectionAtFirstEditableNode(
      bool aIgnoreIfSelectionInEditingHost);

  class BlobReader final {
    typedef EditorBase::AutoEditActionDataSetter AutoEditActionDataSetter;

   public:
    BlobReader(dom::BlobImpl* aBlob, HTMLEditor* aHTMLEditor, bool aIsSafe,
               nsIDocument* aSourceDoc, const EditorDOMPoint& aPointToInsert,
               bool aDoDeleteSelection);

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BlobReader)
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(BlobReader)

    nsresult OnResult(const nsACString& aResult);
    nsresult OnError(const nsAString& aErrorName);

   private:
    ~BlobReader() {}

    RefPtr<dom::BlobImpl> mBlob;
    RefPtr<HTMLEditor> mHTMLEditor;
    nsCOMPtr<nsIDocument> mSourceDoc;
    EditorDOMPoint mPointToInsert;
    EditAction mEditAction;
    bool mIsSafe;
    bool mDoDeleteSelection;
  };

  virtual nsresult InitRules() override;

  virtual void CreateEventListeners() override;
  virtual nsresult InstallEventListeners() override;
  virtual void RemoveEventListeners() override;

  bool ShouldReplaceRootElement();
  void NotifyRootChanged();
  Element* GetBodyElement();

  /**
   * Get the focused node of this editor.
   * @return    If the editor has focus, this returns the focused node.
   *            Otherwise, returns null.
   */
  already_AddRefed<nsINode> GetFocusedNode();

  virtual already_AddRefed<Element> GetInputEventTargetElement() override;

  /**
   * Return TRUE if aElement is a table-related elemet and caret was set.
   */
  bool SetCaretInTableCell(dom::Element* aElement);

  nsresult TabInTable(bool inIsShift, bool* outHandled);

  /**
   * InsertPosition is an enum to indicate where the method should insert to.
   */
  enum class InsertPosition {
    // Before selected cell or a cell containing first selection range.
    eBeforeSelectedCell,
    // After selected cell or a cell containing first selection range.
    eAfterSelectedCell,
  };

  /**
   * InsertTableCellsWithTransaction() inserts <td> elements before or after
   * a cell element containing first selection range.  I.e., if the cell
   * spans columns and aInsertPosition is eAfterSelectedCell, new columns
   * will be inserted after the right-most column which contains the cell.
   * Note that this simply inserts <td> elements, i.e., colspan and rowspan
   * around the cell containing selection are not modified.  So, for example,
   * adding a cell to rectangular table changes non-rectangular table.
   * And if the cell containing selection is at left of row-spanning cell,
   * it may be moved to right side of the row-spanning cell after inserting
   * some cell elements before it.  Similarly, colspan won't be adjusted
   * for keeping table rectangle.
   * If first selection range is not in table cell element, this does nothing
   * but does not return error.
   *
   * @param aNumberOfCellssToInsert     Number of cells to insert.
   * @param aInsertPosition             Before or after the target cell which
   *                                    contains first selection range.
   */
  nsresult InsertTableCellsWithTransaction(int32_t aNumberOfCellsToInsert,
                                           InsertPosition aInsertPosition);

  /**
   * InsertTableColumnsWithTransaction() inserts columns before or after
   * a cell element containing first selection range.  I.e., if the cell
   * spans columns and aInsertPosition is eAfterSelectedCell, new columns
   * will be inserted after the right-most row which contains the cell.
   * If first selection range is not in table cell element, this does nothing
   * but does not return error.
   *
   * @param aNumberOfColumnsToInsert    Number of columns to insert.
   * @param aInsertPosition             Before or after the target cell which
   *                                    contains first selection range.
   */
  nsresult InsertTableColumnsWithTransaction(int32_t aNumberOfColumnsToInsert,
                                             InsertPosition aInsertPosition);

  /**
   * InsertTableRowsWithTransaction() inserts <tr> elements before or after
   * a cell element containing first selection range.  I.e., if the cell
   * spans rows and aInsertPosition is eAfterSelectedCell, new rows will be
   * inserted after the most-bottom row which contains the cell.  If first
   * selection range is not in table cell element, this does nothing but
   * does not return error.
   *
   * @param aNumberOfRowsToInsert       Number of rows to insert.
   * @param aInsertPosition             Before or after the target cell which
   *                                    contains first selection range.
   */
  nsresult InsertTableRowsWithTransaction(int32_t aNumberOfRowsToInsert,
                                          InsertPosition aInsertPosition);

  /**
   * Insert a new cell after or before supplied aCell.
   * Optional: If aNewCell supplied, returns the newly-created cell (addref'd,
   * of course)
   * This doesn't change or use the current selection.
   */
  nsresult InsertCell(Element* aCell, int32_t aRowSpan, int32_t aColSpan,
                      bool aAfter, bool aIsHeader, Element** aNewCell);

  /**
   * DeleteSelectedTableColumnsWithTransaction() removes cell elements which
   * belong to same columns of selected cell elements.
   * If only one cell element is selected or first selection range is
   * in a cell, removes cell elements which belong to same column.
   * If 2 or more cell elements are selected, removes cell elements which
   * belong to any of all selected columns.  In this case,
   * aNumberOfColumnsToDelete is ignored.
   * If there is no selection ranges, returns error.
   * If selection is not in a cell element, this does not return error,
   * just does nothing.
   * WARNING: This does not remove <col> nor <colgroup> elements.
   *
   * @param aNumberOfColumnsToDelete    Number of columns to remove.  This is
   *                                    ignored if 2 ore more cells are
   *                                    selected.
   */
  nsresult DeleteSelectedTableColumnsWithTransaction(
      int32_t aNumberOfColumnsToDelete);

  /**
   * DeleteTableColumnWithTransaction() removes cell elements which belong
   * to the specified column.
   * This method adjusts colspan attribute value if cells spanning the
   * column to delete.
   * WARNING: This does not remove <col> nor <colgroup> elements.
   *
   * @param aTableElement       The <table> element which contains the
   *                            column which you want to remove.
   * @param aRowIndex           Index of the column which you want to remove.
   *                            0 is the first column.
   */
  nsresult DeleteTableColumnWithTransaction(Element& aTableElement,
                                            int32_t aColumnIndex);

  /**
   * DeleteSelectedTableRowsWithTransaction() removes <tr> elements.
   * If only one cell element is selected or first selection range is
   * in a cell, removes <tr> elements starting from a <tr> element
   * containing the selected cell or first selection range.
   * If 2 or more cell elements are selected, all <tr> elements
   * which contains selected cell(s).  In this case, aNumberOfRowsToDelete
   * is ignored.
   * If there is no selection ranges, returns error.
   * If selection is not in a cell element, this does not return error,
   * just does nothing.
   *
   * @param aNumberOfRowsToDelete   Number of rows to remove.  This is ignored
   *                                if 2 or more cells are selected.
   */
  nsresult DeleteSelectedTableRowsWithTransaction(
      int32_t aNumberOfRowsToDelete);

  /**
   * DeleteTableRowWithTransaction() removes a <tr> element whose index in
   * the <table> is aRowIndex.
   * This method adjusts rowspan attribute value if the <tr> element contains
   * cells which spans rows.
   *
   * @param aTableElement       The <table> element which contains the
   *                            <tr> element which you want to remove.
   * @param aRowIndex           Index of the <tr> element which you want to
   *                            remove.  0 is the first row.
   */
  nsresult DeleteTableRowWithTransaction(Element& aTableElement,
                                         int32_t aRowIndex);

  /**
   * DeleteTableCellWithTransaction() removes table cell elements.  If two or
   * more cell elements are selected, this removes all selected cell elements.
   * Otherwise, this removes some cell elements starting from selected cell
   * element or a cell containing first selection range.  When this removes
   * last cell element in <tr> or <table>, this removes the <tr> or the
   * <table> too.  Note that when removing a cell causes number of its row
   * becomes less than the others, this method does NOT fill the place with
   * rowspan nor colspan.  This does not return error even if selection is not
   * in cell element, just does nothing.
   *
   * @param aNumberOfCellsToDelete  Number of cells to remove.  This is ignored
   *                                if 2 or more cells are selected.
   */
  nsresult DeleteTableCellWithTransaction(int32_t aNumberOfCellsToDelete);

  /**
   * DeleteAllChildrenWithTransaction() removes all children of aElement from
   * the tree.
   *
   * @param aElement        The element whose children you want to remove.
   */
  nsresult DeleteAllChildrenWithTransaction(Element& aElement);

  /**
   * Move all contents from aCellToMerge into aTargetCell (append at end).
   */
  nsresult MergeCells(RefPtr<Element> aTargetCell, RefPtr<Element> aCellToMerge,
                      bool aDeleteCellToMerge);

  /**
   * DeleteTableElementAndChildren() removes aTableElement (and its children)
   * from the DOM tree with transaction.
   *
   * @param aTableElement   The <table> element which you want to remove.
   */
  nsresult DeleteTableElementAndChildrenWithTransaction(Element& aTableElement);

  nsresult SetColSpan(Element* aCell, int32_t aColSpan);
  nsresult SetRowSpan(Element* aCell, int32_t aRowSpan);

  /**
   * Helper used to get nsTableWrapperFrame for a table.
   */
  static nsTableWrapperFrame* GetTableFrame(Element* aTable);

  /**
   * GetNumberOfCellsInRow() returns number of actual cell elements in the row.
   * If some cells appear by "rowspan" in other rows, they are ignored.
   *
   * @param aTableElement   The <table> element.
   * @param aRowIndex       Valid row index in aTableElement.  This method
   *                        counts cell elements in the row.
   * @return                -1 if this meets unexpected error.
   *                        Otherwise, number of cells which this method found.
   */
  int32_t GetNumberOfCellsInRow(Element& aTableElement, int32_t aRowIndex);

  /**
   * Test if all cells in row or column at given index are selected.
   */
  bool AllCellsInRowSelected(Element* aTable, int32_t aRowIndex,
                             int32_t aNumberOfColumns);
  bool AllCellsInColumnSelected(Element* aTable, int32_t aColIndex,
                                int32_t aNumberOfRows);

  bool IsEmptyCell(Element* aCell);

  /**
   * Most insert methods need to get the same basic context data.
   * Any of the pointers may be null if you don't need that datum (for more
   * efficiency).
   * Input: *aCell is a known cell,
   *        if null, cell is obtained from the anchor node of the selection.
   * Returns NS_EDITOR_ELEMENT_NOT_FOUND if cell is not found even if aCell is
   * null.
   */
  nsresult GetCellContext(Element** aTable, Element** aCell,
                          nsINode** aCellParent, int32_t* aCellOffset,
                          int32_t* aRowIndex, int32_t* aColIndex);

  nsresult GetCellSpansAt(Element* aTable, int32_t aRowIndex, int32_t aColIndex,
                          int32_t& aActualRowSpan, int32_t& aActualColSpan);

  nsresult SplitCellIntoColumns(Element* aTable, int32_t aRowIndex,
                                int32_t aColIndex, int32_t aColSpanLeft,
                                int32_t aColSpanRight, Element** aNewCell);

  nsresult SplitCellIntoRows(Element* aTable, int32_t aRowIndex,
                             int32_t aColIndex, int32_t aRowSpanAbove,
                             int32_t aRowSpanBelow, Element** aNewCell);

  nsresult CopyCellBackgroundColor(Element* aDestCell, Element* aSourceCell);

  /**
   * Reduce rowspan/colspan when cells span into nonexistent rows/columns.
   */
  nsresult FixBadRowSpan(Element* aTable, int32_t aRowIndex,
                         int32_t& aNewRowCount);
  nsresult FixBadColSpan(Element* aTable, int32_t aColIndex,
                         int32_t& aNewColCount);

  /**
   * XXX NormalizeTableInternal() is broken.  If it meets a cell which has
   *     bigger or smaller rowspan or colspan than actual number of cells,
   *     this always failed to scan the table.  Therefore, this does nothing
   *     when the table should be normalized.
   *
   * @param aTableOrElementInTable  An element which is in a <table> element
   *                                or <table> element itself.  Otherwise,
   *                                this returns NS_OK but does nothing.
   */
  nsresult NormalizeTableInternal(Element& aTableOrElementInTable);

  /**
   * Fallback method: Call this after using ClearSelection() and you
   * failed to set selection to some other content in the document.
   */
  nsresult SetSelectionAtDocumentStart();

  static Element* GetEnclosingTable(nsINode* aNode);

  // Methods for handling plaintext quotations
  nsresult PasteAsPlaintextQuotation(int32_t aSelectionType);

  /**
   * Insert a string as quoted text, replacing the selected text (if any).
   * @param aQuotedText     The string to insert.
   * @param aAddCites       Whether to prepend extra ">" to each line
   *                        (usually true, unless those characters
   *                        have already been added.)
   * @return aNodeInserted  The node spanning the insertion, if applicable.
   *                        If aAddCites is false, this will be null.
   */
  nsresult InsertAsPlaintextQuotation(const nsAString& aQuotedText,
                                      bool aAddCites, nsINode** aNodeInserted);

  /**
   * InsertObject() inserts given object at aPointToInsert.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult InsertObject(const nsACString& aType, nsISupports* aObject,
                        bool aIsSafe, nsIDocument* aSourceDoc,
                        const EditorDOMPoint& aPointToInsert,
                        bool aDoDeleteSelection);

  // factored methods for handling insertion of data from transferables
  // (drag&drop or clipboard)
  virtual nsresult PrepareTransferable(nsITransferable** transferable) override;
  nsresult PrepareHTMLTransferable(nsITransferable** transferable);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult InsertFromTransferable(nsITransferable* transferable,
                                  nsIDocument* aSourceDoc,
                                  const nsAString& aContextStr,
                                  const nsAString& aInfoStr,
                                  bool havePrivateHTMLFlavor,
                                  bool aDoDeleteSelection);

  /**
   * InsertFromDataTransfer() is called only when user drops data into
   * this editor.  Don't use this method for other purposes.
   */
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult InsertFromDataTransfer(dom::DataTransfer* aDataTransfer,
                                          int32_t aIndex,
                                          nsIDocument* aSourceDoc,
                                          const EditorDOMPoint& aDroppedAt,
                                          bool aDoDeleteSelection) override;

  bool HavePrivateHTMLFlavor(nsIClipboard* clipboard);
  nsresult ParseCFHTML(nsCString& aCfhtml, char16_t** aStuffToPaste,
                       char16_t** aCfcontext);

  nsresult StripFormattingNodes(nsIContent& aNode, bool aOnlyList = false);
  nsresult CreateDOMFragmentFromPaste(
      const nsAString& aInputString, const nsAString& aContextStr,
      const nsAString& aInfoStr, nsCOMPtr<nsINode>* outFragNode,
      nsCOMPtr<nsINode>* outStartNode, nsCOMPtr<nsINode>* outEndNode,
      int32_t* outStartOffset, int32_t* outEndOffset, bool aTrustedInput);
  nsresult ParseFragment(const nsAString& aStr, nsAtom* aContextLocalName,
                         nsIDocument* aTargetDoc,
                         dom::DocumentFragment** aFragment, bool aTrustedInput);
  void CreateListOfNodesToPaste(dom::DocumentFragment& aFragment,
                                nsTArray<OwningNonNull<nsINode>>& outNodeList,
                                nsINode* aStartContainer, int32_t aStartOffset,
                                nsINode* aEndContainer, int32_t aEndOffset);
  enum class StartOrEnd { start, end };
  void GetListAndTableParents(StartOrEnd aStartOrEnd,
                              nsTArray<OwningNonNull<nsINode>>& aNodeList,
                              nsTArray<OwningNonNull<Element>>& outArray);
  int32_t DiscoverPartialListsAndTables(
      nsTArray<OwningNonNull<nsINode>>& aPasteNodes,
      nsTArray<OwningNonNull<Element>>& aListsAndTables);
  nsINode* ScanForListAndTableStructure(
      StartOrEnd aStartOrEnd, nsTArray<OwningNonNull<nsINode>>& aNodes,
      Element& aListOrTable);
  void ReplaceOrphanedStructure(
      StartOrEnd aStartOrEnd, nsTArray<OwningNonNull<nsINode>>& aNodeArray,
      nsTArray<OwningNonNull<Element>>& aListAndTableArray,
      int32_t aHighWaterMark);

  /**
   * GetBetterInsertionPointFor() returns better insertion point to insert
   * aNodeToInsert.
   *
   * @param aNodeToInsert       The node to insert.
   * @param aPointToInsert      A candidate point to insert the node.
   * @return                    Better insertion point if next visible node
   *                            is a <br> element and previous visible node
   *                            is neither none, another <br> element nor
   *                            different block level element.
   */
  EditorRawDOMPoint GetBetterInsertionPointFor(
      nsINode& aNodeToInsert, const EditorRawDOMPoint& aPointToInsert);

  /**
   * MakeDefinitionListItemWithTransaction() replaces parent list of current
   * selection with <dl> or create new <dl> element and creates a definition
   * list item whose name is aTagName.
   *
   * @param aTagName            Must be nsGkAtoms::dt or nsGkAtoms::dd.
   */
  nsresult MakeDefinitionListItemWithTransaction(nsAtom& aTagName);

  /**
   * InsertBasicBlockWithTransaction() inserts a block element whose name
   * is aTagName at selection.
   *
   * @param aTagName            A block level element name.  Must NOT be
   *                            nsGkAtoms::dt nor nsGkAtoms::dd.
   */
  nsresult InsertBasicBlockWithTransaction(nsAtom& aTagName);

  /**
   * Increase/decrease the font size of selection.
   */
  nsresult RelativeFontChange(FontSize aDir);

  nsresult RelativeFontChangeOnNode(int32_t aSizeChange, nsIContent* aNode);
  nsresult RelativeFontChangeHelper(int32_t aSizeChange, nsINode* aNode);

  /**
   * Helper routines for inline style.
   */
  nsresult SetInlinePropertyOnTextNode(Text& aData, int32_t aStartOffset,
                                       int32_t aEndOffset, nsAtom& aProperty,
                                       nsAtom* aAttribute,
                                       const nsAString& aValue);

  nsresult PromoteInlineRange(nsRange& aRange);
  nsresult PromoteRangeIfStartsOrEndsInNamedAnchor(nsRange& aRange);
  nsresult SplitStyleAboveRange(nsRange* aRange, nsAtom* aProperty,
                                nsAtom* aAttribute);
  nsresult RemoveStyleInside(nsIContent& aNode, nsAtom* aProperty,
                             nsAtom* aAttribute,
                             const bool aChildrenOnly = false);

  bool NodeIsProperty(nsINode& aNode);
  bool IsAtFrontOfNode(nsINode& aNode, int32_t aOffset);
  bool IsAtEndOfNode(nsINode& aNode, int32_t aOffset);
  bool IsOnlyAttribute(const Element* aElement, nsAtom* aAttribute);

  bool HasStyleOrIdOrClass(Element* aElement);
  nsresult RemoveElementIfNoStyleOrIdOrClass(Element& aElement);

  /**
   * Whether the outer window of the DOM event target has focus or not.
   */
  bool OurWindowHasFocus();

  /**
   * This function is used to insert a string of HTML input optionally with some
   * context information into the editable field.  The HTML input either comes
   * from a transferable object created as part of a drop/paste operation, or
   * from the InsertHTML method.  We may want the HTML input to be sanitized
   * (for example, if it's coming from a transferable object), in which case
   * aTrustedInput should be set to false, otherwise, the caller should set it
   * to true, which means that the HTML will be inserted in the DOM verbatim.
   *
   * aClearStyle should be set to false if you want the paste to be affected by
   * local style (e.g., for the insertHTML command).
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult DoInsertHTMLWithContext(
      const nsAString& aInputString, const nsAString& aContextStr,
      const nsAString& aInfoStr, const nsAString& aFlavor,
      nsIDocument* aSourceDoc, const EditorDOMPoint& aPointToInsert,
      bool aDeleteSelection, bool aTrustedInput, bool aClearStyle = true);

  /**
   * sets the position of an element; warning it does NOT check if the
   * element is already positioned or not and that's on purpose.
   * @param aElement [IN] the element
   * @param aX       [IN] the x position in pixels.
   * @param aY       [IN] the y position in pixels.
   */
  void SetTopAndLeft(Element& aElement, int32_t aX, int32_t aY);

  /**
   * Reset a selected cell or collapsed selection (the caret) after table
   * editing.
   *
   * @param aTable      A table in the document.
   * @param aRow        The row ...
   * @param aCol        ... and column defining the cell where we will try to
   *                    place the caret.
   * @param aSelected   If true, we select the whole cell instead of setting
   *                    caret.
   * @param aDirection  If cell at (aCol, aRow) is not found, search for
   *                    previous cell in the same column (aPreviousColumn) or
   *                    row (ePreviousRow) or don't search for another cell
   *                    (aNoSearch).  If no cell is found, caret is place just
   *                    before table; and if that fails, at beginning of
   *                    document.  Thus we generally don't worry about the
   *                    return value and can use the
   *                    AutoSelectionSetterAfterTableEdit stack-based object to
   *                    insure we reset the caret in a table-editing method.
   */
  void SetSelectionAfterTableEdit(Element* aTable, int32_t aRow, int32_t aCol,
                                  int32_t aDirection, bool aSelected);

  void RemoveListenerAndDeleteRef(const nsAString& aEvent,
                                  nsIDOMEventListener* aListener,
                                  bool aUseCapture, ManualNACPtr aElement,
                                  nsIPresShell* aShell);
  void DeleteRefToAnonymousNode(ManualNACPtr aContent, nsIPresShell* aShell);

  /**
   * RefereshEditingUI() may refresh editing UIs for current Selection, focus,
   * etc.  If this shows or hides some UIs, it causes reflow.  So, this is
   * not safe method.
   */
  nsresult RefereshEditingUI();

  /**
   * Returns the offset of an element's frame to its absolute containing block.
   */
  nsresult GetElementOrigin(Element& aElement, int32_t& aX, int32_t& aY);
  nsresult GetPositionAndDimensions(Element& aElement, int32_t& aX, int32_t& aY,
                                    int32_t& aW, int32_t& aH,
                                    int32_t& aBorderLeft, int32_t& aBorderTop,
                                    int32_t& aMarginLeft, int32_t& aMarginTop);

  bool IsInObservedSubtree(nsIContent* aChild);

  void UpdateRootElement();

  /**
   * SetAllResizersPosition() moves all resizers to proper position.
   * If the resizers are hidden or replaced with another set of resizers
   * while this is running, this returns error.  So, callers shouldn't
   * keep handling the resizers if this returns error.
   */
  nsresult SetAllResizersPosition();

  /**
   * Shows active resizers around an element's frame
   * @param aResizedElement [IN] a DOM Element
   */
  nsresult ShowResizersInternal(Element& aResizedElement);

  /**
   * Hide resizers if they are visible.  If this is called while there is no
   * visible resizers, this does not return error, but does nothing.
   */
  nsresult HideResizersInternal();

  /**
   * RefreshResizersInternal() moves resizers to proper position.  This does
   * nothing if there is no resizing target.
   */
  nsresult RefreshResizersInternal();

  ManualNACPtr CreateResizer(int16_t aLocation, nsIContent& aParentContent);
  void SetAnonymousElementPosition(int32_t aX, int32_t aY, Element* aResizer);

  ManualNACPtr CreateShadow(nsIContent& aParentContent,
                            Element& aOriginalObject);

  /**
   * SetShadowPosition() moves the shadow element to proper position.
   *
   * @param aShadowElement      Must be mResizingShadow or mPositioningShadow.
   * @param aElement            The element which has the shadow.
   * @param aElementX           Left of aElement.
   * @param aElementY           Top of aElement.
   */
  nsresult SetShadowPosition(Element& aShadowElement, Element& aElement,
                             int32_t aElementLeft, int32_t aElementTop);

  ManualNACPtr CreateResizingInfo(nsIContent& aParentContent);
  nsresult SetResizingInfoPosition(int32_t aX, int32_t aY, int32_t aW,
                                   int32_t aH);

  enum class ResizeAt {
    eX,
    eY,
    eWidth,
    eHeight,
  };
  int32_t GetNewResizingIncrement(int32_t aX, int32_t aY, ResizeAt aResizeAt);

  nsresult StartResizing(Element* aHandle);
  int32_t GetNewResizingX(int32_t aX, int32_t aY);
  int32_t GetNewResizingY(int32_t aX, int32_t aY);
  int32_t GetNewResizingWidth(int32_t aX, int32_t aY);
  int32_t GetNewResizingHeight(int32_t aX, int32_t aY);
  void HideShadowAndInfo();
  void SetFinalSize(int32_t aX, int32_t aY);
  void SetResizeIncrements(int32_t aX, int32_t aY, int32_t aW, int32_t aH,
                           bool aPreserveRatio);

  /**
   * HideAnonymousEditingUIs() forcibly hides all editing UIs (resizers,
   * inline-table-editing UI, absolute positioning UI).
   */
  void HideAnonymousEditingUIs();

  /**
   * HideAnonymousEditingUIsIfUnnecessary() hides all editing UIs if some of
   * visible UIs are now unnecessary.
   */
  void HideAnonymousEditingUIsIfUnnecessary();

  /**
   * sets the z-index of an element.
   * @param aElement [IN] the element
   * @param aZorder  [IN] the z-index
   */
  void SetZIndex(Element& aElement, int32_t aZorder);

  /**
   * shows a grabber attached to an arbitrary element. The grabber is an image
   * positioned on the left hand side of the top border of the element. Draggin
   * and dropping it allows to change the element's absolute position in the
   * document. See chrome://editor/content/images/grabber.gif
   * @param aElement [IN] the element
   */
  nsresult ShowGrabberInternal(Element& aElement);

  /**
   * Setting grabber to proper position for current mAbsolutelyPositionedObject.
   * For example, while an element has grabber, the element may be resized
   * or repositioned by script or something.  Then, you need to reset grabber
   * position with this.
   */
  nsresult RefreshGrabberInternal();

  /**
   * hide the grabber if it shown.
   */
  void HideGrabberInternal();

  /**
   * CreateGrabberInternal() creates a grabber for moving aParentContent.
   * This sets mGrabber to the new grabber.  If this returns true, it's
   * always non-nullptr.  Otherwise, i.e., the grabber is hidden during
   * creation, this returns false.
   */
  bool CreateGrabberInternal(nsIContent& aParentContent);

  nsresult StartMoving();
  nsresult SetFinalPosition(int32_t aX, int32_t aY);
  void AddPositioningOffset(int32_t& aX, int32_t& aY);
  void SnapToGrid(int32_t& newX, int32_t& newY);
  nsresult GrabberClicked();
  nsresult EndMoving();
  nsresult GetTemporaryStyleForFocusedPositionedElement(Element& aElement,
                                                        nsAString& aReturn);

  /**
   * Shows inline table editing UI around a <table> element which contains
   * aCellElement.  This returns error if creating UI is hidden during this,
   * or detects another set of UI during this.  In such case, callers
   * shouldn't keep handling anything for the UI.
   *
   * @param aCellElement    Must be an <td> or <th> element.
   */
  nsresult ShowInlineTableEditingUIInternal(Element& aCellElement);

  /**
   * Hide all inline table editing UI.
   */
  void HideInlineTableEditingUIInternal();

  /**
   * RefreshInlineTableEditingUIInternal() moves inline table editing UI to
   * proper position.  This returns error if the UI is hidden or replaced
   * during moving.
   */
  nsresult RefreshInlineTableEditingUIInternal();

  /**
   * IsEmptyTextNode() returns true if aNode is a text node and does not have
   * any visible characters.
   */
  bool IsEmptyTextNode(nsINode& aNode);

  bool IsSimpleModifiableNode(nsIContent* aContent, nsAtom* aProperty,
                              nsAtom* aAttribute, const nsAString* aValue);
  nsresult SetInlinePropertyOnNodeImpl(nsIContent& aNode, nsAtom& aProperty,
                                       nsAtom* aAttribute,
                                       const nsAString& aValue);
  typedef enum { eInserted, eAppended } InsertedOrAppended;
  void DoContentInserted(nsIContent* aChild, InsertedOrAppended);

  /**
   * Returns an anonymous Element of type aTag,
   * child of aParentContent. If aIsCreatedHidden is true, the class
   * "hidden" is added to the created element. If aAnonClass is not
   * the empty string, it becomes the value of the attribute "_moz_anonclass"
   * @return a Element
   * @param aTag             [IN] desired type of the element to create
   * @param aParentContent   [IN] the parent node of the created anonymous
   *                              element
   * @param aAnonClass       [IN] contents of the _moz_anonclass attribute
   * @param aIsCreatedHidden [IN] a boolean specifying if the class "hidden"
   *                              is to be added to the created anonymous
   *                              element
   */
  ManualNACPtr CreateAnonymousElement(nsAtom* aTag, nsIContent& aParentContent,
                                      const nsAString& aAnonClass,
                                      bool aIsCreatedHidden);

  /**
   * Reads a blob into memory and notifies the BlobReader object when the read
   * operation is finished.
   *
   * @param aBlob       The input blob
   * @param aWindow     The global object under which the read should happen.
   * @param aBlobReader The blob reader object to be notified when finished.
   */
  static nsresult SlurpBlob(dom::Blob* aBlob, nsPIDOMWindowOuter* aWindow,
                            BlobReader* aBlobReader);

 protected:
  RefPtr<TypeInState> mTypeInState;
  RefPtr<ComposerCommandsUpdater> mComposerCommandsUpdater;

  bool mCRInParagraphCreatesParagraph;

  bool mCSSAware;
  UniquePtr<CSSEditUtils> mCSSEditUtils;

  // mSelectedCellIndex is reset by GetFirstSelectedTableCellElement(),
  // then, it'll be referred and incremented by
  // GetNextSelectedTableCellElement().
  mutable uint32_t mSelectedCellIndex;

  nsString mLastStyleSheetURL;
  nsString mLastOverrideStyleSheetURL;

  // Maintain a list of associated style sheets and their urls.
  nsTArray<nsString> mStyleSheetURLs;
  nsTArray<RefPtr<StyleSheet>> mStyleSheets;

  // resizing
  // If the instance has shown resizers at least once, mHasShownResizers is
  // set to true.
  bool mHasShownResizers;
  bool mIsObjectResizingEnabled;
  bool mIsResizing;
  bool mPreserveRatio;
  bool mResizedObjectIsAnImage;

  // absolute positioning
  bool mIsAbsolutelyPositioningEnabled;
  bool mResizedObjectIsAbsolutelyPositioned;
  // If the instance has shown grabber at least once, mHasShownGrabber is
  // set to true.
  bool mHasShownGrabber;
  bool mGrabberClicked;
  bool mIsMoving;

  bool mSnapToGridEnabled;

  // inline table editing
  // If the instance has shown inline table editor at least once,
  // mHasShownInlineTableEditor is set to true.
  bool mHasShownInlineTableEditor;
  bool mIsInlineTableEditingEnabled;

  // resizing
  ManualNACPtr mTopLeftHandle;
  ManualNACPtr mTopHandle;
  ManualNACPtr mTopRightHandle;
  ManualNACPtr mLeftHandle;
  ManualNACPtr mRightHandle;
  ManualNACPtr mBottomLeftHandle;
  ManualNACPtr mBottomHandle;
  ManualNACPtr mBottomRightHandle;

  RefPtr<Element> mActivatedHandle;

  ManualNACPtr mResizingShadow;
  ManualNACPtr mResizingInfo;

  RefPtr<Element> mResizedObject;

  int32_t mOriginalX;
  int32_t mOriginalY;

  int32_t mResizedObjectX;
  int32_t mResizedObjectY;
  int32_t mResizedObjectWidth;
  int32_t mResizedObjectHeight;

  int32_t mResizedObjectMarginLeft;
  int32_t mResizedObjectMarginTop;
  int32_t mResizedObjectBorderLeft;
  int32_t mResizedObjectBorderTop;

  int32_t mXIncrementFactor;
  int32_t mYIncrementFactor;
  int32_t mWidthIncrementFactor;
  int32_t mHeightIncrementFactor;

  // When resizers, grabber and/or inline table editor are operated by user
  // actually, the following counters are increased.
  uint32_t mResizerUsedCount;
  uint32_t mGrabberUsedCount;
  uint32_t mInlineTableEditorUsedCount;

  int8_t mInfoXIncrement;
  int8_t mInfoYIncrement;

  // absolute positioning
  int32_t mPositionedObjectX;
  int32_t mPositionedObjectY;
  int32_t mPositionedObjectWidth;
  int32_t mPositionedObjectHeight;

  int32_t mPositionedObjectMarginLeft;
  int32_t mPositionedObjectMarginTop;
  int32_t mPositionedObjectBorderLeft;
  int32_t mPositionedObjectBorderTop;

  RefPtr<Element> mAbsolutelyPositionedObject;
  ManualNACPtr mGrabber;
  ManualNACPtr mPositioningShadow;

  int32_t mGridSize;

  // inline table editing
  RefPtr<Element> mInlineEditedCell;

  ManualNACPtr mAddColumnBeforeButton;
  ManualNACPtr mRemoveColumnButton;
  ManualNACPtr mAddColumnAfterButton;

  ManualNACPtr mAddRowBeforeButton;
  ManualNACPtr mRemoveRowButton;
  ManualNACPtr mAddRowAfterButton;

  void AddMouseClickListener(Element* aElement);
  void RemoveMouseClickListener(Element* aElement);

  nsCOMPtr<nsILinkHandler> mLinkHandler;

  ParagraphSeparator mDefaultParagraphSeparator;

  friend class AutoSelectionSetterAfterTableEdit;
  friend class AutoSetTemporaryAncestorLimiter;
  friend class CSSEditUtils;
  friend class EditorBase;
  friend class EmptyEditableFunctor;
  friend class HTMLEditRules;
  friend class SlurpBlobEventListener;
  friend class TextEditor;
  friend class WSRunObject;
};

}  // namespace mozilla

mozilla::HTMLEditor* nsIEditor::AsHTMLEditor() {
  return static_cast<mozilla::EditorBase*>(this)->mIsHTMLEditorClass
             ? static_cast<mozilla::HTMLEditor*>(this)
             : nullptr;
}

const mozilla::HTMLEditor* nsIEditor::AsHTMLEditor() const {
  return static_cast<const mozilla::EditorBase*>(this)->mIsHTMLEditorClass
             ? static_cast<const mozilla::HTMLEditor*>(this)
             : nullptr;
}

#endif  // #ifndef mozilla_HTMLEditor_h
