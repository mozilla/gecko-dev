/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IMEContentObserver_h_
#define mozilla_IMEContentObserver_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h" // XXX Why does only this need to be included here?
#include "nsIEditor.h"
#include "nsIEditorObserver.h"
#include "nsIReflowObserver.h"
#include "nsISelectionListener.h"
#include "nsIScrollObserver.h"
#include "nsIWidget.h" // for nsIMEUpdatePreference
#include "nsStubMutationObserver.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

class nsIContent;
class nsINode;
class nsISelection;
class nsPresContext;

namespace mozilla {

class EventStateManager;

// IMEContentObserver notifies widget of any text and selection changes
// in the currently focused editor
class IMEContentObserver final : public nsISelectionListener
                               , public nsStubMutationObserver
                               , public nsIReflowObserver
                               , public nsIScrollObserver
                               , public nsSupportsWeakReference
                               , public nsIEditorObserver
{
public:
  IMEContentObserver();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(IMEContentObserver,
                                           nsISelectionListener)
  NS_DECL_NSIEDITOROBSERVER
  NS_DECL_NSISELECTIONLISTENER
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIREFLOWOBSERVER

  // nsIScrollObserver
  virtual void ScrollPositionChanged() override;

  bool OnMouseButtonEvent(nsPresContext* aPresContext,
                          WidgetMouseEvent* aMouseEvent);

  void Init(nsIWidget* aWidget, nsPresContext* aPresContext,
            nsIContent* aContent, nsIEditor* aEditor);
  void Destroy();
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
   * @return            Returns true if the instance is managing the content.
   *                    Otherwise, false.
   */
  bool MaybeReinitialize(nsIWidget* aWidget,
                         nsPresContext* aPresContext,
                         nsIContent* aContent,
                         nsIEditor* aEditor);
  bool IsManaging(nsPresContext* aPresContext, nsIContent* aContent);
  bool IsEditorHandlingEventForComposition() const;
  bool KeepAliveDuringDeactive() const
  {
    return mUpdatePreference.WantDuringDeactive();
  }
  nsIWidget* GetWidget() const { return mWidget; }
  nsIEditor* GetEditor() const { return mEditor; }
  void SuppressNotifyingIME() { mSuppressNotifications++; }
  void UnsuppressNotifyingIME()
  {
    if (!mSuppressNotifications || --mSuppressNotifications) {
      return;
    }
    FlushMergeableNotifications();
  }
  nsPresContext* GetPresContext() const;
  nsresult GetSelectionAndRoot(nsISelection** aSelection,
                               nsIContent** aRoot) const;

  struct TextChangeData
  {
    // mStartOffset is the start offset of modified or removed text in
    // original content and inserted text in new content.
    uint32_t mStartOffset;
    // mRemovalEndOffset is the end offset of modified or removed text in
    // original content.  If the value is same as mStartOffset, no text hasn't
    // been removed yet.
    uint32_t mRemovedEndOffset;
    // mAddedEndOffset is the end offset of inserted text or same as
    // mStartOffset if just removed.  The vlaue is offset in the new content.
    uint32_t mAddedEndOffset;

    bool mCausedOnlyByComposition;
    bool mStored;

    TextChangeData()
      : mStartOffset(0)
      , mRemovedEndOffset(0)
      , mAddedEndOffset(0)
      , mCausedOnlyByComposition(false)
      , mStored(false)
    {
    }

    TextChangeData(uint32_t aStartOffset,
                   uint32_t aRemovedEndOffset,
                   uint32_t aAddedEndOffset,
                   bool aCausedByComposition)
      : mStartOffset(aStartOffset)
      , mRemovedEndOffset(aRemovedEndOffset)
      , mAddedEndOffset(aAddedEndOffset)
      , mCausedOnlyByComposition(aCausedByComposition)
      , mStored(true)
    {
      MOZ_ASSERT(aRemovedEndOffset >= aStartOffset,
                 "removed end offset must not be smaller than start offset");
      MOZ_ASSERT(aAddedEndOffset >= aStartOffset,
                 "added end offset must not be smaller than start offset");
    }
    // Positive if text is added. Negative if text is removed.
    int64_t Difference() const 
    {
      return mAddedEndOffset - mRemovedEndOffset;
    }
  };

private:
  ~IMEContentObserver() {}

  enum State {
    eState_NotObserving,
    eState_Initializing,
    eState_StoppedObserving,
    eState_Observing
  };
  State GetState() const;
  bool IsObservingContent(nsPresContext* aPresContext,
                          nsIContent* aContent) const;
  bool IsReflowLocked() const;
  bool IsSafeToNotifyIME() const;

  void PostFocusSetNotification();
  void MaybeNotifyIMEOfFocusSet()
  {
    PostFocusSetNotification();
    FlushMergeableNotifications();
  }
  void PostTextChangeNotification(const TextChangeData& aTextChangeData);
  void MaybeNotifyIMEOfTextChange(const TextChangeData& aTextChangeData)
  {
    PostTextChangeNotification(aTextChangeData);
    FlushMergeableNotifications();
  }
  void PostSelectionChangeNotification(bool aCausedByComposition);
  void MaybeNotifyIMEOfSelectionChange(bool aCausedByComposition)
  {
    PostSelectionChangeNotification(aCausedByComposition);
    FlushMergeableNotifications();
  }
  void PostPositionChangeNotification();
  void MaybeNotifyIMEOfPositionChange()
  {
    PostPositionChangeNotification();
    FlushMergeableNotifications();
  }

  void NotifyContentAdded(nsINode* aContainer, int32_t aStart, int32_t aEnd);
  void ObserveEditableNode();
  /**
   *  NotifyIMEOfBlur() notifies IME of blur.
   */
  void NotifyIMEOfBlur();
  /**
   *  UnregisterObservers() unregisters all listeners and observers.
   */
  void UnregisterObservers();
  void StoreTextChangeData(const TextChangeData& aTextChangeData);
  void FlushMergeableNotifications();
  void ClearPendingNotifications()
  {
    mIsFocusEventPending = false;
    mIsSelectionChangeEventPending = false;
    mIsPositionChangeEventPending = false;
    mTextChangeData.mStored = false;
  }

#ifdef DEBUG
  void TestMergingTextChangeData();
#endif

  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsISelection> mSelection;
  nsCOMPtr<nsIContent> mRootContent;
  nsCOMPtr<nsINode> mEditableNode;
  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIEditor> mEditor;

  /**
   * FlatTextCache stores flat text length from start of the content to
   * mNodeOffset of mContainerNode.
   */
  struct FlatTextCache
  {
    // mContainerNode and mNodeOffset represent a point in DOM tree.  E.g.,
    // if mContainerNode is a div element, mNodeOffset is index of its child.
    nsCOMPtr<nsINode> mContainerNode;
    int32_t mNodeOffset;
    // Length of flat text generated from contents between the start of content
    // and a child node whose index is mNodeOffset of mContainerNode.
    uint32_t mFlatTextLength;

    FlatTextCache()
      : mNodeOffset(0)
      , mFlatTextLength(0)
    {
    }

    void Clear()
    {
      mContainerNode = nullptr;
      mNodeOffset = 0;
      mFlatTextLength = 0;
    }

    void Cache(nsINode* aContainer, int32_t aNodeOffset,
               uint32_t aFlatTextLength)
    {
      MOZ_ASSERT(aContainer, "aContainer must not be null");
      MOZ_ASSERT(
        aNodeOffset <= static_cast<int32_t>(aContainer->GetChildCount()),
        "aNodeOffset must be same as or less than the count of children");
      mContainerNode = aContainer;
      mNodeOffset = aNodeOffset;
      mFlatTextLength = aFlatTextLength;
    }

    bool Match(nsINode* aContainer, int32_t aNodeOffset) const
    {
      return aContainer == mContainerNode && aNodeOffset == mNodeOffset;
    }
  };
  // mEndOfAddedTextCache caches text length from the start of content to
  // the end of the last added content only while an edit action is being
  // handled by the editor and no other mutation (e.g., removing node)
  // occur.
  FlatTextCache mEndOfAddedTextCache;
  // mStartOfRemovingTextRangeCache caches text length from the start of content
  // to the start of the last removed content only while an edit action is being
  // handled by the editor and no other mutation (e.g., adding node) occur.
  FlatTextCache mStartOfRemovingTextRangeCache;

  TextChangeData mTextChangeData;

  EventStateManager* mESM;

  nsIMEUpdatePreference mUpdatePreference;
  uint32_t mPreAttrChangeLength;
  uint32_t mSuppressNotifications;
  int64_t mPreCharacterDataChangeLength;

  bool mIsObserving;
  bool mIMEHasFocus;
  bool mIsFocusEventPending;
  bool mIsSelectionChangeEventPending;
  bool mSelectionChangeCausedOnlyByComposition;
  bool mIsPositionChangeEventPending;
  bool mIsFlushingPendingNotifications;


  /**
   * Helper classes to notify IME.
   */

  class AChangeEvent: public nsRunnable
  {
  protected:
    enum ChangeEventType
    {
      eChangeEventType_Focus,
      eChangeEventType_Selection,
      eChangeEventType_Text,
      eChangeEventType_Position,
      eChangeEventType_FlushPendingEvents
    };

    AChangeEvent(ChangeEventType aChangeEventType,
                 IMEContentObserver* aIMEContentObserver)
      : mIMEContentObserver(aIMEContentObserver)
      , mChangeEventType(aChangeEventType)
    {
      MOZ_ASSERT(mIMEContentObserver);
    }

    nsRefPtr<IMEContentObserver> mIMEContentObserver;
    ChangeEventType mChangeEventType;

    /**
     * CanNotifyIME() checks if mIMEContentObserver can and should notify IME.
     */
    bool CanNotifyIME() const;

    /**
     * IsSafeToNotifyIME() checks if it's safe to noitify IME.
     */
    bool IsSafeToNotifyIME() const;
  };

  class FocusSetEvent: public AChangeEvent
  {
  public:
    explicit FocusSetEvent(IMEContentObserver* aIMEContentObserver)
      : AChangeEvent(eChangeEventType_Focus, aIMEContentObserver)
    {
    }
    NS_IMETHOD Run() override;
  };

  class SelectionChangeEvent : public AChangeEvent
  {
  public:
    SelectionChangeEvent(IMEContentObserver* aIMEContentObserver,
                         bool aCausedByComposition)
      : AChangeEvent(eChangeEventType_Selection, aIMEContentObserver)
      , mCausedByComposition(aCausedByComposition)
    {
    }
    NS_IMETHOD Run() override;

  private:
    bool mCausedByComposition;
  };

  class TextChangeEvent : public AChangeEvent
  {
  public:
    TextChangeEvent(IMEContentObserver* aIMEContentObserver,
                    TextChangeData& aData)
      : AChangeEvent(eChangeEventType_Text, aIMEContentObserver)
      , mData(aData)
    {
      MOZ_ASSERT(mData.mStored);
      // Reset mStored because this now consumes the data.
      aData.mStored = false;
    }
    NS_IMETHOD Run() override;

  private:
    TextChangeData mData;
  };

  class PositionChangeEvent final : public AChangeEvent
  {
  public:
    explicit PositionChangeEvent(IMEContentObserver* aIMEContentObserver)
      : AChangeEvent(eChangeEventType_Position, aIMEContentObserver)
    {
    }
    NS_IMETHOD Run() override;
  };

  class AsyncMergeableNotificationsFlusher : public AChangeEvent
  {
  public:
    explicit AsyncMergeableNotificationsFlusher(
      IMEContentObserver* aIMEContentObserver)
      : AChangeEvent(eChangeEventType_FlushPendingEvents, aIMEContentObserver)
    {
    }
    NS_IMETHOD Run() override;
  };
};

} // namespace mozilla

#endif // mozilla_IMEContentObserver_h_
