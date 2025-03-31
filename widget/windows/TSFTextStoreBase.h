/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TSFTextStoreBase_h
#define TSFTextStoreBase_h

#include "nsIWidget.h"
#include "nsWindow.h"
#include "TSFUtils.h"  // for inputscope.h with the hack for MinGW
#include "WinUtils.h"
#include "WritingModes.h"

#include "mozilla/RefPtr.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/widget/IMEData.h"

#include <msctf.h>
#include <textstor.h>

struct ITfDocumentMgr;
class nsWindow;

namespace mozilla::widget {

class TSFTextStoreBase : public ITextStoreACP {
 protected:
  using SelectionChangeDataBase = IMENotification::SelectionChangeDataBase;
  using SelectionChangeData = IMENotification::SelectionChangeData;
  using TextChangeDataBase = IMENotification::TextChangeDataBase;
  using TextChangeData = IMENotification::TextChangeData;

 public: /*IUnknown*/
  STDMETHODIMP QueryInterface(REFIID, void**);

  NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(TSFTextStoreBase)

 public: /*ITextStoreACP*/
  STDMETHODIMP AdviseSink(REFIID, IUnknown*, DWORD);
  STDMETHODIMP UnadviseSink(IUnknown*);
  STDMETHODIMP RequestLock(DWORD, HRESULT*);
  STDMETHODIMP GetStatus(TS_STATUS*);
  STDMETHODIMP QueryInsert(LONG, LONG, ULONG, LONG*, LONG*);
  STDMETHODIMP GetSelection(ULONG, ULONG, TS_SELECTION_ACP*, ULONG*);
  STDMETHODIMP SetSelection(ULONG, const TS_SELECTION_ACP*);
  STDMETHODIMP GetText(LONG, LONG, WCHAR*, ULONG, ULONG*, TS_RUNINFO*, ULONG,
                       ULONG*, LONG*);
  STDMETHODIMP SetText(DWORD, LONG, LONG, const WCHAR*, ULONG, TS_TEXTCHANGE*);
  STDMETHODIMP GetFormattedText(LONG, LONG, IDataObject**);
  STDMETHODIMP GetEmbedded(LONG, REFGUID, REFIID, IUnknown**);
  STDMETHODIMP QueryInsertEmbedded(const GUID*, const FORMATETC*, BOOL*);
  STDMETHODIMP InsertEmbedded(DWORD, LONG, LONG, IDataObject*, TS_TEXTCHANGE*);
  STDMETHODIMP RequestAttrsTransitioningAtPosition(LONG, ULONG,
                                                   const TS_ATTRID*, DWORD);
  STDMETHODIMP FindNextAttrTransition(LONG, LONG, ULONG, const TS_ATTRID*,
                                      DWORD, LONG*, BOOL*, LONG*);
  STDMETHODIMP GetEndACP(LONG*);
  STDMETHODIMP GetActiveView(TsViewCookie*);
  STDMETHODIMP GetACPFromPoint(TsViewCookie, const POINT*, DWORD, LONG*);
  STDMETHODIMP GetTextExt(TsViewCookie, LONG, LONG, RECT*, BOOL*);
  STDMETHODIMP GetScreenExt(TsViewCookie, RECT*);
  STDMETHODIMP GetWnd(TsViewCookie, HWND*);
  STDMETHODIMP InsertTextAtSelection(DWORD, const WCHAR*, ULONG, LONG*, LONG*,
                                     TS_TEXTCHANGE*);
  STDMETHODIMP InsertEmbeddedAtSelection(DWORD, IDataObject*, LONG*, LONG*,
                                         TS_TEXTCHANGE*);

 protected:
  TSFTextStoreBase() = default;
  virtual ~TSFTextStoreBase() = default;

  [[nodiscard]] bool InitBase(nsWindow* aWidget, const InputContext& aContext);

  [[nodiscard]] static bool IsReadLock(DWORD aLock) {
    return (TS_LF_READ == (aLock & TS_LF_READ));
  }
  [[nodiscard]] static bool IsReadWriteLock(DWORD aLock) {
    return (TS_LF_READWRITE == (aLock & TS_LF_READWRITE));
  }
  [[nodiscard]] bool IsReadLocked() const { return IsReadLock(mLock); }
  [[nodiscard]] bool IsReadWriteLocked() const {
    return IsReadWriteLock(mLock);
  }

  // This is called immediately after a call of OnLockGranted() of mSink.
  // Note that mLock isn't cleared yet when this is called.
  virtual void DidLockGranted() {}

  [[nodiscard]] bool GetScreenExtInternal(RECT& aScreenExt);

  // DispatchEvent() dispatches the event and if it may not be handled
  // synchronously, this makes the instance not notify TSF of pending
  // notifications until next notification from content.
  void DispatchEvent(WidgetGUIEvent& aEvent);

  void SetInputScope(const nsString& aHTMLInputType,
                     const nsString& aHTMLInputMode);

  /**
   * Return URL which can be exposed to TSF.  Use ::SysFreeString() to delete
   * the result if you use it by yourself.
   */
  BSTR GetExposingURL() const;

  /**
   * Debug utility method to print the result of GetExposingURL().
   */
  void PrintExposingURL(const char* aPrefix) const;

  // Holds the pointer to our current win32 widget
  RefPtr<nsWindow> mWidget;
  // mDispatcher is a helper class to dispatch composition events.
  RefPtr<TextEventDispatcher> mDispatcher;
  // Document manager for the currently focused editor
  RefPtr<ITfDocumentMgr> mDocumentMgr;
  // Edit cookie associated with the current editing context
  DWORD mEditCookie = 0;
  // Editing context at the bottom of mDocumentMgr's context stack
  RefPtr<ITfContext> mContext;
  // Currently installed notification sink
  RefPtr<ITextStoreACPSink> mSink;
  // TS_AS_* mask of what events to notify
  DWORD mSinkMask = 0;
  // 0 if not locked, otherwise TS_LF_* indicating the current lock
  DWORD mLock = 0;
  // 0 if no lock is queued, otherwise TS_LF_* indicating the queue lock
  DWORD mLockQueued = 0;

  /**
   * IsHandlingCompositionInParent() returns true if eCompositionStart is
   * dispatched, but eCompositionCommit(AsIs) is not dispatched.  This means
   * that if composition is handled in a content process, this status indicates
   * whether ContentCacheInParent has composition or not.  On the other hand,
   * if it's handled in the chrome process, this is exactly same as
   * IsHandlingCompositionInContent().
   */
  [[nodiscard]] bool IsHandlingCompositionInParent() const {
    return mDispatcher && mDispatcher->IsComposing();
  }

  /**
   * IsHandlingCompositionInContent() returns true if there is a composition in
   * the focused editor which may be in a content process.
   */
  [[nodiscard]] bool IsHandlingCompositionInContent() const {
    return mDispatcher && mDispatcher->IsHandlingComposition();
  }

  // The input scopes for this context, defaults to IS_DEFAULT.
  nsTArray<InputScope> mInputScopes;

  // The URL cache of the focused document.
  nsString mDocumentURL;

  // Before calling ITextStoreACPSink::OnLayoutChange() and
  // ITfContextOwnerServices::OnLayoutChange(), mWaitingQueryLayout is set to
  // true.  This is set to  false when GetTextExt() or GetACPFromPoint() is
  // called.
  bool mWaitingQueryLayout = false;
  // During the document is locked, we shouldn't destroy the instance.
  // If this is true, the instance will be destroyed after unlocked.
  bool mPendingDestroy = false;
  // While the instance is initializing content/selection cache, another
  // initialization shouldn't run recursively.  Therefore, while the
  // initialization is running, this is set to true.  Use AutoNotifyingTSFBatch
  // to set this.
  bool mDeferNotifyingTSF = false;
  // While the instance is dispatching events, the event may not be handled
  // synchronously when remote content has focus.  In the case, we cannot
  // return the latest layout/content information to TSF/TIP until we get next
  // update notification from ContentCacheInParent.  For preventing TSF/TIP
  // retrieves the latest content/layout information while it becomes available,
  // we should put off notifying TSF of any updates.
  bool mDeferNotifyingTSFUntilNextUpdate = false;
  // Immediately after a call of Destroy(), mDestroyed becomes true.  If this
  // is true, the instance shouldn't grant any requests from the TIP anymore.
  bool mDestroyed = false;
  // While the instance is being destroyed, this is set to true for avoiding
  // recursive Destroy() calls.
  bool mBeingDestroyed = false;
  // Whether we're in the private browsing mode.
  bool mInPrivateBrowsing = true;
};

}  // namespace mozilla::widget

#endif  // #ifndef TSFTextStoreBase_h
