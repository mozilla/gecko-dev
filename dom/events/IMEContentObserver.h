/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IMEContentObserver_h
#define mozilla_IMEContentObserver_h

#include "mozilla/Attributes.h"
#include "mozilla/EditorBase.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h"  // XXX Why does only this need to be included here?
#include "nsIReflowObserver.h"
#include "nsIScrollObserver.h"
#include "nsIWidget.h"
#include "nsStubDocumentObserver.h"
#include "nsStubMutationObserver.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

class nsIContent;
class nsINode;
class nsPresContext;

namespace mozilla {

class EventStateManager;
class TextComposition;

namespace dom {
class Selection;
}  // namespace dom

// IMEContentObserver notifies widget of any text and selection changes
// in the currently focused editor
class IMEContentObserver final : public nsStubMutationObserver,
                                 public nsIReflowObserver,
                                 public nsIScrollObserver,
                                 public nsSupportsWeakReference {
 public:
  using SelectionChangeData = widget::IMENotification::SelectionChangeData;
  using TextChangeData = widget::IMENotification::TextChangeData;
  using TextChangeDataBase = widget::IMENotification::TextChangeDataBase;
  using IMENotificationRequests = widget::IMENotificationRequests;
  using IMEMessage = widget::IMEMessage;

  IMEContentObserver();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(IMEContentObserver,
                                           nsIReflowObserver)
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIREFLOWOBSERVER

  // nsIScrollObserver
  virtual void ScrollPositionChanged() override;

  /**
   * OnSelectionChange() is called when selection is changed in the editor.
   */
  void OnSelectionChange(dom::Selection& aSelection);

  MOZ_CAN_RUN_SCRIPT bool OnMouseButtonEvent(nsPresContext& aPresContext,
                                             WidgetMouseEvent& aMouseEvent);

  MOZ_CAN_RUN_SCRIPT nsresult
  HandleQueryContentEvent(WidgetQueryContentEvent* aEvent);

  /**
   * Handle eSetSelection event if and only if aEvent changes selection offset
   * or length.  Doing nothing when selection range is same is important to
   * honer users' intention or web app's intention because ContentEventHandler
   * does not support to put range boundaries to arbitrary side of element
   * boundaries.  E.g., `<b>bold[]</b> normal` vs. `<b>bold</b>[] normal`.
   * Note that this compares given range with selection cache which has been
   * notified IME via widget.  Therefore, the caller needs to guarantee that
   * pending notifications should've been flushed.  If you test this, you need
   * to wait 2 animation frames before sending eSetSelection event.
   */
  MOZ_CAN_RUN_SCRIPT nsresult MaybeHandleSelectionEvent(
      nsPresContext* aPresContext, WidgetSelectionEvent* aEvent);

  /**
   * Init() initializes the instance, i.e., retrieving necessary objects and
   * starts to observe something.
   * Be aware, callers of this method need to guarantee that the instance
   * won't be released during calling this.
   *
   * @param aWidget         The widget which can access native IME.
   * @param aPresContext    The PresContext which has aContent.
   * @param aElement        An editable element or nullptr if this will observe
   *                        design mode document.
   * @param aEditorBase     The editor which is associated with aContent.
   */
  MOZ_CAN_RUN_SCRIPT void Init(nsIWidget& aWidget, nsPresContext& aPresContext,
                               dom::Element* aElement, EditorBase& aEditorBase);

  /**
   * Destroy() finalizes the instance, i.e., stops observing contents and
   * clearing the members.
   * Be aware, callers of this method need to guarantee that the instance
   * won't be released during calling this.
   */
  void Destroy();

  /**
   * Returns false if the instance refers some objects and observing them.
   * Otherwise, true.
   */
  bool Destroyed() const;

  /**
   * IMEContentObserver is stored by EventStateManager during observing.
   * DisconnectFromEventStateManager() is called when EventStateManager stops
   * storing the instance.
   */
  void DisconnectFromEventStateManager();

  /**
   * MaybeReinitialize() tries to restart to observe the editor's root node.
   * This is useful when the editor is reframed and all children are replaced
   * with new node instances.
   * Be aware, callers of this method need to guarantee that the instance
   * won't be released during calling this.
   *
   * @return            Returns true if the instance is managing the content.
   *                    Otherwise, false.
   */
  MOZ_CAN_RUN_SCRIPT bool MaybeReinitialize(nsIWidget& aWidget,
                                            nsPresContext& aPresContext,
                                            dom::Element* aElement,
                                            EditorBase& aEditorBase);

  /**
   * Return true if this is observing editable content and aElement has focus.
   * If aElement is a text control, check if this is observing its anonymous
   * subtree.  Otherwise, check if this is observing the children of aElement in
   * the DOM tree.  If aElement is nullptr, this returns true if entire the
   * document is editable, e.g., in the designMode.
   */
  [[nodiscard]] bool IsObserving(const nsPresContext& aPresContext,
                                 const dom::Element* aElement) const;

  [[nodiscard]] bool IsBeingInitializedFor(const nsPresContext& aPresContext,
                                           const dom::Element* aElement,
                                           const EditorBase& aEditorBase) const;
  bool IsObserving(const TextComposition& aTextComposition) const;
  bool WasInitializedWith(const EditorBase& aEditorBase) const {
    return mEditorBase == &aEditorBase;
  }
  bool IsEditorHandlingEventForComposition() const;
  bool KeepAliveDuringDeactive() const {
    return mIMENotificationRequests &&
           mIMENotificationRequests->WantDuringDeactive();
  }
  [[nodiscard]] bool EditorIsTextEditor() const {
    return mEditorBase && mEditorBase->IsTextEditor();
  }
  nsIWidget* GetWidget() const { return mWidget; }
  void SuppressNotifyingIME();
  void UnsuppressNotifyingIME();
  nsPresContext* GetPresContext() const;
  nsresult GetSelectionAndRoot(dom::Selection** aSelection,
                               dom::Element** aRootElement) const;

  /**
   * TryToFlushPendingNotifications() should be called when pending events
   * should be flushed.  This tries to run the queued IMENotificationSender.
   * Doesn't do anything in child processes where flushing happens
   * asynchronously unless aAllowAsync is false.
   */
  void TryToFlushPendingNotifications(bool aAllowAsync);

  /**
   * MaybeNotifyCompositionEventHandled() posts composition event handled
   * notification into the pseudo queue.
   */
  void MaybeNotifyCompositionEventHandled();

  /**
   * Following methods are called when the editor:
   *   - an edit action handled.
   *   - before handling an edit action.
   *   - canceled handling an edit action after calling BeforeEditAction().
   */
  void OnEditActionHandled();
  void BeforeEditAction();
  void CancelEditAction();

  /**
   * Called when text control value is changed while this is not observing
   * mRootElement.  This is typically there is no frame for the editor (i.e.,
   * no proper anonymous <div> element for the editor yet) or the TextEditor
   * has not been created (i.e., IMEStateManager has not been reinitialized
   * this instance with new anonymous <div> element yet).
   */
  void OnTextControlValueChangedWhileNotObservable(const nsAString& aNewValue);

  dom::Element* GetObservingElement() const {
    return mIsObserving ? mRootElement.get() : nullptr;
  }

 private:
  ~IMEContentObserver() = default;

  enum State {
    eState_NotObserving,
    eState_Initializing,
    eState_StoppedObserving,
    eState_Observing
  };
  State GetState() const;
  MOZ_CAN_RUN_SCRIPT bool InitWithEditor(nsPresContext& aPresContext,
                                         dom::Element* aElement,
                                         EditorBase& aEditorBase);
  void OnIMEReceivedFocus();
  void Clear();
  [[nodiscard]] bool IsObservingContent(const nsPresContext& aPresContext,
                                        const dom::Element* aElement) const;
  [[nodiscard]] bool IsReflowLocked() const;
  [[nodiscard]] bool IsSafeToNotifyIME() const;
  [[nodiscard]] bool IsEditorComposing() const;

  // Following methods are called by DocumentObserver when
  // beginning to update the contents and ending updating the contents.
  void BeginDocumentUpdate();
  void EndDocumentUpdate();

  // Following methods manages added nodes during a document change.

  /**
   * MaybeNotifyIMEOfAddedTextDuringDocumentChange() may send text change
   * notification caused by the nodes added between mFirstAddedContent in
   * mFirstAddedContainer and mLastAddedContent in
   * mLastAddedContainer and forgets the range.
   */
  void MaybeNotifyIMEOfAddedTextDuringDocumentChange();

  /**
   * IsInDocumentChange() returns true while the DOM tree is being modified
   * with mozAutoDocUpdate.  E.g., it's being modified by setting innerHTML or
   * insertAdjacentHTML().  This returns false when user types something in
   * the focused editor editor.
   */
  bool IsInDocumentChange() const {
    return mDocumentObserver && mDocumentObserver->IsUpdating();
  }

  /**
   * Forget the range of added nodes during a document change.
   */
  void ClearAddedNodesDuringDocumentChange();

  /**
   * HasAddedNodesDuringDocumentChange() returns true when this stores range
   * of nodes which were added into the DOM tree during a document change but
   * have not been sent to IME.  Note that this should always return false when
   * IsInDocumentChange() returns false.
   */
  bool HasAddedNodesDuringDocumentChange() const {
    return mFirstAddedContainer && mLastAddedContainer;
  }

  /**
   * Returns true if the passed-in node in aParent is the next node of
   * mLastAddedContent in pre-order tree traversal of the DOM.
   */
  bool IsNextNodeOfLastAddedNode(nsINode* aParent, nsIContent* aChild) const;

  void PostFocusSetNotification();
  void MaybeNotifyIMEOfFocusSet();
  void PostTextChangeNotification();
  void MaybeNotifyIMEOfTextChange(const TextChangeDataBase& aTextChangeData);
  void CancelNotifyingIMEOfTextChange();
  void PostSelectionChangeNotification();
  void MaybeNotifyIMEOfSelectionChange(bool aCausedByComposition,
                                       bool aCausedBySelectionEvent,
                                       bool aOccurredDuringComposition);
  void PostPositionChangeNotification();
  void MaybeNotifyIMEOfPositionChange();
  void CancelNotifyingIMEOfPositionChange();
  void PostCompositionEventHandledNotification();

  void NotifyContentAdded(nsINode* aContainer, nsIContent* aFirstContent,
                          nsIContent* aLastContent);
  void ObserveEditableNode();
  /**
   *  NotifyIMEOfBlur() notifies IME of blur.
   */
  void NotifyIMEOfBlur();
  /**
   *  UnregisterObservers() unregisters all listeners and observers.
   */
  void UnregisterObservers();
  void FlushMergeableNotifications();
  bool NeedsTextChangeNotification() const {
    return mIMENotificationRequests &&
           mIMENotificationRequests->WantTextChange();
  }
  bool NeedsPositionChangeNotification() const {
    return mIMENotificationRequests &&
           mIMENotificationRequests->WantPositionChanged();
  }
  void ClearPendingNotifications() {
    mNeedsToNotifyIMEOfFocusSet = false;
    mNeedsToNotifyIMEOfTextChange = false;
    mNeedsToNotifyIMEOfSelectionChange = false;
    mNeedsToNotifyIMEOfPositionChange = false;
    mNeedsToNotifyIMEOfCompositionEventHandled = false;
    mTextChangeData.Clear();
  }
  bool NeedsToNotifyIMEOfSomething() const {
    return mNeedsToNotifyIMEOfFocusSet || mNeedsToNotifyIMEOfTextChange ||
           mNeedsToNotifyIMEOfSelectionChange ||
           mNeedsToNotifyIMEOfPositionChange ||
           mNeedsToNotifyIMEOfCompositionEventHandled;
  }

  /**
   * UpdateSelectionCache() updates mSelectionData with the latest selection.
   * This should be called only when IsSafeToNotifyIME() returns true.
   */
  MOZ_CAN_RUN_SCRIPT bool UpdateSelectionCache(bool aRequireFlush = true);

  nsCOMPtr<nsIWidget> mWidget;
  // mFocusedWidget has the editor observed by the instance.  E.g., if the
  // focused editor is in XUL panel, this should be the widget of the panel.
  // On the other hand, mWidget is its parent which handles IME.
  nsCOMPtr<nsIWidget> mFocusedWidget;
  RefPtr<dom::Selection> mSelection;
  RefPtr<dom::Element> mRootElement;
  nsCOMPtr<nsINode> mEditableNode;
  nsCOMPtr<nsIDocShell> mDocShell;
  RefPtr<EditorBase> mEditorBase;

  /**
   * Helper classes to notify IME.
   */

  class AChangeEvent : public Runnable {
   protected:
    enum ChangeEventType {
      eChangeEventType_Focus,
      eChangeEventType_Selection,
      eChangeEventType_Text,
      eChangeEventType_Position,
      eChangeEventType_CompositionEventHandled
    };

    explicit AChangeEvent(const char* aName,
                          IMEContentObserver* aIMEContentObserver)
        : Runnable(aName),
          mIMEContentObserver(do_GetWeakReference(
              static_cast<nsIReflowObserver*>(aIMEContentObserver))) {
      MOZ_ASSERT(aIMEContentObserver);
    }

    already_AddRefed<IMEContentObserver> GetObserver() const {
      nsCOMPtr<nsIReflowObserver> observer =
          do_QueryReferent(mIMEContentObserver);
      return observer.forget().downcast<IMEContentObserver>();
    }

    nsWeakPtr mIMEContentObserver;

    /**
     * CanNotifyIME() checks if mIMEContentObserver can and should notify IME.
     */
    bool CanNotifyIME(ChangeEventType aChangeEventType) const;

    /**
     * IsSafeToNotifyIME() checks if it's safe to noitify IME.
     */
    bool IsSafeToNotifyIME(ChangeEventType aChangeEventType) const;
  };

  class IMENotificationSender : public AChangeEvent {
   public:
    explicit IMENotificationSender(IMEContentObserver* aIMEContentObserver)
        : AChangeEvent("IMENotificationSender", aIMEContentObserver),
          mIsRunning(false) {}
    MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override;

    void Dispatch(nsIDocShell* aDocShell);

   private:
    MOZ_CAN_RUN_SCRIPT void SendFocusSet();
    MOZ_CAN_RUN_SCRIPT void SendSelectionChange();
    void SendTextChange();
    void SendPositionChange();
    void SendCompositionEventHandled();

    bool mIsRunning;
  };

  // mQueuedSender is, it was put into the event queue but not run yet.
  RefPtr<IMENotificationSender> mQueuedSender;

  /**
   * IMEContentObserver is a mutation observer of mRootContent.  However,
   * it needs to know the beginning of content changes and end of it too for
   * reducing redundant computation of text offset with ContentEventHandler.
   * Therefore, it needs helper class to listen only them since if
   * both mutations were observed by IMEContentObserver directly, each
   * methods need to check if the changing node is in mRootContent but it's
   * too expensive.
   */
  class DocumentObserver final : public nsStubDocumentObserver {
   public:
    DocumentObserver() = delete;
    explicit DocumentObserver(IMEContentObserver& aIMEContentObserver)
        : mIMEContentObserver(&aIMEContentObserver), mDocumentUpdating(0) {
      SetEnabledCallbacks(nsIMutationObserver::kBeginUpdate |
                          nsIMutationObserver::kEndUpdate);
    }

    NS_DECL_CYCLE_COLLECTION_CLASS(DocumentObserver)
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE
    NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE

    void Observe(dom::Document*);
    void StopObserving();
    void Destroy();

    bool Destroyed() const { return !mIMEContentObserver; }
    bool IsObserving() const { return mDocument != nullptr; }
    bool IsUpdating() const { return mDocumentUpdating != 0; }

   private:
    virtual ~DocumentObserver() { Destroy(); }

    RefPtr<IMEContentObserver> mIMEContentObserver;
    RefPtr<dom::Document> mDocument;
    uint32_t mDocumentUpdating;
  };
  RefPtr<DocumentObserver> mDocumentObserver;

  /**
   * FlatTextCache stores length of flattened text starting from start of
   * the observing node (typically editing host or the anonymous <div> of
   * TextEditor) to:
   * - end of mContent if it's set (IsCachingToEndOfContent() returns true)
   * - before first content of mContainerNode if mContent is not set
   * (IsCachingToStartOfContainer() returns true).  In this case, the text
   * length includes a line break length which is caused by the open tag of
   * mContainerNode if and only if it's an element node and the open tag causes
   * a line break.
   */
  struct FlatTextCache {
   public:
    void Clear() {
      mContainerNode = nullptr;
      mContent = nullptr;
      mFlatTextLength = 0;
    }

    /**
     * Return true if mFlatTextLength caches flattened text length starting from
     * start of the observing node to the end of mContent.
     */
    [[nodiscard]] bool IsCachingToEndOfContent() const {
      return mContainerNode && mContent;
    }

    /**
     * Return true if mFlatTextLength caches flattened text length starting from
     * start of the observing node to the start of mContainerNode.  Note that if
     * mContainerNode is an element and whose open tag causes a line break,
     * mFlatTextLength includes the line break length too.
     */
    [[nodiscard]] bool IsCachingToStartOfContainer() const {
      return mContainerNode && !mContent;
    }

    /**
     * Compute flattened text length starting from first content of aRootElement
     * and ending at end of aContent.
     *
     * @param aContent          This will be set to mContent which points the
     *                          last child content node which participates in
     *                          the computed mFlatTextLength.
     * @param aRootElement      The root element of the editor, i.e., editing
     *                          host or the anonymous <div> in a text control.
     *                          (This is required to suppress
     *                          ContentEventHandler to generate a line break
     *                          caused by open tag of the editable root element
     *                          due to not editable.  Therefore, we need to call
     *                          ContentEventHandler methods with this.)
     */
    [[nodiscard]] nsresult ComputeAndCacheFlatTextLengthBeforeEndOfContent(
        const nsIContent& aContent, const dom::Element* aRootElement);

    void CacheFlatTextLengthBeforeEndOfContent(
        const nsIContent& aContent, uint32_t aFlatTextLength,
        const dom::Element* aRootElement) {
      mContainerNode = aContent.GetParentNode();
      mContent = const_cast<nsIContent*>(&aContent);
      mFlatTextLength = aFlatTextLength;
      MOZ_ASSERT(IsCachingToEndOfContent());
      AssertValidCache(aRootElement);
    }

    /**
     * Compute flattened text length starting from first content of aRootElement
     * and ending at start of the first content of aContainer.
     *
     * @param aContainer        This will be set to mContainer and mContent will
     *                          be set to nullptr.
     * @param aRootElement      The root element of the editor, i.e., editing
     *                          host or the anonymous <div> in a text control.
     *                          (This is required to suppress
     *                          ContentEventHandler to generate a line break
     *                          caused by open tag of the editable root element
     *                          due to not editable.  Therefore, we need to call
     *                          ContentEventHandler methods with this.)
     */
    [[nodiscard]] nsresult ComputeAndCacheFlatTextLengthBeforeFirstContent(
        const nsINode& aContainer, const dom::Element* aRootElement);

    void CacheFlatTextLengthBeforeFirstContent(
        const nsINode& aContainer, uint32_t aFlatTextLength,
        const dom::Element* aRootElement) {
      mContainerNode = const_cast<nsINode*>(&aContainer);
      mContent = nullptr;
      mFlatTextLength = aFlatTextLength;
      MOZ_ASSERT(IsCachingToStartOfContainer());
      AssertValidCache(aRootElement);
    }

    /**
     * Return flattened text length of aRemovingContent.  This is designed
     * only for the case when aRemovingContent is being removed from the tree.
     *
     * @param aRemovingContent  The content node which is being removed.
     * @param aRootElement      The root element of the editor, i.e., editing
     *                          host or the anonymous <div> in a text control.
     *                          For avoiding to generate a redundant line break
     *                          at open tag of this element, this is required
     *                          to call methods of ContentEventHandler.
     */
    [[nodiscard]] static Result<uint32_t, nsresult>
    ComputeTextLengthOfRemovingContent(const nsIContent& aRemovingContent,
                                       const dom::Element* aRootElement);

    /**
     * Return flattened text length of starting from first content of
     * aRootElement and ending at before aContent (if ContentEventHandler
     * generates a line break at open tag of aContent, the result does not
     * contain the line break length).
     *
     * @param aContent          The content node which is immediately after a
     *                          content which you want to compute the flattened
     *                          text length before end of it.
     * @param aRootElement      The root element of the editor, i.e., editing
     *                          host or the anonymous <div> in a text control.
     *                          For avoiding to generate a redundant line break
     *                          at open tag of this element, this is required
     *                          to call methods of ContentEventHandler.
     */
    [[nodiscard]] static Result<uint32_t, nsresult>
    ComputeTextLengthBeforeContent(const nsIContent& aContent,
                                   const dom::Element* aRootElement);

    /**
     * Return flattened text length of starting from start of aStartContent and
     * ending at end of aEndContent.  If ContentEventHandler generates a line
     * break at open tag of aStartContent, the result includes the line break
     * length.
     *
     * @param aStartContent     The first content node of consecutive nodes
     *                          which you want to compute flattened text length
     *                          starting from.
     * @param aEndContent       The last content node of consecutive nodes
     *                          which you want to compute flattened text length
     *                          ending at.
     * @param aRootElement      The root element of the editor, i.e., editing
     *                          host or the anonymous <div> in a text control.
     *                          For avoiding to generate a redundant line break
     *                          at open tag of this element, this is required
     *                          to call methods of ContentEventHandler.
     */
    [[nodiscard]] static Result<uint32_t, nsresult>
    ComputeTextLengthStartOfContentToEndOfContent(
        const nsIContent& aStartContent, const nsIContent& aEndContent,
        const dom::Element* aRootElement);

    /**
     * Return flattened text length starting from first content of aRootElement
     * and ending at start of the first content of aContainer.  So, if
     * ContentEventHandler generates a line break at the open tag of aContainer,
     * the result includes the line break length.
     *
     * @param aContainer        The container node which you want to compute the
     *                          flattened text length before the first content
     *                          of.
     * @param aRootElement      The root element of the editor, i.e., editing
     *                          host or the anonymous <div> in a text control.
     *                          For avoiding to generate a redundant line break
     *                          at open tag of this element, this is required
     *                          to call methods of ContentEventHandler.
     */
    [[nodiscard]] static Result<uint32_t, nsresult>
    ComputeTextLengthBeforeFirstContentOf(const nsINode& aContainer,
                                          const dom::Element* aRootElement);

    [[nodiscard]] bool CachesTextLengthBeforeContent(
        const nsIContent& aContent) const {
      MOZ_ASSERT(!aContent.IsBeingRemoved());
      return CachesTextLengthBeforeContent(aContent,
                                           aContent.GetPreviousSibling());
    }
    [[nodiscard]] bool CachesTextLengthBeforeContent(
        const nsIContent& aContent, const nsIContent* aPreviousSibling) const {
      MOZ_ASSERT_IF(!aContent.IsBeingRemoved(),
                    aContent.GetPreviousSibling() == aPreviousSibling);
      if (!mContainerNode || mContainerNode != aContent.GetParentNode()) {
        return false;
      }
      if (IsCachingToStartOfContainer()) {
        MOZ_ASSERT(!mContent);
        return !aPreviousSibling;
      }
      MOZ_ASSERT(mContent);
      return mContainerNode == aContent.GetParentNode() &&
             mContent == aPreviousSibling;
    }

    /**
     * This works only in the debug build and
     * test.ime_content_observer.assert_valid_cache pref is enabled.  This
     * checks with expensive computation, therefore, the pref is enabled only
     * when running automated tests for editors.
     */
    void AssertValidCache(const dom::Element* aRootElement) const;

    // mContainerNode is parent node of mContent when it's cached.
    nsCOMPtr<nsINode> mContainerNode;
    // mContent points to the last child which participates in the current
    // mFlatTextLength.  If this is nullptr, mFlatTextLength means that it
    // length before the first content of mContainerNode, i.e., including the
    // line break of that caused by the open tag of mContainerNode.
    nsCOMPtr<nsIContent> mContent;
    // Length of flat text generated from contents between the start of the
    // observing node (typically editing host or the anonymous <div> of
    // TextEditor) and the end of mContent.
    uint32_t mFlatTextLength = 0;
  };
  // mEndOfAddedTextCache caches text length from the start of the observing
  // node to the end of the last added content only while an edit action is
  // being handled by the editor and no other mutation (e.g., removing node)
  // occur.
  FlatTextCache mEndOfAddedTextCache;
  // mStartOfRemovingTextRangeCache caches text length from the start of the
  // observing node to the start of the last removed content only while an edit
  // action is being handled by the editor and no other mutation (e.g., adding
  // node) occur.  In other words, this caches text length before end of
  // mContent or before first child of mContainerNode.
  FlatTextCache mStartOfRemovingTextRangeCache;

  // mFirstAddedContainer is parent node of first added node in current
  // document change.  So, this is not nullptr only when a node was added
  // during a document change and the change has not been included into
  // mTextChangeData yet.
  // Note that this shouldn't be in cycle collection since this is not nullptr
  // only during a document change.
  nsCOMPtr<nsINode> mFirstAddedContainer;
  // mLastAddedContainer is parent node of last added node in current
  // document change.  So, this is not nullptr only when a node was added
  // during a document change and the change has not been included into
  // mTextChangeData yet.
  // Note that this shouldn't be in cycle collection since this is not nullptr
  // only during a document change.
  nsCOMPtr<nsINode> mLastAddedContainer;

  // mFirstAddedContent is the first node added in mFirstAddedContainer.
  nsCOMPtr<nsIContent> mFirstAddedContent;
  // mLastAddedContent is the last node added in mLastAddedContainer;
  nsCOMPtr<nsIContent> mLastAddedContent;

  TextChangeData mTextChangeData;

  // mSelectionData is the last selection data which was notified.  The
  // selection information is modified by UpdateSelectionCache().  The reason
  // of the selection change is modified by MaybeNotifyIMEOfSelectionChange().
  SelectionChangeData mSelectionData;

  EventStateManager* mESM = nullptr;

  const IMENotificationRequests* mIMENotificationRequests = nullptr;
  int64_t mPreCharacterDataChangeLength = -1;
  uint32_t mSuppressNotifications = 0;

  // If the observing editor is a text control's one, this is set to the value
  // length.
  uint32_t mTextControlValueLength = 0;

  // mSendingNotification is a notification which is now sending from
  // IMENotificationSender.  When the value is NOTIFY_IME_OF_NOTHING, it's
  // not sending any notification.
  IMEMessage mSendingNotification = widget::NOTIFY_IME_OF_NOTHING;

  bool mIsObserving = false;
  bool mIsTextControl = false;
  bool mIMEHasFocus = false;
  bool mNeedsToNotifyIMEOfFocusSet = false;
  bool mNeedsToNotifyIMEOfTextChange = false;
  bool mNeedsToNotifyIMEOfSelectionChange = false;
  bool mNeedsToNotifyIMEOfPositionChange = false;
  bool mNeedsToNotifyIMEOfCompositionEventHandled = false;
  // mIsHandlingQueryContentEvent is true when IMEContentObserver is handling
  // WidgetQueryContentEvent with ContentEventHandler.
  bool mIsHandlingQueryContentEvent = false;
};

}  // namespace mozilla

#endif  // mozilla_IMEContentObserver_h
