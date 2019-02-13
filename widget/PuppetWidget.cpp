/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "ClientLayerManager.h"
#include "gfxPlatform.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/Hal.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/unused.h"
#include "PuppetWidget.h"
#include "nsIWidgetListener.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;

static void
InvalidateRegion(nsIWidget* aWidget, const nsIntRegion& aRegion)
{
  nsIntRegionRectIterator it(aRegion);
  while(const nsIntRect* r = it.Next()) {
    aWidget->Invalidate(*r);
  }
}

/*static*/ already_AddRefed<nsIWidget>
nsIWidget::CreatePuppetWidget(TabChild* aTabChild)
{
  MOZ_ASSERT(!aTabChild || nsIWidget::UsePuppetWidgets(),
             "PuppetWidgets not allowed in this configuration");

  nsCOMPtr<nsIWidget> widget = new PuppetWidget(aTabChild);
  return widget.forget();
}

namespace mozilla {
namespace widget {

static bool
IsPopup(const nsWidgetInitData* aInitData)
{
  return aInitData && aInitData->mWindowType == eWindowType_popup;
}

static bool
MightNeedIMEFocus(const nsWidgetInitData* aInitData)
{
  // In the puppet-widget world, popup widgets are just dummies and
  // shouldn't try to mess with IME state.
#ifdef MOZ_CROSS_PROCESS_IME
  return !IsPopup(aInitData);
#else
  return false;
#endif
}

// Arbitrary, fungible.
const size_t PuppetWidget::kMaxDimension = 4000;

NS_IMPL_ISUPPORTS_INHERITED0(PuppetWidget, nsBaseWidget)

PuppetWidget::PuppetWidget(TabChild* aTabChild)
  : mTabChild(aTabChild)
  , mMemoryPressureObserver(nullptr)
  , mDPI(-1)
  , mDefaultScale(-1)
  , mNativeKeyCommandsValid(false)
  , mCursorHotspotX(0)
  , mCursorHotspotY(0)
{
  MOZ_COUNT_CTOR(PuppetWidget);

  mSingleLineCommands.SetCapacity(4);
  mMultiLineCommands.SetCapacity(4);
  mRichTextCommands.SetCapacity(4);
}

PuppetWidget::~PuppetWidget()
{
  MOZ_COUNT_DTOR(PuppetWidget);

  Destroy();
}

NS_IMETHODIMP
PuppetWidget::Create(nsIWidget        *aParent,
                     nsNativeWidget   aNativeParent,
                     const nsIntRect  &aRect,
                     nsWidgetInitData *aInitData)
{
  MOZ_ASSERT(!aNativeParent, "got a non-Puppet native parent");

  BaseCreate(nullptr, aRect, aInitData);

  mBounds = aRect;
  mEnabled = true;
  mVisible = true;

  mDrawTarget = gfxPlatform::GetPlatform()->
    CreateOffscreenContentDrawTarget(IntSize(1, 1), SurfaceFormat::B8G8R8A8);

  mNeedIMEStateInit = MightNeedIMEFocus(aInitData);

  PuppetWidget* parent = static_cast<PuppetWidget*>(aParent);
  if (parent) {
    parent->SetChild(this);
    mLayerManager = parent->GetLayerManager();
  }
  else {
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, false);
  }
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    mMemoryPressureObserver = new MemoryPressureObserver(this);
    obs->AddObserver(mMemoryPressureObserver, "memory-pressure", false);
  }

  return NS_OK;
}

void
PuppetWidget::InitIMEState()
{
  MOZ_ASSERT(mTabChild);
  if (mNeedIMEStateInit) {
    mContentCache.Clear();
    mTabChild->SendNotifyIMEFocus(false, mContentCache,
                                  &mIMEPreferenceOfParent);
    mNeedIMEStateInit = false;
  }
}

already_AddRefed<nsIWidget>
PuppetWidget::CreateChild(const nsIntRect  &aRect,
                          nsWidgetInitData *aInitData,
                          bool             aForceUseIWidgetParent)
{
  bool isPopup = IsPopup(aInitData);
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(mTabChild);
  return ((widget &&
           NS_SUCCEEDED(widget->Create(isPopup ? nullptr: this, nullptr, aRect,
                                       aInitData))) ?
          widget.forget() : nullptr);
}

NS_IMETHODIMP
PuppetWidget::Destroy()
{
  Base::OnDestroy();
  Base::Destroy();
  mPaintTask.Revoke();
  if (mMemoryPressureObserver) {
    mMemoryPressureObserver->Remove();
  }
  mMemoryPressureObserver = nullptr;
  mChild = nullptr;
  if (mLayerManager) {
    mLayerManager->Destroy();
  }
  mLayerManager = nullptr;
  mTabChild = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Show(bool aState)
{
  NS_ASSERTION(mEnabled,
               "does it make sense to Show()/Hide() a disabled widget?");

  bool wasVisible = mVisible;
  mVisible = aState;

  if (mChild) {
    mChild->mVisible = aState;
  }

  if (!wasVisible && mVisible) {
    Resize(mBounds.width, mBounds.height, false);
    Invalidate(mBounds);
  }

  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Resize(double aWidth,
                     double aHeight,
                     bool   aRepaint)
{
  nsIntRect oldBounds = mBounds;
  mBounds.SizeTo(nsIntSize(NSToIntRound(aWidth), NSToIntRound(aHeight)));

  if (mChild) {
    return mChild->Resize(aWidth, aHeight, aRepaint);
  }

  // XXX: roc says that |aRepaint| dictates whether or not to
  // invalidate the expanded area
  if (oldBounds.Size() < mBounds.Size() && aRepaint) {
    nsIntRegion dirty(mBounds);
    dirty.Sub(dirty,  oldBounds);
    InvalidateRegion(this, dirty);
  }

  if (!oldBounds.IsEqualEdges(mBounds) && mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
  }

  return NS_OK;
}

nsresult
PuppetWidget::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& configuration = aConfigurations[i];
    PuppetWidget* w = static_cast<PuppetWidget*>(configuration.mChild.get());
    NS_ASSERTION(w->GetParent() == this,
                 "Configured widget is not a child");
    w->SetWindowClipRegion(configuration.mClipRegion, true);
    nsIntRect bounds;
    w->GetBounds(bounds);
    if (bounds.Size() != configuration.mBounds.Size()) {
      w->Resize(configuration.mBounds.x, configuration.mBounds.y,
                configuration.mBounds.width, configuration.mBounds.height,
                true);
    } else if (bounds.TopLeft() != configuration.mBounds.TopLeft()) {
      w->Move(configuration.mBounds.x, configuration.mBounds.y);
    }
    w->SetWindowClipRegion(configuration.mClipRegion, false);
  }
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::SetFocus(bool aRaise)
{
  // XXX/cjones: someone who knows about event handling needs to
  // decide how this should work.
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Invalidate(const nsIntRect& aRect)
{
#ifdef DEBUG
  debug_DumpInvalidate(stderr, this, &aRect,
                       nsAutoCString("PuppetWidget"), 0);
#endif

  if (mChild) {
    return mChild->Invalidate(aRect);
  }

  mDirtyRegion.Or(mDirtyRegion, aRect);

  if (!mDirtyRegion.IsEmpty() && !mPaintTask.IsPending()) {
    mPaintTask = new PaintTask(this);
    return NS_DispatchToCurrentThread(mPaintTask.get());
  }

  return NS_OK;
}

void
PuppetWidget::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (nullptr == aPoint) {
    event.refPoint.x = 0;
    event.refPoint.y = 0;
  }
  else {
    // use the point override if provided
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }
  event.time = PR_Now() / 1000;
}

NS_IMETHODIMP
PuppetWidget::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus)
{
#ifdef DEBUG
  debug_DumpEvent(stdout, event->widget, event,
                  nsAutoCString("PuppetWidget"), 0);
#endif

  MOZ_ASSERT(!mChild || mChild->mWindowType == eWindowType_popup,
             "Unexpected event dispatch!");

  AutoCacheNativeKeyCommands autoCache(this);
  if (event->mFlags.mIsSynthesizedForTests && !mNativeKeyCommandsValid) {
    WidgetKeyboardEvent* keyEvent = event->AsKeyboardEvent();
    if (keyEvent) {
      mTabChild->RequestNativeKeyBindings(&autoCache, keyEvent);
    }
  }

  aStatus = nsEventStatus_eIgnore;

  if (mAttachedWidgetListener) {
    aStatus = mAttachedWidgetListener->HandleEvent(event, mUseAttachedEvents);
  }

  return NS_OK;
}

nsEventStatus
PuppetWidget::DispatchInputEvent(WidgetInputEvent* aEvent)
{
  if (!mTabChild) {
    return nsEventStatus_eIgnore;
  }

  switch (aEvent->mClass) {
    case eMouseEventClass:
      unused <<
        mTabChild->SendDispatchMouseEvent(*aEvent->AsMouseEvent());
      break;
    case eKeyboardEventClass:
      unused <<
        mTabChild->SendDispatchKeyboardEvent(*aEvent->AsKeyboardEvent());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unsupported event type");
  }

  return nsEventStatus_eIgnore;
}

nsEventStatus
PuppetWidget::DispatchAPZAwareEvent(WidgetInputEvent* aEvent)
{
  if (!AsyncPanZoomEnabled()) {
    nsEventStatus status = nsEventStatus_eIgnore;
    DispatchEvent(aEvent, status);
    return status;
  }

  if (!mTabChild) {
    return nsEventStatus_eIgnore;
  }

  switch (aEvent->mClass) {
    case eWheelEventClass:
      unused <<
        mTabChild->SendDispatchWheelEvent(*aEvent->AsWheelEvent());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unsupported event type");
  }

  return nsEventStatus_eIgnore;
}

nsresult
PuppetWidget::SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                       int32_t aNativeKeyCode,
                                       uint32_t aModifierFlags,
                                       const nsAString& aCharacters,
                                       const nsAString& aUnmodifiedCharacters,
                                       nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "keyevent");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeKeyEvent(aNativeKeyboardLayout, aNativeKeyCode,
    aModifierFlags, nsString(aCharacters), nsString(aUnmodifiedCharacters),
    notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeMouseEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                         uint32_t aNativeMessage,
                                         uint32_t aModifierFlags,
                                         nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mouseevent");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeMouseEvent(aPoint, aNativeMessage,
    aModifierFlags, notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeMouseMove(mozilla::LayoutDeviceIntPoint aPoint,
                                        nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mousemove");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeMouseMove(aPoint, notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeMouseScrollEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                               uint32_t aNativeMessage,
                                               double aDeltaX,
                                               double aDeltaY,
                                               double aDeltaZ,
                                               uint32_t aModifierFlags,
                                               uint32_t aAdditionalFlags,
                                               nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mousescrollevent");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeMouseScrollEvent(aPoint, aNativeMessage,
    aDeltaX, aDeltaY, aDeltaZ, aModifierFlags, aAdditionalFlags,
    notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                         TouchPointerState aPointerState,
                                         nsIntPoint aPointerScreenPoint,
                                         double aPointerPressure,
                                         uint32_t aPointerOrientation,
                                         nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "touchpoint");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeTouchPoint(aPointerId, aPointerState,
    aPointerScreenPoint, aPointerPressure, aPointerOrientation,
    notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeTouchTap(nsIntPoint aPointerScreenPoint,
                                       bool aLongTap,
                                       nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "touchtap");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeTouchTap(aPointerScreenPoint, aLongTap,
    notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::ClearNativeTouchSequence(nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "cleartouch");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendClearNativeTouchSequence(notifier.SaveObserver());
  return NS_OK;
}
 
void
PuppetWidget::SetConfirmedTargetAPZC(uint64_t aInputBlockId,
                                     const nsTArray<ScrollableLayerGuid>& aTargets) const
{
  if (mTabChild) {
    mTabChild->SendSetTargetAPZC(aInputBlockId, aTargets);
  }
}

void
PuppetWidget::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                    const FrameMetrics::ViewID& aViewId,
                                    const Maybe<ZoomConstraints>& aConstraints)
{
  if (mTabChild) {
    mTabChild->DoUpdateZoomConstraints(aPresShellId, aViewId, aConstraints);
  }
}

bool
PuppetWidget::AsyncPanZoomEnabled() const
{
  return mTabChild && mTabChild->AsyncPanZoomEnabled();
}

NS_IMETHODIMP_(bool)
PuppetWidget::ExecuteNativeKeyBinding(NativeKeyBindingsType aType,
                                      const mozilla::WidgetKeyboardEvent& aEvent,
                                      DoCommandCallback aCallback,
                                      void* aCallbackData)
{
  // B2G doesn't have native key bindings.
#ifdef MOZ_WIDGET_GONK
  return false;
#else // #ifdef MOZ_WIDGET_GONK
  MOZ_ASSERT(mNativeKeyCommandsValid);

  const nsTArray<mozilla::CommandInt>* commands = nullptr;
  switch (aType) {
    case nsIWidget::NativeKeyBindingsForSingleLineEditor:
      commands = &mSingleLineCommands;
      break;
    case nsIWidget::NativeKeyBindingsForMultiLineEditor:
      commands = &mMultiLineCommands;
      break;
    case nsIWidget::NativeKeyBindingsForRichTextEditor:
      commands = &mRichTextCommands;
      break;
    default:
      MOZ_CRASH("Invalid type");
      break;
  }

  if (commands->IsEmpty()) {
    return false;
  }

  for (uint32_t i = 0; i < commands->Length(); i++) {
    aCallback(static_cast<mozilla::Command>((*commands)[i]), aCallbackData);
  }
  return true;
#endif
}

LayerManager*
PuppetWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                              LayersBackend aBackendHint,
                              LayerManagerPersistence aPersistence,
                              bool* aAllowRetaining)
{
  if (!mLayerManager) {
    mLayerManager = new ClientLayerManager(this);
  }
  ShadowLayerForwarder* lf = mLayerManager->AsShadowForwarder();
  if (!lf->HasShadowManager() && aShadowManager) {
    lf->SetShadowManager(aShadowManager);
  }
  if (aAllowRetaining) {
    *aAllowRetaining = true;
  }
  return mLayerManager;
}

nsresult
PuppetWidget::IMEEndComposition(bool aCancel)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  nsEventStatus status;
  bool noCompositionEvent = true;
  WidgetCompositionEvent compositionCommitEvent(true, NS_COMPOSITION_COMMIT,
                                                this);
  InitEvent(compositionCommitEvent, nullptr);
  // SendEndIMEComposition is always called since ResetInputState
  // should always be called even if we aren't composing something.
  if (!mTabChild ||
      !mTabChild->SendEndIMEComposition(aCancel, &noCompositionEvent,
                                        &compositionCommitEvent.mData)) {
    return NS_ERROR_FAILURE;
  }

  if (noCompositionEvent) {
    return NS_OK;
  }

  DispatchEvent(&compositionCommitEvent, status);
  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEInternal(const IMENotification& aIMENotification)
{
  switch (aIMENotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      return IMEEndComposition(false);
    case REQUEST_TO_CANCEL_COMPOSITION:
      return IMEEndComposition(true);
    case NOTIFY_IME_OF_FOCUS:
    case NOTIFY_IME_OF_BLUR:
      return NotifyIMEOfFocusChange(aIMENotification);
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      return NotifyIMEOfSelectionChange(aIMENotification);
    case NOTIFY_IME_OF_TEXT_CHANGE:
      return NotifyIMEOfTextChange(aIMENotification);
    case NOTIFY_IME_OF_COMPOSITION_UPDATE:
      return NotifyIMEOfUpdateComposition(aIMENotification);
    case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
      return NotifyIMEOfMouseButtonEvent(aIMENotification);
    case NOTIFY_IME_OF_POSITION_CHANGE:
      return NotifyIMEOfPositionChange(aIMENotification);
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP
PuppetWidget::StartPluginIME(const mozilla::WidgetKeyboardEvent& aKeyboardEvent,
                             int32_t aPanelX, int32_t aPanelY,
                             nsString& aCommitted)
{
  if (!mTabChild ||
      !mTabChild->SendStartPluginIME(aKeyboardEvent, aPanelX,
                                     aPanelY, &aCommitted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::SetPluginFocused(bool& aFocused)
{
  if (!mTabChild || !mTabChild->SendSetPluginFocused(aFocused)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP_(void)
PuppetWidget::SetInputContext(const InputContext& aContext,
                              const InputContextAction& aAction)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return;
#endif

  if (!mTabChild) {
    return;
  }
  mTabChild->SendSetInputContext(
    static_cast<int32_t>(aContext.mIMEState.mEnabled),
    static_cast<int32_t>(aContext.mIMEState.mOpen),
    aContext.mHTMLInputType,
    aContext.mHTMLInputInputmode,
    aContext.mActionHint,
    static_cast<int32_t>(aAction.mCause),
    static_cast<int32_t>(aAction.mFocusChange));
}

NS_IMETHODIMP_(InputContext)
PuppetWidget::GetInputContext()
{
#ifndef MOZ_CROSS_PROCESS_IME
  return InputContext();
#endif

  InputContext context;
  if (mTabChild) {
    int32_t enabled, open;
    intptr_t nativeIMEContext;
    mTabChild->SendGetInputContext(&enabled, &open, &nativeIMEContext);
    context.mIMEState.mEnabled = static_cast<IMEState::Enabled>(enabled);
    context.mIMEState.mOpen = static_cast<IMEState::Open>(open);
    context.mNativeIMEContext = reinterpret_cast<void*>(nativeIMEContext);
  }
  return context;
}

nsresult
PuppetWidget::NotifyIMEOfFocusChange(const IMENotification& aIMENotification)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

  bool gotFocus = aIMENotification.mMessage == NOTIFY_IME_OF_FOCUS;
  if (gotFocus) {
    // When IME gets focus, we should initalize all information of the content.
    if (NS_WARN_IF(!mContentCache.CacheAll(this, &aIMENotification))) {
      return NS_ERROR_FAILURE;
    }
  } else {
    // When IME loses focus, we don't need to store anything.
    mContentCache.Clear();
  }

  mIMEPreferenceOfParent = nsIMEUpdatePreference();
  if (!mTabChild->SendNotifyIMEFocus(gotFocus, mContentCache,
                                     &mIMEPreferenceOfParent)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfUpdateComposition(
                const IMENotification& aIMENotification)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  NS_ENSURE_TRUE(mTabChild, NS_ERROR_FAILURE);

  if (NS_WARN_IF(!mContentCache.CacheSelection(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendNotifyIMESelectedCompositionRect(mContentCache);
  return NS_OK;
}

nsIMEUpdatePreference
PuppetWidget::GetIMEUpdatePreference()
{
#ifdef MOZ_CROSS_PROCESS_IME
  // e10s requires IME information cache into TabParent
  return nsIMEUpdatePreference(mIMEPreferenceOfParent.mWantUpdates |
                               nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE |
                               nsIMEUpdatePreference::NOTIFY_TEXT_CHANGE |
                               nsIMEUpdatePreference::NOTIFY_POSITION_CHANGE );
#else
  // B2G doesn't handle IME as widget-level.
  return nsIMEUpdatePreference();
#endif
}

nsresult
PuppetWidget::NotifyIMEOfTextChange(const IMENotification& aIMENotification)
{
  MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_TEXT_CHANGE,
             "Passed wrong notification");

#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

  // FYI: text change notification is the first notification after
  //      a user operation changes the content.  So, we need to modify
  //      the cache as far as possible here.

  if (NS_WARN_IF(!mContentCache.CacheText(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }

  // TabParent doesn't this this to cache.  we don't send the notification
  // if parent process doesn't request NOTIFY_TEXT_CHANGE.
  if (mIMEPreferenceOfParent.WantTextChange() &&
      (mIMEPreferenceOfParent.WantChangesCausedByComposition() ||
       !aIMENotification.mTextChangeData.mCausedByComposition)) {
    mTabChild->SendNotifyIMETextChange(
      mContentCache,
      aIMENotification.mTextChangeData.mStartOffset,
      aIMENotification.mTextChangeData.mOldEndOffset,
      aIMENotification.mTextChangeData.mNewEndOffset,
      aIMENotification.mTextChangeData.mCausedByComposition);
  } else {
    mTabChild->SendUpdateContentCache(mContentCache);
  }
  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfSelectionChange(
                const IMENotification& aIMENotification)
{
  MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_SELECTION_CHANGE,
             "Passed wrong notification");

#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

  // Note that selection change must be notified after text change if it occurs.
  // Therefore, we don't need to query text content again here.
  mContentCache.SetSelection(
    this,
    aIMENotification.mSelectionChangeData.mOffset,
    aIMENotification.mSelectionChangeData.mLength,
    aIMENotification.mSelectionChangeData.mReversed,
    aIMENotification.mSelectionChangeData.GetWritingMode());

  mTabChild->SendNotifyIMESelection(
    mContentCache, aIMENotification.mSelectionChangeData.mCausedByComposition);
  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfMouseButtonEvent(
                const IMENotification& aIMENotification)
{
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }

  bool consumedByIME = false;
  if (!mTabChild->SendNotifyIMEMouseButtonEvent(aIMENotification,
                                                &consumedByIME)) {
    return NS_ERROR_FAILURE;
  }

  return consumedByIME ? NS_SUCCESS_EVENT_CONSUMED : NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfPositionChange(const IMENotification& aIMENotification)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif
  if (NS_WARN_IF(!mTabChild)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!mContentCache.CacheEditorRect(this, &aIMENotification)) ||
      NS_WARN_IF(!mContentCache.CacheSelection(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  if (!mTabChild->SendNotifyIMEPositionChange(mContentCache)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::SetCursor(nsCursor aCursor)
{
  if (mCursor == aCursor && !mCustomCursor && !mUpdateCursor) {
    return NS_OK;
  }

  mCustomCursor = nullptr;

  if (mTabChild &&
      !mTabChild->SendSetCursor(aCursor, mUpdateCursor)) {
    return NS_ERROR_FAILURE;
  }

  mCursor = aCursor;
  mUpdateCursor = false;

  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::SetCursor(imgIContainer* aCursor,
                        uint32_t aHotspotX, uint32_t aHotspotY)
{
  if (!aCursor || !mTabChild) {
    return NS_OK;
  }

  if (mCustomCursor == aCursor &&
      mCursorHotspotX == aHotspotX &&
      mCursorHotspotY == aHotspotY &&
      !mUpdateCursor) {
    return NS_OK;
  }

  RefPtr<mozilla::gfx::SourceSurface> surface =
    aCursor->GetFrame(imgIContainer::FRAME_CURRENT,
                      imgIContainer::FLAG_SYNC_DECODE);
  if (!surface) {
    return NS_ERROR_FAILURE;
  }

  mozilla::RefPtr<mozilla::gfx::DataSourceSurface> dataSurface =
    surface->GetDataSurface();
  size_t length;
  int32_t stride;
  mozilla::UniquePtr<char[]> surfaceData =
    nsContentUtils::GetSurfaceData(dataSurface, &length, &stride);

  nsCString cursorData = nsCString(surfaceData.get(), length);
  mozilla::gfx::IntSize size = dataSurface->GetSize();
  if (!mTabChild->SendSetCustomCursor(cursorData, size.width, size.height, stride,
                                      static_cast<uint8_t>(dataSurface->GetFormat()),
                                      aHotspotX, aHotspotY, mUpdateCursor)) {
    return NS_ERROR_FAILURE;
  }

  mCursor = nsCursor(-1);
  mCustomCursor = aCursor;
  mCursorHotspotX = aHotspotX;
  mCursorHotspotY = aHotspotY;
  mUpdateCursor = false;

  return NS_OK;
}

void
PuppetWidget::ClearCachedCursor()
{
  nsBaseWidget::ClearCachedCursor();
  mCustomCursor = nullptr;
}

nsresult
PuppetWidget::Paint()
{
  MOZ_ASSERT(!mDirtyRegion.IsEmpty(), "paint event logic messed up");

  if (!mAttachedWidgetListener)
    return NS_OK;

  nsIntRegion region = mDirtyRegion;

  // reset repaint tracking
  mDirtyRegion.SetEmpty();
  mPaintTask.Revoke();

  mAttachedWidgetListener->WillPaintWindow(this);

  if (mAttachedWidgetListener) {
#ifdef DEBUG
    debug_DumpPaintEvent(stderr, this, region,
                         nsAutoCString("PuppetWidget"), 0);
#endif

    if (mozilla::layers::LayersBackend::LAYERS_CLIENT == mLayerManager->GetBackendType()) {
      // Do nothing, the compositor will handle drawing
      if (mTabChild) {
        mTabChild->NotifyPainted();
      }
    } else {
      nsRefPtr<gfxContext> ctx = new gfxContext(mDrawTarget);
      ctx->Rectangle(gfxRect(0,0,0,0));
      ctx->Clip();
      AutoLayerManagerSetup setupLayerManager(this, ctx,
                                              BufferMode::BUFFER_NONE);
      mAttachedWidgetListener->PaintWindow(this, region);
      if (mTabChild) {
        mTabChild->NotifyPainted();
      }
    }
  }

  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->DidPaintWindow();
  }

  return NS_OK;
}

void
PuppetWidget::SetChild(PuppetWidget* aChild)
{
  MOZ_ASSERT(this != aChild, "can't parent a widget to itself");
  MOZ_ASSERT(!aChild->mChild,
             "fake widget 'hierarchy' only expected to have one level");

  mChild = aChild;
}

NS_IMETHODIMP
PuppetWidget::PaintTask::Run()
{
  if (mWidget) {
    mWidget->Paint();
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PuppetWidget::MemoryPressureObserver, nsIObserver)

NS_IMETHODIMP
PuppetWidget::MemoryPressureObserver::Observe(nsISupports* aSubject,
                                              const char* aTopic,
                                              const char16_t* aData)
{
  if (!mWidget) {
    return NS_OK;
  }

  if (strcmp("memory-pressure", aTopic) == 0) {
    if (!mWidget->mVisible && mWidget->mLayerManager &&
        XRE_GetProcessType() == GeckoProcessType_Content) {
      mWidget->mLayerManager->ClearCachedResources();
    }
  }
  return NS_OK;
}

void
PuppetWidget::MemoryPressureObserver::Remove()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "memory-pressure");
  }
  mWidget = nullptr;
}

bool
PuppetWidget::NeedsPaint()
{
  // e10s popups are handled by the parent process, so never should be painted here
  if (XRE_GetProcessType() == GeckoProcessType_Content &&
      Preferences::GetBool("browser.tabs.remote.desktopbehavior", false) &&
      mWindowType == eWindowType_popup) {
    NS_WARNING("Trying to paint an e10s popup in the child process!");
    return false;
  }

  return mVisible;
}

float
PuppetWidget::GetDPI()
{
  if (mDPI < 0) {
    if (mTabChild) {
      mTabChild->GetDPI(&mDPI);
    } else {
      mDPI = 96.0;
    }
  }

  return mDPI;
}

double
PuppetWidget::GetDefaultScaleInternal()
{
  if (mDefaultScale < 0) {
    if (mTabChild) {
      mTabChild->GetDefaultScale(&mDefaultScale);
    } else {
      mDefaultScale = 1;
    }
  }

  return mDefaultScale;
}

void*
PuppetWidget::GetNativeData(uint32_t aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_SHAREABLE_WINDOW: {
    MOZ_ASSERT(mTabChild, "Need TabChild to get the nativeWindow from!");
    mozilla::WindowsHandle nativeData = 0;
    if (mTabChild) {
      mTabChild->SendGetWidgetNativeData(&nativeData);
    }
    return (void*)nativeData;
  }
  case NS_NATIVE_WINDOW:
  case NS_NATIVE_DISPLAY:
  case NS_NATIVE_PLUGIN_PORT:
  case NS_NATIVE_GRAPHIC:
  case NS_NATIVE_SHELLWIDGET:
  case NS_NATIVE_WIDGET:
    NS_WARNING("nsWindow::GetNativeData not implemented for this type");
    break;
  default:
    NS_WARNING("nsWindow::GetNativeData called with bad value");
    break;
  }
  return nullptr;
}

#if defined(XP_WIN)
void
PuppetWidget::SetNativeData(uint32_t aDataType, uintptr_t aVal)
{
  switch (aDataType) {
  case NS_NATIVE_CHILD_OF_SHAREABLE_WINDOW:
    MOZ_ASSERT(mTabChild, "Need TabChild to send the message.");
    if (mTabChild) {
      mTabChild->SendSetNativeChildOfShareableWindow(aVal);
    }
    break;
  default:
    NS_WARNING("SetNativeData called with unsupported data type.");
  }
}
#endif

nsIntPoint
PuppetWidget::GetChromeDimensions()
{
  if (!GetOwningTabChild()) {
    NS_WARNING("PuppetWidget without Tab does not have chrome information.");
    return nsIntPoint();
  }
  return LayoutDeviceIntPoint::ToUntyped(GetOwningTabChild()->GetChromeDisplacement());
}

nsIntPoint
PuppetWidget::GetWindowPosition()
{
  if (!GetOwningTabChild()) {
    return nsIntPoint();
  }

  int32_t winX, winY, winW, winH;
  NS_ENSURE_SUCCESS(GetOwningTabChild()->GetDimensions(0, &winX, &winY, &winW, &winH), nsIntPoint());
  return nsIntPoint(winX, winY);
}

NS_METHOD
PuppetWidget::GetScreenBounds(nsIntRect &aRect) {
  aRect.MoveTo(LayoutDeviceIntPoint::ToUntyped(WidgetToScreenOffset()));
  aRect.SizeTo(mBounds.Size());
  return NS_OK;
}

uint32_t PuppetWidget::GetMaxTouchPoints() const
{
  static uint32_t sTouchPoints = 0;
  static bool sIsInitialized = false;
  if (sIsInitialized) {
    return sTouchPoints;
  }
  if (mTabChild) {
    mTabChild->GetMaxTouchPoints(&sTouchPoints);
    sIsInitialized = true;
  }
  return sTouchPoints;
}

PuppetScreen::PuppetScreen(void *nativeScreen)
{
}

PuppetScreen::~PuppetScreen()
{
}

static ScreenConfiguration
ScreenConfig()
{
  ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  return config;
}

nsIntSize
PuppetWidget::GetScreenDimensions()
{
  nsIntRect r = ScreenConfig().rect();
  return nsIntSize(r.width, r.height);
}

NS_IMETHODIMP
PuppetScreen::GetId(uint32_t *outId)
{
  *outId = 1;
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetRect(int32_t *outLeft,  int32_t *outTop,
                      int32_t *outWidth, int32_t *outHeight)
{
  nsIntRect r = ScreenConfig().rect();
  *outLeft = r.x;
  *outTop = r.y;
  *outWidth = r.width;
  *outHeight = r.height;
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetAvailRect(int32_t *outLeft,  int32_t *outTop,
                           int32_t *outWidth, int32_t *outHeight)
{
  return GetRect(outLeft, outTop, outWidth, outHeight);
}

NS_IMETHODIMP
PuppetScreen::GetPixelDepth(int32_t *aPixelDepth)
{
  *aPixelDepth = ScreenConfig().pixelDepth();
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetColorDepth(int32_t *aColorDepth)
{
  *aColorDepth = ScreenConfig().colorDepth();
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetRotation(uint32_t* aRotation)
{
  NS_WARNING("Attempt to get screen rotation through nsIScreen::GetRotation().  Nothing should know or care this in sandboxed contexts.  If you want *orientation*, use hal.");
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
PuppetScreen::SetRotation(uint32_t aRotation)
{
  NS_WARNING("Attempt to set screen rotation through nsIScreen::GetRotation().  Nothing should know or care this in sandboxed contexts.  If you want *orientation*, use hal.");
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMPL_ISUPPORTS(PuppetScreenManager, nsIScreenManager)

PuppetScreenManager::PuppetScreenManager()
{
    mOneScreen = new PuppetScreen(nullptr);
}

PuppetScreenManager::~PuppetScreenManager()
{
}

NS_IMETHODIMP
PuppetScreenManager::ScreenForId(uint32_t aId,
                                 nsIScreen** outScreen)
{
  NS_IF_ADDREF(*outScreen = mOneScreen.get());
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreenManager::GetPrimaryScreen(nsIScreen** outScreen)
{
  NS_IF_ADDREF(*outScreen = mOneScreen.get());
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreenManager::ScreenForRect(int32_t inLeft,
                                   int32_t inTop,
                                   int32_t inWidth,
                                   int32_t inHeight,
                                   nsIScreen** outScreen)
{
  return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
PuppetScreenManager::ScreenForNativeWidget(void* aWidget,
                                           nsIScreen** outScreen)
{
  return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
PuppetScreenManager::GetNumberOfScreens(uint32_t* aNumberOfScreens)
{
  *aNumberOfScreens = 1;
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreenManager::GetSystemDefaultScale(float *aDefaultScale)
{
  *aDefaultScale = 1.0f;
  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
