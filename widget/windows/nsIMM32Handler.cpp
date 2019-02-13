/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "nsIMM32Handler.h"
#include "nsWindow.h"
#include "nsWindowDefs.h"
#include "WinUtils.h"
#include "KeyboardLayout.h"
#include <algorithm>

#include "mozilla/MiscEvents.h"
#include "mozilla/TextEvents.h"

#ifndef IME_PROP_ACCEPT_WIDE_VKEY
#define IME_PROP_ACCEPT_WIDE_VKEY 0x20
#endif

using namespace mozilla;
using namespace mozilla::widget;

static nsIMM32Handler* gIMM32Handler = nullptr;

PRLogModuleInfo* gIMM32Log = nullptr;

static void
HandleSeparator(nsACString& aDesc)
{
  if (!aDesc.IsEmpty()) {
    aDesc.AppendLiteral(" | ");
  }
}

class GetIMEGeneralPropertyName : public nsAutoCString
{
public:
  GetIMEGeneralPropertyName(DWORD aFlags)
  {
    if (!aFlags) {
      AppendLiteral("no flags");
      return;
    }
    if (aFlags & IME_PROP_AT_CARET) {
      AppendLiteral("IME_PROP_AT_CARET");
    }
    if (aFlags & IME_PROP_SPECIAL_UI) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_SPECIAL_UI");
    }
    if (aFlags & IME_PROP_CANDLIST_START_FROM_1) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_CANDLIST_START_FROM_1");
    }
    if (aFlags & IME_PROP_UNICODE) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_UNICODE");
    }
    if (aFlags & IME_PROP_COMPLETE_ON_UNSELECT) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_COMPLETE_ON_UNSELECT");
    }
    if (aFlags & IME_PROP_ACCEPT_WIDE_VKEY) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_ACCEPT_WIDE_VKEY");
    }
  }
  virtual ~GetIMEGeneralPropertyName() {}
};

class GetIMEUIPropertyName : public nsAutoCString
{
public:
  GetIMEUIPropertyName(DWORD aFlags)
  {
    if (!aFlags) {
      AppendLiteral("no flags");
      return;
    }
    if (aFlags & UI_CAP_2700) {
      AppendLiteral("UI_CAP_2700");
    }
    if (aFlags & UI_CAP_ROT90) {
      HandleSeparator(*this);
      AppendLiteral("UI_CAP_ROT90");
    }
    if (aFlags & UI_CAP_ROTANY) {
      HandleSeparator(*this);
      AppendLiteral("UI_CAP_ROTANY");
    }
  }
  virtual ~GetIMEUIPropertyName() {}
};

class GetWritingModeName : public nsAutoCString
{
public:
  GetWritingModeName(const WritingMode& aWritingMode)
  {
    if (!aWritingMode.IsVertical()) {
      Assign("Horizontal");
      return;
    }
    if (aWritingMode.IsVerticalLR()) {
      Assign("Vertical (LR)");
      return;
    }
    Assign("Vertical (RL)");
  }
  virtual ~GetWritingModeName() {}
};

static UINT sWM_MSIME_MOUSE = 0; // mouse message for MSIME 98/2000

//-------------------------------------------------------------------------
//
// from http://download.microsoft.com/download/6/0/9/60908e9e-d2c1-47db-98f6-216af76a235f/msime.h
// The document for this has been removed from MSDN...
//
//-------------------------------------------------------------------------

#define RWM_MOUSE           TEXT("MSIMEMouseOperation")

#define IMEMOUSE_NONE       0x00    // no mouse button was pushed
#define IMEMOUSE_LDOWN      0x01
#define IMEMOUSE_RDOWN      0x02
#define IMEMOUSE_MDOWN      0x04
#define IMEMOUSE_WUP        0x10    // wheel up
#define IMEMOUSE_WDOWN      0x20    // wheel down

WritingMode nsIMM32Handler::sWritingModeOfCompositionFont;
nsString nsIMM32Handler::sIMEName;
UINT nsIMM32Handler::sCodePage = 0;
DWORD nsIMM32Handler::sIMEProperty = 0;
DWORD nsIMM32Handler::sIMEUIProperty = 0;
bool nsIMM32Handler::sAssumeVerticalWritingModeNotSupported = false;

/* static */ void
nsIMM32Handler::EnsureHandlerInstance()
{
  if (!gIMM32Handler) {
    gIMM32Handler = new nsIMM32Handler();
  }
}

/* static */ void
nsIMM32Handler::Initialize()
{
  if (!gIMM32Log)
    gIMM32Log = PR_NewLogModule("nsIMM32HandlerWidgets");

  if (!sWM_MSIME_MOUSE) {
    sWM_MSIME_MOUSE = ::RegisterWindowMessage(RWM_MOUSE);
  }
  sAssumeVerticalWritingModeNotSupported =
    Preferences::GetBool(
      "intl.imm.vertical_writing.always_assume_not_supported", false);
  InitKeyboardLayout(nullptr, ::GetKeyboardLayout(0));
}

/* static */ void
nsIMM32Handler::Terminate()
{
  if (!gIMM32Handler)
    return;
  delete gIMM32Handler;
  gIMM32Handler = nullptr;
}

/* static */ bool
nsIMM32Handler::IsComposingOnOurEditor()
{
  return gIMM32Handler && gIMM32Handler->mIsComposing;
}

/* static */ bool
nsIMM32Handler::IsComposingOnPlugin()
{
  return gIMM32Handler && gIMM32Handler->mIsComposingOnPlugin;
}

/* static */ bool
nsIMM32Handler::IsComposingWindow(nsWindow* aWindow)
{
  return gIMM32Handler && gIMM32Handler->mComposingWindow == aWindow;
}

/* static */ bool
nsIMM32Handler::IsTopLevelWindowOfComposition(nsWindow* aWindow)
{
  if (!gIMM32Handler || !gIMM32Handler->mComposingWindow) {
    return false;
  }
  HWND wnd = gIMM32Handler->mComposingWindow->GetWindowHandle();
  return WinUtils::GetTopLevelHWND(wnd, true) == aWindow->GetWindowHandle();
}

/* static */
bool
nsIMM32Handler::IsJapanist2003Active()
{
  return sIMEName.EqualsLiteral("Japanist 2003");
}

/* static */ bool
nsIMM32Handler::IsGoogleJapaneseInputActive()
{
  // NOTE: Even on Windows for en-US, the name of Google Japanese Input is
  //       written in Japanese.
  return sIMEName.Equals(L"Google \x65E5\x672C\x8A9E\x5165\x529B "
                         L"IMM32 \x30E2\x30B8\x30E5\x30FC\x30EB");
}

/* static */ bool
nsIMM32Handler::ShouldDrawCompositionStringOurselves()
{
  // If current IME has special UI or its composition window should not
  // positioned to caret position, we should now draw composition string
  // ourselves.
  return !(sIMEProperty & IME_PROP_SPECIAL_UI) &&
          (sIMEProperty & IME_PROP_AT_CARET);
}

/* static */ bool
nsIMM32Handler::IsVerticalWritingSupported()
{
  // Even if IME claims that they support vertical writing mode but it may not
  // support vertical writing mode for its candidate window.
  if (sAssumeVerticalWritingModeNotSupported) {
    return false;
  }
  // Google Japanese Input doesn't support vertical writing mode.  We should
  // return false if it's active IME.
  if (IsGoogleJapaneseInputActive()) {
    return false;
  }
  return !!(sIMEUIProperty & (UI_CAP_2700 | UI_CAP_ROT90 | UI_CAP_ROTANY));
}

/* static */ void
nsIMM32Handler::InitKeyboardLayout(nsWindow* aWindow,
                                   HKL aKeyboardLayout)
{
  UINT IMENameLength = ::ImmGetDescriptionW(aKeyboardLayout, nullptr, 0);
  if (IMENameLength) {
    // Add room for the terminating null character
    sIMEName.SetLength(++IMENameLength);
    IMENameLength =
      ::ImmGetDescriptionW(aKeyboardLayout, wwc(sIMEName.BeginWriting()),
                           IMENameLength);
    // Adjust the length to ignore the terminating null character
    sIMEName.SetLength(IMENameLength);
  } else {
    sIMEName.Truncate();
  }

  WORD langID = LOWORD(aKeyboardLayout);
  ::GetLocaleInfoW(MAKELCID(langID, SORT_DEFAULT),
                   LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                   (PWSTR)&sCodePage, sizeof(sCodePage) / sizeof(WCHAR));
  sIMEProperty = ::ImmGetProperty(aKeyboardLayout, IGP_PROPERTY);
  sIMEUIProperty = ::ImmGetProperty(aKeyboardLayout, IGP_UI);

  // If active IME is a TIP of TSF, we cannot retrieve the name with IMM32 API.
  // For hacking some bugs of some TIP, we should set an IME name from the
  // pref.
  if (sCodePage == 932 && sIMEName.IsEmpty()) {
    sIMEName =
      Preferences::GetString("intl.imm.japanese.assume_active_tip_name_as");
  }

  // Whether the IME supports vertical writing mode might be changed or
  // some IMEs may need specific font for their UI.  Therefore, we should
  // update composition font forcibly here.
  if (aWindow) {
    MaybeAdjustCompositionFont(aWindow, sWritingModeOfCompositionFont, true);
  }

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: InitKeyboardLayout, aKeyboardLayout=%08x (\"%s\"), sCodePage=%lu, "
     "sIMEProperty=%s, sIMEUIProperty=%s",
     aKeyboardLayout, NS_ConvertUTF16toUTF8(sIMEName).get(),
     sCodePage, GetIMEGeneralPropertyName(sIMEProperty).get(),
     GetIMEUIPropertyName(sIMEUIProperty).get()));
}

/* static */ UINT
nsIMM32Handler::GetKeyboardCodePage()
{
  return sCodePage;
}

/* static */
nsIMEUpdatePreference
nsIMM32Handler::GetIMEUpdatePreference()
{
  return nsIMEUpdatePreference(
    nsIMEUpdatePreference::NOTIFY_POSITION_CHANGE |
    nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE |
    nsIMEUpdatePreference::NOTIFY_MOUSE_BUTTON_EVENT_ON_CHAR);
}

// used for checking the lParam of WM_IME_COMPOSITION
#define IS_COMPOSING_LPARAM(lParam) \
  ((lParam) & (GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS))
#define IS_COMMITTING_LPARAM(lParam) ((lParam) & GCS_RESULTSTR)
// Some IMEs (e.g., the standard IME for Korean) don't have caret position,
// then, we should not set caret position to compositionchange event.
#define NO_IME_CARET -1

nsIMM32Handler::nsIMM32Handler() :
  mComposingWindow(nullptr), mCursorPosition(NO_IME_CARET), mCompositionStart(0),
  mIsComposing(false), mIsComposingOnPlugin(false),
  mNativeCaretIsCreated(false)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info, ("IMM32: nsIMM32Handler is created\n"));
}

nsIMM32Handler::~nsIMM32Handler()
{
  if (mIsComposing) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: ~nsIMM32Handler, ERROR, the instance is still composing\n"));
  }
  MOZ_LOG(gIMM32Log, LogLevel::Info, ("IMM32: nsIMM32Handler is destroyed\n"));
}

nsresult
nsIMM32Handler::EnsureClauseArray(int32_t aCount)
{
  NS_ENSURE_ARG_MIN(aCount, 0);
  mClauseArray.SetCapacity(aCount + 32);
  return NS_OK;
}

nsresult
nsIMM32Handler::EnsureAttributeArray(int32_t aCount)
{
  NS_ENSURE_ARG_MIN(aCount, 0);
  mAttributeArray.SetCapacity(aCount + 64);
  return NS_OK;
}

/* static */ void
nsIMM32Handler::CommitComposition(nsWindow* aWindow, bool aForce)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: CommitComposition, aForce=%s, aWindow=%p, hWnd=%08x, mComposingWindow=%p%s\n",
     aForce ? "TRUE" : "FALSE",
     aWindow, aWindow->GetWindowHandle(),
     gIMM32Handler ? gIMM32Handler->mComposingWindow : nullptr,
     gIMM32Handler && gIMM32Handler->mComposingWindow ?
       IsComposingOnOurEditor() ? " (composing on editor)" :
                                  " (composing on plug-in)" : ""));
  if (!aForce && !IsComposingWindow(aWindow)) {
    return;
  }

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  bool associated = IMEContext.AssociateDefaultContext();
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: CommitComposition, associated=%s\n",
     associated ? "YES" : "NO"));

  if (IMEContext.IsValid()) {
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_CANCEL, 0);
  }

  if (associated) {
    IMEContext.Disassociate();
  }
}

/* static */ void
nsIMM32Handler::CancelComposition(nsWindow* aWindow, bool aForce)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: CancelComposition, aForce=%s, aWindow=%p, hWnd=%08x, mComposingWindow=%p%s\n",
     aForce ? "TRUE" : "FALSE",
     aWindow, aWindow->GetWindowHandle(),
     gIMM32Handler ? gIMM32Handler->mComposingWindow : nullptr,
     gIMM32Handler && gIMM32Handler->mComposingWindow ?
       IsComposingOnOurEditor() ? " (composing on editor)" :
                                  " (composing on plug-in)" : ""));
  if (!aForce && !IsComposingWindow(aWindow)) {
    return;
  }

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  bool associated = IMEContext.AssociateDefaultContext();
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: CancelComposition, associated=%s\n",
     associated ? "YES" : "NO"));

  if (IMEContext.IsValid()) {
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_CANCEL, 0);
  }

  if (associated) {
    IMEContext.Disassociate();
  }
}

// static
void
nsIMM32Handler::OnUpdateComposition(nsWindow* aWindow)
{
  if (!gIMM32Handler) {
    return;
  }
 
  if (aWindow->PluginHasFocus()) {
    return;
  }

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  gIMM32Handler->SetIMERelatedWindowsPos(aWindow, IMEContext);
}

// static
void
nsIMM32Handler::OnSelectionChange(nsWindow* aWindow,
                                  const IMENotification& aIMENotification)
{
  if (aIMENotification.mSelectionChangeData.mCausedByComposition) {
    return;
  }
  MaybeAdjustCompositionFont(aWindow,
    aIMENotification.mSelectionChangeData.GetWritingMode());
}

// static
void
nsIMM32Handler::MaybeAdjustCompositionFont(nsWindow* aWindow,
                                           const WritingMode& aWritingMode,
                                           bool aForceUpdate)
{
  switch (sCodePage) {
    case 932: // Japanese Shift-JIS
    case 936: // Simlified Chinese GBK
    case 949: // Korean
    case 950: // Traditional Chinese Big5
      EnsureHandlerInstance();
      break;
    default:
      // If there is no instance of nsIMM32Hander, we shouldn't waste footprint.
      if (!gIMM32Handler) {
        return;
      }
  }

  // Like Navi-Bar of ATOK, some IMEs may require proper composition font even
  // before sending WM_IME_STARTCOMPOSITION.
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  gIMM32Handler->AdjustCompositionFont(IMEContext, aWritingMode, aForceUpdate);
}

/* static */ bool
nsIMM32Handler::ProcessInputLangChangeMessage(nsWindow* aWindow,
                                              WPARAM wParam,
                                              LPARAM lParam,
                                              MSGResult& aResult)
{
  aResult.mResult = 0;
  aResult.mConsumed = false;
  // We don't need to create the instance of the handler here.
  if (gIMM32Handler) {
    gIMM32Handler->OnInputLangChange(aWindow, wParam, lParam, aResult);
  }
  InitKeyboardLayout(aWindow, reinterpret_cast<HKL>(lParam));
  // We can release the instance here, because the instance may be never
  // used. E.g., the new keyboard layout may not use IME, or it may use TSF.
  Terminate();
  // Don't return as "processed", the messages should be processed on nsWindow
  // too.
  return false;
}

/* static */ bool
nsIMM32Handler::ProcessMessage(nsWindow* aWindow, UINT msg,
                               WPARAM &wParam, LPARAM &lParam,
                               MSGResult& aResult)
{
  // XXX We store the composing window in mComposingWindow.  If IME messages are
  // sent to different window, we should commit the old transaction.  And also
  // if the new window handle is not focused, probably, we should not start
  // the composition, however, such case should not be, it's just bad scenario.

  // When a plug-in has focus or compsition, we should dispatch the IME events
  // to the plug-in.
  if (aWindow->PluginHasFocus() || IsComposingOnPlugin()) {
      return ProcessMessageForPlugin(aWindow, msg, wParam, lParam, aResult);
  }

  aResult.mResult = 0;
  switch (msg) {
    case WM_INPUTLANGCHANGE:
      return ProcessInputLangChangeMessage(aWindow, wParam, lParam, aResult);
    case WM_IME_STARTCOMPOSITION:
      EnsureHandlerInstance();
      return gIMM32Handler->OnIMEStartComposition(aWindow, aResult);
    case WM_IME_COMPOSITION:
      EnsureHandlerInstance();
      return gIMM32Handler->OnIMEComposition(aWindow, wParam, lParam, aResult);
    case WM_IME_ENDCOMPOSITION:
      EnsureHandlerInstance();
      return gIMM32Handler->OnIMEEndComposition(aWindow, aResult);
    case WM_IME_CHAR:
      return OnIMEChar(aWindow, wParam, lParam, aResult);
    case WM_IME_NOTIFY:
      return OnIMENotify(aWindow, wParam, lParam, aResult);
    case WM_IME_REQUEST:
      EnsureHandlerInstance();
      return gIMM32Handler->OnIMERequest(aWindow, wParam, lParam, aResult);
    case WM_IME_SELECT:
      return OnIMESelect(aWindow, wParam, lParam, aResult);
    case WM_IME_SETCONTEXT:
      return OnIMESetContext(aWindow, wParam, lParam, aResult);
    case WM_KEYDOWN:
      return OnKeyDownEvent(aWindow, wParam, lParam, aResult);
    case WM_CHAR:
      if (!gIMM32Handler) {
        return false;
      }
      return gIMM32Handler->OnChar(aWindow, wParam, lParam, aResult);
    default:
      return false;
  };
}

/* static */ bool
nsIMM32Handler::ProcessMessageForPlugin(nsWindow* aWindow, UINT msg,
                                        WPARAM &wParam, LPARAM &lParam,
                                        MSGResult& aResult)
{
  aResult.mResult = 0;
  aResult.mConsumed = false;
  switch (msg) {
    case WM_INPUTLANGCHANGEREQUEST:
    case WM_INPUTLANGCHANGE:
      aWindow->DispatchPluginEvent(msg, wParam, lParam, false);
      return ProcessInputLangChangeMessage(aWindow, wParam, lParam, aResult);
    case WM_IME_COMPOSITION:
      EnsureHandlerInstance();
      return gIMM32Handler->OnIMECompositionOnPlugin(aWindow, wParam, lParam,
                                                     aResult);
    case WM_IME_STARTCOMPOSITION:
      EnsureHandlerInstance();
      return gIMM32Handler->OnIMEStartCompositionOnPlugin(aWindow, wParam,
                                                          lParam, aResult);
    case WM_IME_ENDCOMPOSITION:
      EnsureHandlerInstance();
      return gIMM32Handler->OnIMEEndCompositionOnPlugin(aWindow, wParam, lParam,
                                                        aResult);
    case WM_IME_CHAR:
      EnsureHandlerInstance();
      return gIMM32Handler->OnIMECharOnPlugin(aWindow, wParam, lParam, aResult);
    case WM_IME_SETCONTEXT:
      return OnIMESetContextOnPlugin(aWindow, wParam, lParam, aResult);
    case WM_CHAR:
      if (!gIMM32Handler) {
        return false;
      }
      return gIMM32Handler->OnCharOnPlugin(aWindow, wParam, lParam, aResult);
    case WM_IME_COMPOSITIONFULL:
    case WM_IME_CONTROL:
    case WM_IME_KEYDOWN:
    case WM_IME_KEYUP:
    case WM_IME_REQUEST:
    case WM_IME_SELECT:
      aResult.mConsumed =
        aWindow->DispatchPluginEvent(msg, wParam, lParam, false);
      return true;
  }
  return false;
}

/****************************************************************************
 * message handlers
 ****************************************************************************/

void
nsIMM32Handler::OnInputLangChange(nsWindow* aWindow,
                                  WPARAM wParam,
                                  LPARAM lParam,
                                  MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnInputLangChange, hWnd=%08x, wParam=%08x, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam, lParam));

  aWindow->NotifyIME(REQUEST_TO_COMMIT_COMPOSITION);
  NS_ASSERTION(!mIsComposing, "ResetInputState failed");

  if (mIsComposing) {
    HandleEndComposition(aWindow);
  }

  aResult.mConsumed = false;
}

bool
nsIMM32Handler::OnIMEStartComposition(nsWindow* aWindow,
                                      MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMEStartComposition, hWnd=%08x, mIsComposing=%s\n",
     aWindow->GetWindowHandle(), mIsComposing ? "TRUE" : "FALSE"));
  aResult.mConsumed = ShouldDrawCompositionStringOurselves();
  if (mIsComposing) {
    NS_WARNING("Composition has been already started");
    return true;
  }

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  HandleStartComposition(aWindow, IMEContext);
  return true;
}

bool
nsIMM32Handler::OnIMEComposition(nsWindow* aWindow,
                                 WPARAM wParam,
                                 LPARAM lParam,
                                 MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMEComposition, hWnd=%08x, lParam=%08x, mIsComposing=%s\n",
     aWindow->GetWindowHandle(), lParam, mIsComposing ? "TRUE" : "FALSE"));
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMEComposition, GCS_RESULTSTR=%s, GCS_COMPSTR=%s, GCS_COMPATTR=%s, GCS_COMPCLAUSE=%s, GCS_CURSORPOS=%s\n",
     lParam & GCS_RESULTSTR  ? "YES" : "no",
     lParam & GCS_COMPSTR    ? "YES" : "no",
     lParam & GCS_COMPATTR   ? "YES" : "no",
     lParam & GCS_COMPCLAUSE ? "YES" : "no",
     lParam & GCS_CURSORPOS  ? "YES" : "no"));

  NS_PRECONDITION(!aWindow->PluginHasFocus(),
    "OnIMEComposition should not be called when a plug-in has focus");

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  aResult.mConsumed = HandleComposition(aWindow, IMEContext, lParam);
  return true;
}

bool
nsIMM32Handler::OnIMEEndComposition(nsWindow* aWindow,
                                    MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMEEndComposition, hWnd=%08x, mIsComposing=%s\n",
     aWindow->GetWindowHandle(), mIsComposing ? "TRUE" : "FALSE"));

  aResult.mConsumed = ShouldDrawCompositionStringOurselves();
  if (!mIsComposing) {
    return true;
  }

  // Korean IME posts WM_IME_ENDCOMPOSITION first when we hit space during
  // composition. Then, we should ignore the message and commit the composition
  // string at following WM_IME_COMPOSITION.
  MSG compositionMsg;
  if (WinUtils::PeekMessage(&compositionMsg, aWindow->GetWindowHandle(),
                            WM_IME_STARTCOMPOSITION, WM_IME_COMPOSITION,
                            PM_NOREMOVE) &&
      compositionMsg.message == WM_IME_COMPOSITION &&
      IS_COMMITTING_LPARAM(compositionMsg.lParam)) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: OnIMEEndComposition, WM_IME_ENDCOMPOSITION is followed by "
       "WM_IME_COMPOSITION, ignoring the message..."));
    return true;
  }

  // Otherwise, e.g., ChangJie doesn't post WM_IME_COMPOSITION before
  // WM_IME_ENDCOMPOSITION when composition string becomes empty.
  // Then, we should dispatch a compositionupdate event, a compositionchange
  // event and a compositionend event.
  // XXX Shouldn't we dispatch the compositionchange event with actual or
  //     latest composition string?
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMEEndComposition, mCompositionString=\"%s\"%s",
     NS_ConvertUTF16toUTF8(mCompositionString).get(),
     mCompositionString.IsEmpty() ? "" : ", but canceling it..."));

  HandleEndComposition(aWindow, &EmptyString());

  return true;
}

/* static */ bool
nsIMM32Handler::OnIMEChar(nsWindow* aWindow,
                          WPARAM wParam,
                          LPARAM lParam,
                          MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMEChar, hWnd=%08x, char=%08x\n",
     aWindow->GetWindowHandle(), wParam));

  // We don't need to fire any compositionchange events from here. This method
  // will be called when the composition string of the current IME is not drawn
  // by us and some characters are committed. In that case, the committed
  // string was processed in nsWindow::OnIMEComposition already.

  // We need to consume the message so that Windows don't send two WM_CHAR msgs
  aResult.mConsumed = true;
  return true;
}

/* static */ bool
nsIMM32Handler::OnIMECompositionFull(nsWindow* aWindow,
                                     MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMECompositionFull, hWnd=%08x\n",
     aWindow->GetWindowHandle()));

  // not implement yet
  aResult.mConsumed = false;
  return true;
}

/* static */ bool
nsIMM32Handler::OnIMENotify(nsWindow* aWindow,
                            WPARAM wParam,
                            LPARAM lParam,
                            MSGResult& aResult)
{
  switch (wParam) {
    case IMN_CHANGECANDIDATE:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_CHANGECANDIDATE, lParam=%08x\n",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_CLOSECANDIDATE:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_CLOSECANDIDATE, lParam=%08x\n",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_CLOSESTATUSWINDOW:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_CLOSESTATUSWINDOW\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_GUIDELINE:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_GUIDELINE\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_OPENCANDIDATE:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_OPENCANDIDATE, lParam=%08x\n",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_OPENSTATUSWINDOW:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_OPENSTATUSWINDOW\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCANDIDATEPOS:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETCANDIDATEPOS, lParam=%08x\n",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_SETCOMPOSITIONFONT:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETCOMPOSITIONFONT\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCOMPOSITIONWINDOW:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETCOMPOSITIONWINDOW\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCONVERSIONMODE:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETCONVERSIONMODE\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETOPENSTATUS:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETOPENSTATUS\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETSENTENCEMODE:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETSENTENCEMODE\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETSTATUSWINDOWPOS:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETSTATUSWINDOWPOS\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_PRIVATE:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_PRIVATE\n",
         aWindow->GetWindowHandle()));
      break;
  }

  // not implement yet
  aResult.mConsumed = false;
  return true;
}

bool
nsIMM32Handler::OnIMERequest(nsWindow* aWindow,
                             WPARAM wParam,
                             LPARAM lParam,
                             MSGResult& aResult)
{
  switch (wParam) {
    case IMR_RECONVERTSTRING:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMERequest, hWnd=%08x, IMR_RECONVERTSTRING\n",
         aWindow->GetWindowHandle()));
      aResult.mConsumed = HandleReconvert(aWindow, lParam, &aResult.mResult);
      return true;
    case IMR_QUERYCHARPOSITION:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMERequest, hWnd=%08x, IMR_QUERYCHARPOSITION\n",
         aWindow->GetWindowHandle()));
      aResult.mConsumed =
        HandleQueryCharPosition(aWindow, lParam, &aResult.mResult);
      return true;
    case IMR_DOCUMENTFEED:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMERequest, hWnd=%08x, IMR_DOCUMENTFEED\n",
         aWindow->GetWindowHandle()));
      aResult.mConsumed = HandleDocumentFeed(aWindow, lParam, &aResult.mResult);
      return true;
    default:
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: OnIMERequest, hWnd=%08x, wParam=%08x\n",
         aWindow->GetWindowHandle(), wParam));
      aResult.mConsumed = false;
      return true;
  }
}

/* static */ bool
nsIMM32Handler::OnIMESelect(nsWindow* aWindow,
                            WPARAM wParam,
                            LPARAM lParam,
                            MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMESelect, hWnd=%08x, wParam=%08x, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam, lParam));

  // not implement yet
  aResult.mConsumed = false;
  return true;
}

/* static */ bool
nsIMM32Handler::OnIMESetContext(nsWindow* aWindow,
                                WPARAM wParam,
                                LPARAM lParam,
                                MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMESetContext, hWnd=%08x, %s, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam ? "Active" : "Deactive", lParam));

  aResult.mConsumed = false;

  // NOTE: If the aWindow is top level window of the composing window because
  // when a window on deactive window gets focus, WM_IME_SETCONTEXT (wParam is
  // TRUE) is sent to the top level window first.  After that,
  // WM_IME_SETCONTEXT (wParam is FALSE) is sent to the top level window.
  // Finally, WM_IME_SETCONTEXT (wParam is TRUE) is sent to the focused window.
  // The top level window never becomes composing window, so, we can ignore
  // the WM_IME_SETCONTEXT on the top level window.
  if (IsTopLevelWindowOfComposition(aWindow)) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: OnIMESetContext, hWnd=%08x is top level window\n"));
    return true;
  }

  // When IME context is activating on another window,
  // we should commit the old composition on the old window.
  bool cancelComposition = false;
  if (wParam && gIMM32Handler) {
    cancelComposition =
      gIMM32Handler->CommitCompositionOnPreviousWindow(aWindow);
  }

  if (wParam && (lParam & ISC_SHOWUICOMPOSITIONWINDOW) &&
      ShouldDrawCompositionStringOurselves()) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: OnIMESetContext, ISC_SHOWUICOMPOSITIONWINDOW is removed\n"));
    lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
  }

  // We should sent WM_IME_SETCONTEXT to the DefWndProc here because the
  // ancestor windows shouldn't receive this message.  If they receive the
  // message, we cannot know whether which window is the target of the message.
  aResult.mResult = ::DefWindowProc(aWindow->GetWindowHandle(),
                                    WM_IME_SETCONTEXT, wParam, lParam);

  // Cancel composition on the new window if we committed our composition on
  // another window.
  if (cancelComposition) {
    CancelComposition(aWindow, true);
  }

  aResult.mConsumed = true;
  return true;
}

bool
nsIMM32Handler::OnChar(nsWindow* aWindow,
                       WPARAM wParam,
                       LPARAM lParam,
                       MSGResult& aResult)
{
  // The return value must be same as aResult.mConsumed because only when we
  // consume the message, the caller shouldn't do anything anymore but
  // otherwise, the caller should handle the message.
  aResult.mConsumed = false;
  if (IsIMECharRecordsEmpty()) {
    return aResult.mConsumed;
  }
  WPARAM recWParam;
  LPARAM recLParam;
  DequeueIMECharRecords(recWParam, recLParam);
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnChar, aWindow=%p, wParam=%08x, lParam=%08x,\n",
     aWindow->GetWindowHandle(), wParam, lParam));
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("               recorded: wParam=%08x, lParam=%08x\n",
     recWParam, recLParam));
  // If an unexpected char message comes, we should reset the records,
  // of course, this shouldn't happen.
  if (recWParam != wParam || recLParam != lParam) {
    ResetIMECharRecords();
    return aResult.mConsumed;
  }
  // Eat the char message which is caused by WM_IME_CHAR because we should
  // have processed the IME messages, so, this message could be come from
  // a windowless plug-in.
  aResult.mConsumed = true;
  return aResult.mConsumed;
}

/****************************************************************************
 * message handlers for plug-in
 ****************************************************************************/

bool
nsIMM32Handler::OnIMEStartCompositionOnPlugin(nsWindow* aWindow,
                                              WPARAM wParam,
                                              LPARAM lParam,
                                              MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMEStartCompositionOnPlugin, hWnd=%08x, mIsComposingOnPlugin=%s\n",
     aWindow->GetWindowHandle(), mIsComposingOnPlugin ? "TRUE" : "FALSE"));
  mIsComposingOnPlugin = true;
  mComposingWindow = aWindow;
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  SetIMERelatedWindowsPosOnPlugin(aWindow, IMEContext);
  // On widnowless plugin, we should assume that the focused editor is always
  // in horizontal writing mode.
  AdjustCompositionFont(IMEContext, WritingMode());
  aResult.mConsumed =
    aWindow->DispatchPluginEvent(WM_IME_STARTCOMPOSITION, wParam, lParam,
                                 false);
  return true;
}

bool
nsIMM32Handler::OnIMECompositionOnPlugin(nsWindow* aWindow,
                                         WPARAM wParam,
                                         LPARAM lParam,
                                         MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMECompositionOnPlugin, hWnd=%08x, lParam=%08x, mIsComposingOnPlugin=%s\n",
     aWindow->GetWindowHandle(), lParam,
     mIsComposingOnPlugin ? "TRUE" : "FALSE"));
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMECompositionOnPlugin, GCS_RESULTSTR=%s, GCS_COMPSTR=%s, GCS_COMPATTR=%s, GCS_COMPCLAUSE=%s, GCS_CURSORPOS=%s\n",
     lParam & GCS_RESULTSTR  ? "YES" : "no",
     lParam & GCS_COMPSTR    ? "YES" : "no",
     lParam & GCS_COMPATTR   ? "YES" : "no",
     lParam & GCS_COMPCLAUSE ? "YES" : "no",
     lParam & GCS_CURSORPOS  ? "YES" : "no"));
  // We should end composition if there is a committed string.
  if (IS_COMMITTING_LPARAM(lParam)) {
    mIsComposingOnPlugin = false;
    mComposingWindow = nullptr;
  }
  // Continue composition if there is still a string being composed.
  if (IS_COMPOSING_LPARAM(lParam)) {
    mIsComposingOnPlugin = true;
    mComposingWindow = aWindow;
    nsIMEContext IMEContext(aWindow->GetWindowHandle());
    SetIMERelatedWindowsPosOnPlugin(aWindow, IMEContext);
  }
  aResult.mConsumed =
    aWindow->DispatchPluginEvent(WM_IME_COMPOSITION, wParam, lParam, true);
  return true;
}

bool
nsIMM32Handler::OnIMEEndCompositionOnPlugin(nsWindow* aWindow,
                                            WPARAM wParam,
                                            LPARAM lParam,
                                            MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMEEndCompositionOnPlugin, hWnd=%08x, mIsComposingOnPlugin=%s\n",
     aWindow->GetWindowHandle(), mIsComposingOnPlugin ? "TRUE" : "FALSE"));

  mIsComposingOnPlugin = false;
  mComposingWindow = nullptr;

  if (mNativeCaretIsCreated) {
    ::DestroyCaret();
    mNativeCaretIsCreated = false;
  }

  aResult.mConsumed =
    aWindow->DispatchPluginEvent(WM_IME_ENDCOMPOSITION, wParam, lParam,
                                 false);
  return true;
}

bool
nsIMM32Handler::OnIMECharOnPlugin(nsWindow* aWindow,
                                  WPARAM wParam,
                                  LPARAM lParam,
                                  MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMECharOnPlugin, hWnd=%08x, char=%08x, scancode=%08x\n",
     aWindow->GetWindowHandle(), wParam, lParam));

  aResult.mConsumed =
    aWindow->DispatchPluginEvent(WM_IME_CHAR, wParam, lParam, true);

  if (!aResult.mConsumed) {
    // Record the WM_CHAR messages which are going to be coming.
    EnsureHandlerInstance();
    EnqueueIMECharRecords(wParam, lParam);
  }
  return true;
}

/* static */ bool
nsIMM32Handler::OnIMESetContextOnPlugin(nsWindow* aWindow,
                                        WPARAM wParam,
                                        LPARAM lParam,
                                        MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnIMESetContextOnPlugin, hWnd=%08x, %s, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam ? "Active" : "Deactive", lParam));

  // If the IME context becomes active on a plug-in, we should commit
  // our composition.  And also we should cancel the composition on new
  // window.  Note that if IsTopLevelWindowOfComposition(aWindow) returns
  // true, we should ignore the message here, see the comment in
  // OnIMESetContext() for the detail.
  if (wParam && gIMM32Handler && !IsTopLevelWindowOfComposition(aWindow)) {
    if (gIMM32Handler->CommitCompositionOnPreviousWindow(aWindow)) {
      CancelComposition(aWindow);
    }
  }

  // Dispatch message to the plug-in.
  // XXX When a windowless plug-in gets focus, we should send
  //     WM_IME_SETCONTEXT
  aWindow->DispatchPluginEvent(WM_IME_SETCONTEXT, wParam, lParam, false);

  // We should send WM_IME_SETCONTEXT to the DefWndProc here.  It shouldn't
  // be received on ancestor windows, see OnIMESetContext() for the detail.
  aResult.mResult = ::DefWindowProc(aWindow->GetWindowHandle(),
                                    WM_IME_SETCONTEXT, wParam, lParam);

  // Don't synchronously dispatch the pending events when we receive
  // WM_IME_SETCONTEXT because we get it during plugin destruction.
  // (bug 491848)
  aResult.mConsumed = true;
  return true;
}

bool
nsIMM32Handler::OnCharOnPlugin(nsWindow* aWindow,
                               WPARAM wParam,
                               LPARAM lParam,
                               MSGResult& aResult)
{
  // We should never consume char message on windowless plugin.
  aResult.mConsumed = false;
  if (IsIMECharRecordsEmpty()) {
    return false;
  }

  WPARAM recWParam;
  LPARAM recLParam;
  DequeueIMECharRecords(recWParam, recLParam);
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnCharOnPlugin, aWindow=%p, wParam=%08x, lParam=%08x,\n",
     aWindow->GetWindowHandle(), wParam, lParam));
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("                       recorded: wParam=%08x, lParam=%08x\n",
     recWParam, recLParam));
  // If an unexpected char message comes, we should reset the records,
  // of course, this shouldn't happen.
  if (recWParam != wParam || recLParam != lParam) {
    ResetIMECharRecords();
  }
  // WM_CHAR on plug-in is always handled by nsWindow.
  return false;
}

/****************************************************************************
 * others
 ****************************************************************************/

void
nsIMM32Handler::HandleStartComposition(nsWindow* aWindow,
                                       const nsIMEContext &aIMEContext)
{
  NS_PRECONDITION(!mIsComposing,
    "HandleStartComposition is called but mIsComposing is TRUE");
  NS_PRECONDITION(!aWindow->PluginHasFocus(),
    "HandleStartComposition should not be called when a plug-in has focus");

  WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, aWindow);
  nsIntPoint point(0, 0);
  aWindow->InitEvent(selection, &point);
  aWindow->DispatchWindowEvent(&selection);
  if (!selection.mSucceeded) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleStartComposition, FAILED (NS_QUERY_SELECTED_TEXT)\n"));
    return;
  }

  AdjustCompositionFont(aIMEContext, selection.GetWritingMode());

  mCompositionStart = selection.mReply.mOffset;

  WidgetCompositionEvent event(true, NS_COMPOSITION_START, aWindow);
  aWindow->InitEvent(event, &point);
  aWindow->DispatchWindowEvent(&event);

  mIsComposing = true;
  mComposingWindow = aWindow;

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleStartComposition, START composition, mCompositionStart=%ld\n",
     mCompositionStart));
}

bool
nsIMM32Handler::HandleComposition(nsWindow* aWindow,
                                  const nsIMEContext &aIMEContext,
                                  LPARAM lParam)
{
  NS_PRECONDITION(!aWindow->PluginHasFocus(),
    "HandleComposition should not be called when a plug-in has focus");

  // for bug #60050
  // MS-IME 95/97/98/2000 may send WM_IME_COMPOSITION with non-conversion
  // mode before it send WM_IME_STARTCOMPOSITION.
  // However, ATOK sends a WM_IME_COMPOSITION before WM_IME_STARTCOMPOSITION,
  // and if we access ATOK via some APIs, ATOK will sometimes fail to
  // initialize its state.  If WM_IME_STARTCOMPOSITION is already in the
  // message queue, we should ignore the strange WM_IME_COMPOSITION message and
  // skip to the next.  So, we should look for next composition message
  // (WM_IME_STARTCOMPOSITION or WM_IME_ENDCOMPOSITION or WM_IME_COMPOSITION),
  // and if it's WM_IME_STARTCOMPOSITION, and one more next composition message
  // is WM_IME_COMPOSITION, current IME is ATOK, probably.  Otherwise, we
  // should start composition forcibly.
  if (!mIsComposing) {
    MSG msg1, msg2;
    HWND wnd = aWindow->GetWindowHandle();
    if (WinUtils::PeekMessage(&msg1, wnd, WM_IME_STARTCOMPOSITION,
                              WM_IME_COMPOSITION, PM_NOREMOVE) &&
        msg1.message == WM_IME_STARTCOMPOSITION &&
        WinUtils::PeekMessage(&msg2, wnd, WM_IME_ENDCOMPOSITION,
                              WM_IME_COMPOSITION, PM_NOREMOVE) &&
        msg2.message == WM_IME_COMPOSITION) {
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: HandleComposition, Ignores due to find a WM_IME_STARTCOMPOSITION\n"));
      return ShouldDrawCompositionStringOurselves();
    }
  }

  bool startCompositionMessageHasBeenSent = mIsComposing;

  //
  // This catches a fixed result
  //
  if (IS_COMMITTING_LPARAM(lParam)) {
    if (!mIsComposing) {
      HandleStartComposition(aWindow, aIMEContext);
    }

    GetCompositionString(aIMEContext, GCS_RESULTSTR, mCompositionString);

    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleComposition, GCS_RESULTSTR\n"));

    HandleEndComposition(aWindow, &mCompositionString);

    if (!IS_COMPOSING_LPARAM(lParam)) {
      return ShouldDrawCompositionStringOurselves();
    }
  }


  //
  // This provides us with a composition string
  //
  if (!mIsComposing) {
    HandleStartComposition(aWindow, aIMEContext);
  }

  //--------------------------------------------------------
  // 1. Get GCS_COMPSTR
  //--------------------------------------------------------
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleComposition, GCS_COMPSTR\n"));

  nsAutoString previousCompositionString(mCompositionString);
  GetCompositionString(aIMEContext, GCS_COMPSTR, mCompositionString);

  if (!IS_COMPOSING_LPARAM(lParam)) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleComposition, lParam doesn't indicate composing, "
       "mCompositionString=\"%s\", previousCompositionString=\"%s\"",
       NS_ConvertUTF16toUTF8(mCompositionString).get(),
       NS_ConvertUTF16toUTF8(previousCompositionString).get()));

    // If composition string isn't changed, we can trust the lParam.
    // So, we need to do nothing.
    if (previousCompositionString == mCompositionString) {
      return ShouldDrawCompositionStringOurselves();
    }

    // IME may send WM_IME_COMPOSITION without composing lParam values
    // when composition string becomes empty (e.g., using Backspace key).
    // If composition string is empty, we should dispatch a compositionchange
    // event with empty string and clear the clause information.
    if (mCompositionString.IsEmpty()) {
      mClauseArray.Clear();
      mAttributeArray.Clear();
      mCursorPosition = 0;
      DispatchCompositionChangeEvent(aWindow, aIMEContext);
      return ShouldDrawCompositionStringOurselves();
    }

    // Otherwise, we cannot trust the lParam value.  We might need to
    // dispatch compositionchange event with the latest composition string
    // information.
  }

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=296339
  if (mCompositionString.IsEmpty() && !startCompositionMessageHasBeenSent) {
    // In this case, maybe, the sender is MSPinYin. That sends *only*
    // WM_IME_COMPOSITION with GCS_COMP* and GCS_RESULT* when
    // user inputted the Chinese full stop. So, that doesn't send
    // WM_IME_STARTCOMPOSITION and WM_IME_ENDCOMPOSITION.
    // If WM_IME_STARTCOMPOSITION was not sent and the composition
    // string is null (it indicates the composition transaction ended),
    // WM_IME_ENDCOMPOSITION may not be sent. If so, we cannot run
    // HandleEndComposition() in other place.
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleComposition, Aborting GCS_COMPSTR\n"));
    HandleEndComposition(aWindow);
    return IS_COMMITTING_LPARAM(lParam);
  }

  //--------------------------------------------------------
  // 2. Get GCS_COMPCLAUSE
  //--------------------------------------------------------
  long clauseArrayLength =
    ::ImmGetCompositionStringW(aIMEContext.get(), GCS_COMPCLAUSE, nullptr, 0);
  clauseArrayLength /= sizeof(uint32_t);

  if (clauseArrayLength > 0) {
    nsresult rv = EnsureClauseArray(clauseArrayLength);
    NS_ENSURE_SUCCESS(rv, false);

    // Intelligent ABC IME (Simplified Chinese IME, the code page is 936)
    // will crash in ImmGetCompositionStringW for GCS_COMPCLAUSE (bug 424663).
    // See comment 35 of the bug for the detail. Therefore, we should use A
    // API for it, however, we should not kill Unicode support on all IMEs.
    bool useA_API = !(sIMEProperty & IME_PROP_UNICODE);

    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleComposition, GCS_COMPCLAUSE, useA_API=%s\n",
       useA_API ? "TRUE" : "FALSE"));

    long clauseArrayLength2 = 
      useA_API ?
        ::ImmGetCompositionStringA(aIMEContext.get(), GCS_COMPCLAUSE,
                                   mClauseArray.Elements(),
                                   mClauseArray.Capacity() * sizeof(uint32_t)) :
        ::ImmGetCompositionStringW(aIMEContext.get(), GCS_COMPCLAUSE,
                                   mClauseArray.Elements(),
                                   mClauseArray.Capacity() * sizeof(uint32_t));
    clauseArrayLength2 /= sizeof(uint32_t);

    if (clauseArrayLength != clauseArrayLength2) {
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: HandleComposition, GCS_COMPCLAUSE, clauseArrayLength=%ld but clauseArrayLength2=%ld\n",
         clauseArrayLength, clauseArrayLength2));
      if (clauseArrayLength > clauseArrayLength2)
        clauseArrayLength = clauseArrayLength2;
    }

    if (useA_API) {
      // Convert each values of sIMECompClauseArray. The values mean offset of
      // the clauses in ANSI string. But we need the values in Unicode string.
      nsAutoCString compANSIStr;
      if (ConvertToANSIString(mCompositionString, GetKeyboardCodePage(),
                              compANSIStr)) {
        uint32_t maxlen = compANSIStr.Length();
        mClauseArray[0] = 0; // first value must be 0
        for (int32_t i = 1; i < clauseArrayLength; i++) {
          uint32_t len = std::min(mClauseArray[i], maxlen);
          mClauseArray[i] = ::MultiByteToWideChar(GetKeyboardCodePage(), 
                                                  MB_PRECOMPOSED,
                                                  (LPCSTR)compANSIStr.get(),
                                                  len, nullptr, 0);
        }
      }
    }
  }
  // compClauseArrayLength may be negative. I.e., ImmGetCompositionStringW
  // may return an error code.
  mClauseArray.SetLength(std::max<long>(0, clauseArrayLength));

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleComposition, GCS_COMPCLAUSE, mClauseLength=%ld\n",
     mClauseArray.Length()));

  //--------------------------------------------------------
  // 3. Get GCS_COMPATTR
  //--------------------------------------------------------
  // This provides us with the attribute string necessary 
  // for doing hiliting
  long attrArrayLength =
    ::ImmGetCompositionStringW(aIMEContext.get(), GCS_COMPATTR, nullptr, 0);
  attrArrayLength /= sizeof(uint8_t);

  if (attrArrayLength > 0) {
    nsresult rv = EnsureAttributeArray(attrArrayLength);
    NS_ENSURE_SUCCESS(rv, false);
    attrArrayLength =
      ::ImmGetCompositionStringW(aIMEContext.get(), GCS_COMPATTR,
                                 mAttributeArray.Elements(),
                                 mAttributeArray.Capacity() * sizeof(uint8_t));
  }

  // attrStrLen may be negative. I.e., ImmGetCompositionStringW may return an
  // error code.
  mAttributeArray.SetLength(std::max<long>(0, attrArrayLength));

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleComposition, GCS_COMPATTR, mAttributeLength=%ld\n",
     mAttributeArray.Length()));

  //--------------------------------------------------------
  // 4. Get GCS_CURSOPOS
  //--------------------------------------------------------
  // Some IMEs (e.g., the standard IME for Korean) don't have caret position.
  if (lParam & GCS_CURSORPOS) {
    mCursorPosition =
      ::ImmGetCompositionStringW(aIMEContext.get(), GCS_CURSORPOS, nullptr, 0);
    if (mCursorPosition < 0) {
      mCursorPosition = NO_IME_CARET; // The result is error
    }
  } else {
    mCursorPosition = NO_IME_CARET;
  }

  NS_ASSERTION(mCursorPosition <= (long)mCompositionString.Length(),
               "illegal pos");

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleComposition, GCS_CURSORPOS, mCursorPosition=%d\n",
     mCursorPosition));

  //--------------------------------------------------------
  // 5. Send the compositionchange event
  //--------------------------------------------------------
  DispatchCompositionChangeEvent(aWindow, aIMEContext);

  return ShouldDrawCompositionStringOurselves();
}

void
nsIMM32Handler::HandleEndComposition(nsWindow* aWindow,
                                     const nsAString* aCommitString)
{
  NS_PRECONDITION(mIsComposing,
    "HandleEndComposition is called but mIsComposing is FALSE");
  NS_PRECONDITION(!aWindow->PluginHasFocus(),
    "HandleComposition should not be called when a plug-in has focus");

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleEndComposition(aWindow=0x%p, aCommitString=0x%p (\"%s\"))",
     aWindow, aCommitString,
     aCommitString ? NS_ConvertUTF16toUTF8(*aCommitString).get() : ""));

  if (mNativeCaretIsCreated) {
    ::DestroyCaret();
    mNativeCaretIsCreated = false;
  }

  uint32_t message =
    aCommitString ? NS_COMPOSITION_COMMIT : NS_COMPOSITION_COMMIT_AS_IS;
  WidgetCompositionEvent compositionCommitEvent(true, message, aWindow);
  nsIntPoint point(0, 0);
  aWindow->InitEvent(compositionCommitEvent, &point);
  if (aCommitString) {
    compositionCommitEvent.mData = *aCommitString;
  }
  aWindow->DispatchWindowEvent(&compositionCommitEvent);
  mIsComposing = false;
  mComposingWindow = nullptr;
}

static void
DumpReconvertString(RECONVERTSTRING* aReconv)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("  dwSize=%ld, dwVersion=%ld, dwStrLen=%ld, dwStrOffset=%ld\n",
     aReconv->dwSize, aReconv->dwVersion,
     aReconv->dwStrLen, aReconv->dwStrOffset));
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("  dwCompStrLen=%ld, dwCompStrOffset=%ld, dwTargetStrLen=%ld, dwTargetStrOffset=%ld\n",
     aReconv->dwCompStrLen, aReconv->dwCompStrOffset,
     aReconv->dwTargetStrLen, aReconv->dwTargetStrOffset));
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("  result str=\"%s\"\n",
     NS_ConvertUTF16toUTF8(
       nsAutoString((char16_t*)((char*)(aReconv) + aReconv->dwStrOffset),
                    aReconv->dwStrLen)).get()));
}

bool
nsIMM32Handler::HandleReconvert(nsWindow* aWindow,
                                LPARAM lParam,
                                LRESULT *oResult)
{
  *oResult = 0;
  RECONVERTSTRING* pReconv = reinterpret_cast<RECONVERTSTRING*>(lParam);

  WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, aWindow);
  nsIntPoint point(0, 0);
  aWindow->InitEvent(selection, &point);
  aWindow->DispatchWindowEvent(&selection);
  if (!selection.mSucceeded) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleReconvert, FAILED (NS_QUERY_SELECTED_TEXT)\n"));
    return false;
  }

  uint32_t len = selection.mReply.mString.Length();
  uint32_t needSize = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!pReconv) {
    // Return need size to reconvert.
    if (len == 0) {
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: HandleReconvert, There are not selected text\n"));
      return false;
    }
    *oResult = needSize;
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleReconvert, SUCCEEDED result=%ld\n",
       *oResult));
    return true;
  }

  if (pReconv->dwSize < needSize) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleReconvert, FAILED pReconv->dwSize=%ld, needSize=%ld\n",
       pReconv->dwSize, needSize));
    return false;
  }

  *oResult = needSize;

  // Fill reconvert struct
  pReconv->dwVersion         = 0;
  pReconv->dwStrLen          = len;
  pReconv->dwStrOffset       = sizeof(RECONVERTSTRING);
  pReconv->dwCompStrLen      = len;
  pReconv->dwCompStrOffset   = 0;
  pReconv->dwTargetStrLen    = len;
  pReconv->dwTargetStrOffset = 0;

  ::CopyMemory(reinterpret_cast<LPVOID>(lParam + sizeof(RECONVERTSTRING)),
               selection.mReply.mString.get(), len * sizeof(WCHAR));

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleReconvert, SUCCEEDED result=%ld\n",
     *oResult));
  DumpReconvertString(pReconv);

  return true;
}

bool
nsIMM32Handler::HandleQueryCharPosition(nsWindow* aWindow,
                                        LPARAM lParam,
                                        LRESULT *oResult)
{
  uint32_t len = mIsComposing ? mCompositionString.Length() : 0;
  *oResult = false;
  IMECHARPOSITION* pCharPosition = reinterpret_cast<IMECHARPOSITION*>(lParam);
  if (!pCharPosition) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleQueryCharPosition, FAILED (pCharPosition is null)\n"));
    return false;
  }
  if (pCharPosition->dwSize < sizeof(IMECHARPOSITION)) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleReconvert, FAILED, pCharPosition->dwSize=%ld, sizeof(IMECHARPOSITION)=%ld\n",
       pCharPosition->dwSize, sizeof(IMECHARPOSITION)));
    return false;
  }
  if (::GetFocus() != aWindow->GetWindowHandle()) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleReconvert, FAILED, ::GetFocus()=%08x, OurWindowHandle=%08x\n",
       ::GetFocus(), aWindow->GetWindowHandle()));
    return false;
  }
  if (pCharPosition->dwCharPos > len) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleQueryCharPosition, FAILED, pCharPosition->dwCharPos=%ld, len=%ld\n",
      pCharPosition->dwCharPos, len));
    return false;
  }

  nsIntRect r;
  bool ret =
    GetCharacterRectOfSelectedTextAt(aWindow, pCharPosition->dwCharPos, r);
  NS_ENSURE_TRUE(ret, false);

  nsIntRect screenRect;
  // We always need top level window that is owner window of the popup window
  // even if the content of the popup window has focus.
  ResolveIMECaretPos(aWindow->GetTopLevelWindow(false),
                     r, nullptr, screenRect);

  // XXX This might need to check writing mode.  However, MSDN doesn't explain
  //     how to set the values in vertical writing mode. Additionally, IME
  //     doesn't work well with top-left of the character (this is explicitly
  //     documented) and its horizontal width.  So, it might be better to set
  //     top-right corner of the character and horizontal width, but we're not
  //     sure if it doesn't cause any problems with a lot of IMEs...
  pCharPosition->pt.x = screenRect.x;
  pCharPosition->pt.y = screenRect.y;

  pCharPosition->cLineHeight = r.height;

  WidgetQueryContentEvent editorRect(true, NS_QUERY_EDITOR_RECT, aWindow);
  aWindow->InitEvent(editorRect);
  aWindow->DispatchWindowEvent(&editorRect);
  if (NS_WARN_IF(!editorRect.mSucceeded)) {
    MOZ_LOG(gIMM32Log, LogLevel::Error,
      ("IMM32: HandleQueryCharPosition, NS_QUERY_EDITOR_RECT failed"));
    ::GetWindowRect(aWindow->GetWindowHandle(), &pCharPosition->rcDocument);
  } else {
    nsIntRect editorRectInWindow =
      LayoutDevicePixel::ToUntyped(editorRect.mReply.mRect);
    nsWindow* window = editorRect.mReply.mFocusedWidget ?
      static_cast<nsWindow*>(editorRect.mReply.mFocusedWidget) : aWindow;
    nsIntRect editorRectInScreen;
    ResolveIMECaretPos(window, editorRectInWindow, nullptr, editorRectInScreen);
    ::SetRect(&pCharPosition->rcDocument,
              editorRectInScreen.x, editorRectInScreen.y,
              editorRectInScreen.XMost(), editorRectInScreen.YMost());
  }

  *oResult = TRUE;

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleQueryCharPosition, SUCCEEDED, pCharPosition={ pt={ x=%d, "
     "y=%d }, cLineHeight=%d, rcDocument={ left=%d, top=%d, right=%d, "
     "bottom=%d } }",
     pCharPosition->pt.x, pCharPosition->pt.y, pCharPosition->cLineHeight,
     pCharPosition->rcDocument.left, pCharPosition->rcDocument.top,
     pCharPosition->rcDocument.right, pCharPosition->rcDocument.bottom));
  return true;
}

bool
nsIMM32Handler::HandleDocumentFeed(nsWindow* aWindow,
                                   LPARAM lParam,
                                   LRESULT *oResult)
{
  *oResult = 0;
  RECONVERTSTRING* pReconv = reinterpret_cast<RECONVERTSTRING*>(lParam);

  nsIntPoint point(0, 0);

  bool hasCompositionString =
    mIsComposing && ShouldDrawCompositionStringOurselves();

  int32_t targetOffset, targetLength;
  if (!hasCompositionString) {
    WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, aWindow);
    aWindow->InitEvent(selection, &point);
    aWindow->DispatchWindowEvent(&selection);
    if (!selection.mSucceeded) {
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: HandleDocumentFeed, FAILED (NS_QUERY_SELECTED_TEXT)\n"));
      return false;
    }
    targetOffset = int32_t(selection.mReply.mOffset);
    targetLength = int32_t(selection.mReply.mString.Length());
  } else {
    targetOffset = int32_t(mCompositionStart);
    targetLength = int32_t(mCompositionString.Length());
  }

  // XXX nsString::Find and nsString::RFind take int32_t for offset, so,
  //     we cannot support this message when the current offset is larger than
  //     INT32_MAX.
  if (targetOffset < 0 || targetLength < 0 ||
      targetOffset + targetLength < 0) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleDocumentFeed, FAILED (The selection is out of range)\n"));
    return false;
  }

  // Get all contents of the focused editor.
  WidgetQueryContentEvent textContent(true, NS_QUERY_TEXT_CONTENT, aWindow);
  textContent.InitForQueryTextContent(0, UINT32_MAX);
  aWindow->InitEvent(textContent, &point);
  aWindow->DispatchWindowEvent(&textContent);
  if (!textContent.mSucceeded) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleDocumentFeed, FAILED (NS_QUERY_TEXT_CONTENT)\n"));
    return false;
  }

  nsAutoString str(textContent.mReply.mString);
  if (targetOffset > int32_t(str.Length())) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleDocumentFeed, FAILED (The caret offset is invalid)\n"));
    return false;
  }

  // Get the focused paragraph, we decide that it starts from the previous CRLF
  // (or start of the editor) to the next one (or the end of the editor).
  int32_t paragraphStart = str.RFind("\n", false, targetOffset, -1) + 1;
  int32_t paragraphEnd =
    str.Find("\r", false, targetOffset + targetLength, -1);
  if (paragraphEnd < 0) {
    paragraphEnd = str.Length();
  }
  nsDependentSubstring paragraph(str, paragraphStart,
                                 paragraphEnd - paragraphStart);

  uint32_t len = paragraph.Length();
  uint32_t needSize = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!pReconv) {
    *oResult = needSize;
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleDocumentFeed, SUCCEEDED result=%ld\n",
       *oResult));
    return true;
  }

  if (pReconv->dwSize < needSize) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: HandleDocumentFeed, FAILED pReconv->dwSize=%ld, needSize=%ld\n",
       pReconv->dwSize, needSize));
    return false;
  }

  // Fill reconvert struct
  pReconv->dwVersion         = 0;
  pReconv->dwStrLen          = len;
  pReconv->dwStrOffset       = sizeof(RECONVERTSTRING);
  if (hasCompositionString) {
    pReconv->dwCompStrLen      = targetLength;
    pReconv->dwCompStrOffset   =
      (targetOffset - paragraphStart) * sizeof(WCHAR);
    // Set composition target clause information
    uint32_t offset, length;
    if (!GetTargetClauseRange(&offset, &length)) {
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: HandleDocumentFeed, FAILED, by GetTargetClauseRange\n"));
      return false;
    }
    pReconv->dwTargetStrLen    = length;
    pReconv->dwTargetStrOffset = (offset - paragraphStart) * sizeof(WCHAR);
  } else {
    pReconv->dwTargetStrLen    = targetLength;
    pReconv->dwTargetStrOffset =
      (targetOffset - paragraphStart) * sizeof(WCHAR);
    // There is no composition string, so, the length is zero but we should
    // set the cursor offset to the composition str offset.
    pReconv->dwCompStrLen      = 0;
    pReconv->dwCompStrOffset   = pReconv->dwTargetStrOffset;
  }

  *oResult = needSize;
  ::CopyMemory(reinterpret_cast<LPVOID>(lParam + sizeof(RECONVERTSTRING)),
               paragraph.BeginReading(), len * sizeof(WCHAR));

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: HandleDocumentFeed, SUCCEEDED result=%ld\n",
     *oResult));
  DumpReconvertString(pReconv);

  return true;
}

bool
nsIMM32Handler::CommitCompositionOnPreviousWindow(nsWindow* aWindow)
{
  if (!mComposingWindow || mComposingWindow == aWindow) {
    return false;
  }

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: CommitCompositionOnPreviousWindow, mIsComposing=%s, mIsComposingOnPlugin=%s\n",
     mIsComposing ? "TRUE" : "FALSE", mIsComposingOnPlugin ? "TRUE" : "FALSE"));

  // If we have composition, we should dispatch composition events internally.
  if (mIsComposing) {
    nsIMEContext IMEContext(mComposingWindow->GetWindowHandle());
    NS_ASSERTION(IMEContext.IsValid(), "IME context must be valid");

    HandleEndComposition(mComposingWindow);
    return true;
  }

  // XXX When plug-in has composition, we should commit composition on the
  // plug-in.  However, we need some more work for that.
  return mIsComposingOnPlugin;
}

static uint32_t
PlatformToNSAttr(uint8_t aAttr)
{
  switch (aAttr)
  {
    case ATTR_INPUT_ERROR:
    // case ATTR_FIXEDCONVERTED:
    case ATTR_INPUT:
      return NS_TEXTRANGE_RAWINPUT;
    case ATTR_CONVERTED:
      return NS_TEXTRANGE_CONVERTEDTEXT;
    case ATTR_TARGET_NOTCONVERTED:
      return NS_TEXTRANGE_SELECTEDRAWTEXT;
    case ATTR_TARGET_CONVERTED:
      return NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
    default:
      NS_ASSERTION(false, "unknown attribute");
      return NS_TEXTRANGE_CARETPOSITION;
  }
}

static const char*
GetRangeTypeName(uint32_t aRangeType)
{
  switch (aRangeType) {
    case NS_TEXTRANGE_RAWINPUT:
      return "NS_TEXTRANGE_RAWINPUT";
    case NS_TEXTRANGE_CONVERTEDTEXT:
      return "NS_TEXTRANGE_CONVERTEDTEXT";
    case NS_TEXTRANGE_SELECTEDRAWTEXT:
      return "NS_TEXTRANGE_SELECTEDRAWTEXT";
    case NS_TEXTRANGE_SELECTEDCONVERTEDTEXT:
      return "NS_TEXTRANGE_SELECTEDCONVERTEDTEXT";
    case NS_TEXTRANGE_CARETPOSITION:
      return "NS_TEXTRANGE_CARETPOSITION";
    default:
      return "UNKNOWN SELECTION TYPE!!";
  }
}

void
nsIMM32Handler::DispatchCompositionChangeEvent(nsWindow* aWindow,
                                               const nsIMEContext& aIMEContext)
{
  NS_ASSERTION(mIsComposing, "conflict state");
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: DispatchCompositionChangeEvent"));

  // If we don't need to draw composition string ourselves, we don't need to
  // fire compositionchange event during composing.
  if (!ShouldDrawCompositionStringOurselves()) {
    // But we need to adjust composition window pos and native caret pos, here.
    SetIMERelatedWindowsPos(aWindow, aIMEContext);
    return;
  }

  nsRefPtr<nsWindow> kungFuDeathGrip(aWindow);

  nsIntPoint point(0, 0);

  WidgetCompositionEvent event(true, NS_COMPOSITION_CHANGE, aWindow);

  aWindow->InitEvent(event, &point);

  event.mRanges = CreateTextRangeArray();
  event.mData = mCompositionString;

  aWindow->DispatchWindowEvent(&event);

  // Calling SetIMERelatedWindowsPos will be failure on e10s at this point.
  // compositionchange event will notify NOTIFY_IME_OF_COMPOSITION_UPDATE, then
  // it will call SetIMERelatedWindowsPos.
}

already_AddRefed<TextRangeArray>
nsIMM32Handler::CreateTextRangeArray()
{
  // Sogou (Simplified Chinese IME) returns contradictory values: The cursor
  // position is actual cursor position. However, other values (composition
  // string and attributes) are empty. So, if you want to remove following
  // assertion, be careful.
  NS_ASSERTION(ShouldDrawCompositionStringOurselves(),
    "CreateTextRangeArray is called when we don't need to fire "
    "compositionchange event");

  nsRefPtr<TextRangeArray> textRangeArray = new TextRangeArray();

  TextRange range;
  if (mCompositionString.IsEmpty()) {
    // Don't append clause information if composition string is empty.
  } else if (mClauseArray.Length() == 0) {
    // Some IMEs don't return clause array information, then, we assume that
    // all characters in the composition string are in one clause.
    range.mStartOffset = 0;
    range.mEndOffset = mCompositionString.Length();
    range.mRangeType = NS_TEXTRANGE_RAWINPUT;
    textRangeArray->AppendElement(range);

    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: CreateTextRangeArray, mClauseLength=0\n"));
  } else {
    // iterate over the attributes
    uint32_t lastOffset = 0;
    for (uint32_t i = 0; i < mClauseArray.Length() - 1; i++) {
      uint32_t current = mClauseArray[i + 1];
      if (current > mCompositionString.Length()) {
        MOZ_LOG(gIMM32Log, LogLevel::Info,
          ("IMM32: CreateTextRangeArray, mClauseArray[%ld]=%lu. "
           "This is larger than mCompositionString.Length()=%lu\n",
           i + 1, current, mCompositionString.Length()));
        current = int32_t(mCompositionString.Length());
      }

      range.mRangeType = PlatformToNSAttr(mAttributeArray[lastOffset]);
      range.mStartOffset = lastOffset;
      range.mEndOffset = current;
      textRangeArray->AppendElement(range);

      lastOffset = current;

      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: CreateTextRangeArray, index=%ld, rangeType=%s, range=[%lu-%lu]\n",
         i, GetRangeTypeName(range.mRangeType), range.mStartOffset,
         range.mEndOffset));
    }
  }

  if (mCursorPosition == NO_IME_CARET) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: CreateTextRangeArray, no caret\n"));
    return textRangeArray.forget();
  }

  int32_t cursor = mCursorPosition;
  if (uint32_t(cursor) > mCompositionString.Length()) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: CreateTextRangeArray, mCursorPosition=%ld. "
       "This is larger than mCompositionString.Length()=%lu\n",
       mCursorPosition, mCompositionString.Length()));
    cursor = mCompositionString.Length();
  }

  range.mStartOffset = range.mEndOffset = cursor;
  range.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  textRangeArray->AppendElement(range);

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: CreateTextRangeArray, caret position=%ld\n",
     range.mStartOffset));

  return textRangeArray.forget();
}

void
nsIMM32Handler::GetCompositionString(const nsIMEContext &aIMEContext,
                                     DWORD aIndex,
                                     nsAString& aCompositionString) const
{
  aCompositionString.Truncate();

  // Retrieve the size of the required output buffer.
  long lRtn = ::ImmGetCompositionStringW(aIMEContext.get(), aIndex, nullptr, 0);
  if (lRtn < 0 ||
      !aCompositionString.SetLength((lRtn / sizeof(WCHAR)) + 1,
                                    mozilla::fallible)) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: GetCompositionString, FAILED by OOM\n"));
    return; // Error or out of memory.
  }

  // Actually retrieve the composition string information.
  lRtn = ::ImmGetCompositionStringW(aIMEContext.get(), aIndex,
                                    (LPVOID)aCompositionString.BeginWriting(),
                                    lRtn + sizeof(WCHAR));
  aCompositionString.SetLength(lRtn / sizeof(WCHAR));

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: GetCompositionString, SUCCEEDED aCompositionString=\"%s\"\n",
     NS_ConvertUTF16toUTF8(aCompositionString).get()));
}

bool
nsIMM32Handler::GetTargetClauseRange(uint32_t *aOffset, uint32_t *aLength)
{
  NS_ENSURE_TRUE(aOffset, false);
  NS_ENSURE_TRUE(mIsComposing, false);
  NS_ENSURE_TRUE(ShouldDrawCompositionStringOurselves(), false);

  bool found = false;
  *aOffset = mCompositionStart;
  for (uint32_t i = 0; i < mAttributeArray.Length(); i++) {
    if (mAttributeArray[i] == ATTR_TARGET_NOTCONVERTED ||
        mAttributeArray[i] == ATTR_TARGET_CONVERTED) {
      *aOffset = mCompositionStart + i;
      found = true;
      break;
    }
  }

  if (!aLength) {
    return true;
  }

  if (!found) {
    // The all composition string is targetted when there is no ATTR_TARGET_*
    // clause. E.g., there is only ATTR_INPUT
    *aLength = mCompositionString.Length();
    return true;
  }

  uint32_t offsetInComposition = *aOffset - mCompositionStart;
  *aLength = mCompositionString.Length() - offsetInComposition;
  for (uint32_t i = offsetInComposition; i < mAttributeArray.Length(); i++) {
    if (mAttributeArray[i] != ATTR_TARGET_NOTCONVERTED &&
        mAttributeArray[i] != ATTR_TARGET_CONVERTED) {
      *aLength = i - offsetInComposition;
      break;
    }
  }
  return true;
}

bool
nsIMM32Handler::ConvertToANSIString(const nsAFlatString& aStr, UINT aCodePage,
                                   nsACString& aANSIStr)
{
  int len = ::WideCharToMultiByte(aCodePage, 0,
                                  (LPCWSTR)aStr.get(), aStr.Length(),
                                  nullptr, 0, nullptr, nullptr);
  NS_ENSURE_TRUE(len >= 0, false);

  if (!aANSIStr.SetLength(len, mozilla::fallible)) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: ConvertToANSIString, FAILED by OOM\n"));
    return false;
  }
  ::WideCharToMultiByte(aCodePage, 0, (LPCWSTR)aStr.get(), aStr.Length(),
                        (LPSTR)aANSIStr.BeginWriting(), len, nullptr, nullptr);
  return true;
}

bool
nsIMM32Handler::GetCharacterRectOfSelectedTextAt(nsWindow* aWindow,
                                                 uint32_t aOffset,
                                                 nsIntRect& aCharRect,
                                                 WritingMode* aWritingMode)
{
  nsIntPoint point(0, 0);

  WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, aWindow);
  aWindow->InitEvent(selection, &point);
  aWindow->DispatchWindowEvent(&selection);
  if (!selection.mSucceeded) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: GetCharacterRectOfSelectedTextAt, aOffset=%lu, FAILED (NS_QUERY_SELECTED_TEXT)\n",
       aOffset));
    return false;
  }

  uint32_t offset = selection.mReply.mOffset + aOffset;
  bool useCaretRect = selection.mReply.mString.IsEmpty();
  if (useCaretRect && ShouldDrawCompositionStringOurselves() &&
      mIsComposing && !mCompositionString.IsEmpty()) {
    // There is not a normal selection, but we have composition string.
    // XXX mnakano - Should we implement NS_QUERY_IME_SELECTED_TEXT?
    useCaretRect = false;
    if (mCursorPosition != NO_IME_CARET) {
      uint32_t cursorPosition =
        std::min<uint32_t>(mCursorPosition, mCompositionString.Length());
      NS_ASSERTION(offset >= cursorPosition, "offset is less than cursorPosition!");
      offset -= cursorPosition;
    }
  }

  nsIntRect r;
  if (!useCaretRect) {
    WidgetQueryContentEvent charRect(true, NS_QUERY_TEXT_RECT, aWindow);
    charRect.InitForQueryTextRect(offset, 1);
    aWindow->InitEvent(charRect, &point);
    aWindow->DispatchWindowEvent(&charRect);
    if (charRect.mSucceeded) {
      aCharRect = LayoutDevicePixel::ToUntyped(charRect.mReply.mRect);
      if (aWritingMode) {
        *aWritingMode = charRect.GetWritingMode();
      }
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: GetCharacterRectOfSelectedTextAt, aOffset=%lu, SUCCEEDED\n",
         aOffset));
      MOZ_LOG(gIMM32Log, LogLevel::Info,
        ("IMM32: GetCharacterRectOfSelectedTextAt, "
         "aCharRect={ x: %ld, y: %ld, width: %ld, height: %ld }, "
         "charRect.GetWritingMode()=%s",
         aCharRect.x, aCharRect.y, aCharRect.width, aCharRect.height,
         GetWritingModeName(charRect.GetWritingMode()).get()));
      return true;
    }
  }

  return GetCaretRect(aWindow, aCharRect, aWritingMode);
}

bool
nsIMM32Handler::GetCaretRect(nsWindow* aWindow,
                             nsIntRect& aCaretRect,
                             WritingMode* aWritingMode)
{
  nsIntPoint point(0, 0);

  WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, aWindow);
  aWindow->InitEvent(selection, &point);
  aWindow->DispatchWindowEvent(&selection);
  if (!selection.mSucceeded) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: GetCaretRect,  FAILED (NS_QUERY_SELECTED_TEXT)\n"));
    return false;
  }

  uint32_t offset = selection.mReply.mOffset;

  WidgetQueryContentEvent caretRect(true, NS_QUERY_CARET_RECT, aWindow);
  caretRect.InitForQueryCaretRect(offset);
  aWindow->InitEvent(caretRect, &point);
  aWindow->DispatchWindowEvent(&caretRect);
  if (!caretRect.mSucceeded) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: GetCaretRect,  FAILED (NS_QUERY_CARET_RECT)\n"));
    return false;
  }
  aCaretRect = LayoutDevicePixel::ToUntyped(caretRect.mReply.mRect);
  if (aWritingMode) {
    *aWritingMode = caretRect.GetWritingMode();
  }
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: GetCaretRect, SUCCEEDED, "
     "aCaretRect={ x: %ld, y: %ld, width: %ld, height: %ld }, "
     "caretRect.GetWritingMode()=%s",
     aCaretRect.x, aCaretRect.y, aCaretRect.width, aCaretRect.height,
     GetWritingModeName(caretRect.GetWritingMode()).get()));
  return true;
}

bool
nsIMM32Handler::SetIMERelatedWindowsPos(nsWindow* aWindow,
                                        const nsIMEContext &aIMEContext)
{
  nsIntRect r;
  // Get first character rect of current a normal selected text or a composing
  // string.
  WritingMode writingMode;
  bool ret = GetCharacterRectOfSelectedTextAt(aWindow, 0, r, &writingMode);
  NS_ENSURE_TRUE(ret, false);
  nsWindow* toplevelWindow = aWindow->GetTopLevelWindow(false);
  nsIntRect firstSelectedCharRect;
  ResolveIMECaretPos(toplevelWindow, r, aWindow, firstSelectedCharRect);

  // Set native caret size/position to our caret. Some IMEs honor it. E.g.,
  // "Intelligent ABC" (Simplified Chinese) and "MS PinYin 3.0" (Simplified
  // Chinese) on XP.
  nsIntRect caretRect(firstSelectedCharRect);
  if (GetCaretRect(aWindow, r)) {
    ResolveIMECaretPos(toplevelWindow, r, aWindow, caretRect);
  } else {
    NS_WARNING("failed to get caret rect");
    caretRect.width = 1;
  }
  if (!mNativeCaretIsCreated) {
    mNativeCaretIsCreated = ::CreateCaret(aWindow->GetWindowHandle(), nullptr,
                                          caretRect.width, caretRect.height);
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: SetIMERelatedWindowsPos, mNativeCaretIsCreated=%s, width=%ld height=%ld\n",
       mNativeCaretIsCreated ? "TRUE" : "FALSE",
       caretRect.width, caretRect.height));
  }
  ::SetCaretPos(caretRect.x, caretRect.y);

  if (ShouldDrawCompositionStringOurselves()) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: SetIMERelatedWindowsPos, Set candidate window\n"));

    // Get a rect of first character in current target in composition string.
    nsIntRect firstTargetCharRect, lastTargetCharRect;
    if (mIsComposing && !mCompositionString.IsEmpty()) {
      // If there are no targetted selection, we should use it's first character
      // rect instead.
      uint32_t offset, length;
      if (!GetTargetClauseRange(&offset, &length)) {
        MOZ_LOG(gIMM32Log, LogLevel::Info,
          ("IMM32: SetIMERelatedWindowsPos, FAILED, by GetTargetClauseRange\n"));
        return false;
      }
      ret = GetCharacterRectOfSelectedTextAt(aWindow,
                                             offset - mCompositionStart,
                                             firstTargetCharRect, &writingMode);
      NS_ENSURE_TRUE(ret, false);
      if (length) {
        ret = GetCharacterRectOfSelectedTextAt(aWindow,
                offset + length - 1 - mCompositionStart, lastTargetCharRect);
        NS_ENSURE_TRUE(ret, false);
      } else {
        lastTargetCharRect = firstTargetCharRect;
      }
    } else {
      // If there are no composition string, we should use a first character
      // rect.
      ret = GetCharacterRectOfSelectedTextAt(aWindow, 0,
                                             firstTargetCharRect, &writingMode);
      NS_ENSURE_TRUE(ret, false);
      lastTargetCharRect = firstTargetCharRect;
    }
    ResolveIMECaretPos(toplevelWindow, firstTargetCharRect,
                       aWindow, firstTargetCharRect);
    ResolveIMECaretPos(toplevelWindow, lastTargetCharRect,
                       aWindow, lastTargetCharRect);
    nsIntRect targetClauseRect;
    targetClauseRect.UnionRect(firstTargetCharRect, lastTargetCharRect);

    // Move the candidate window to proper position from the target clause as
    // far as possible.
    CANDIDATEFORM candForm;
    candForm.dwIndex = 0;
    if (!writingMode.IsVertical() || IsVerticalWritingSupported()) {
      candForm.dwStyle = CFS_EXCLUDE;
      // Candidate window shouldn't overlap the target clause in any writing
      // mode.
      candForm.rcArea.left = targetClauseRect.x;
      candForm.rcArea.right = targetClauseRect.XMost();
      candForm.rcArea.top = targetClauseRect.y;
      candForm.rcArea.bottom = targetClauseRect.YMost();
      if (!writingMode.IsVertical()) {
        // In horizontal layout, current point of interest should be top-left
        // of the first character.
        candForm.ptCurrentPos.x = firstTargetCharRect.x;
        candForm.ptCurrentPos.y = firstTargetCharRect.y;
      } else if (writingMode.IsVerticalRL()) {
        // In vertical layout (RL), candidate window should be positioned right
        // side of target clause.  However, we don't set vertical writing font
        // to the IME.  Therefore, the candidate window may be positioned
        // bottom-left of target clause rect with these information.
        candForm.ptCurrentPos.x = targetClauseRect.x;
        candForm.ptCurrentPos.y = targetClauseRect.y;
      } else {
        MOZ_ASSERT(writingMode.IsVerticalLR(), "Did we miss some causes?");
        // In vertical layout (LR), candidate window should be poisitioned left
        // side of target clause.  Although, we don't set vertical writing font
        // to the IME, the candidate window may be positioned bottom-right of
        // the target clause rect with these information.
        candForm.ptCurrentPos.x = targetClauseRect.XMost();
        candForm.ptCurrentPos.y = targetClauseRect.y;
      }
    } else {
      // If vertical writing is not supported by IME, let's set candidate
      // window position to the bottom-left of the target clause because
      // the position must be the safest position to prevent the candidate
      // window to overlap with the target clause.
      candForm.dwStyle = CFS_CANDIDATEPOS;
      candForm.ptCurrentPos.x = targetClauseRect.x;
      candForm.ptCurrentPos.y = targetClauseRect.YMost();
    }
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: SetIMERelatedWindowsPos, Calling ImmSetCandidateWindow()... "
       "ptCurrentPos={ x=%d, y=%d }, "
       "rcArea={ left=%d, top=%d, right=%d, bottom=%d }, "
       "writingMode=%s",
       candForm.ptCurrentPos.x, candForm.ptCurrentPos.y,
       candForm.rcArea.left, candForm.rcArea.top,
       candForm.rcArea.right, candForm.rcArea.bottom,
       GetWritingModeName(writingMode).get()));
    ::ImmSetCandidateWindow(aIMEContext.get(), &candForm);
  } else {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: SetIMERelatedWindowsPos, Set composition window\n"));

    // Move the composition window to caret position (if selected some
    // characters, we should use first character rect of them).
    // And in this mode, IME adjusts the candidate window position
    // automatically. So, we don't need to set it.
    COMPOSITIONFORM compForm;
    compForm.dwStyle = CFS_POINT;
    compForm.ptCurrentPos.x =
      !writingMode.IsVerticalLR() ? firstSelectedCharRect.x :
                                    firstSelectedCharRect.XMost();
    compForm.ptCurrentPos.y = firstSelectedCharRect.y;
    ::ImmSetCompositionWindow(aIMEContext.get(), &compForm);
  }

  return true;
}

void
nsIMM32Handler::SetIMERelatedWindowsPosOnPlugin(nsWindow* aWindow,
                                                const nsIMEContext& aIMEContext)
{
  WidgetQueryContentEvent editorRectEvent(true, NS_QUERY_EDITOR_RECT, aWindow);
  aWindow->InitEvent(editorRectEvent);
  aWindow->DispatchWindowEvent(&editorRectEvent);
  if (!editorRectEvent.mSucceeded) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: SetIMERelatedWindowsPosOnPlugin, "
       "FAILED (NS_QUERY_EDITOR_RECT)"));
    return;
  }

  // Clip the plugin rect by the client rect of the window because composition
  // window needs to be specified the position in the client area.
  nsWindow* toplevelWindow = aWindow->GetTopLevelWindow(false);
  LayoutDeviceIntRect pluginRectInScreen =
    editorRectEvent.mReply.mRect + toplevelWindow->WidgetToScreenOffset();
  nsIntRect winRectInScreen;
  aWindow->GetClientBounds(winRectInScreen);
  // composition window cannot be positioned on the edge of client area.
  winRectInScreen.width--;
  winRectInScreen.height--;
  nsIntRect clippedPluginRect;
  clippedPluginRect.x =
    std::min(std::max(pluginRectInScreen.x, winRectInScreen.x),
             winRectInScreen.XMost());
  clippedPluginRect.y =
    std::min(std::max(pluginRectInScreen.y, winRectInScreen.y),
             winRectInScreen.YMost());
  int32_t xMost = std::min(pluginRectInScreen.XMost(), winRectInScreen.XMost());
  int32_t yMost = std::min(pluginRectInScreen.YMost(), winRectInScreen.YMost());
  clippedPluginRect.width = std::max(0, xMost - clippedPluginRect.x);
  clippedPluginRect.height = std::max(0, yMost - clippedPluginRect.y);
  clippedPluginRect -= aWindow->WidgetToScreenOffsetUntyped();

  // Cover the plugin with native caret.  This prevents IME's window and plugin
  // overlap.
  if (mNativeCaretIsCreated) {
    ::DestroyCaret();
  }
  mNativeCaretIsCreated =
    ::CreateCaret(aWindow->GetWindowHandle(), nullptr,
                  clippedPluginRect.width, clippedPluginRect.height);
  ::SetCaretPos(clippedPluginRect.x, clippedPluginRect.y);

  // Set the composition window to bottom-left of the clipped plugin.
  // As far as we know, there is no IME for RTL language.  Therefore, this code
  // must not need to take care of RTL environment.
  COMPOSITIONFORM compForm;
  compForm.dwStyle = CFS_POINT;
  compForm.ptCurrentPos.x = clippedPluginRect.BottomLeft().x;
  compForm.ptCurrentPos.y = clippedPluginRect.BottomLeft().y;
  if (!::ImmSetCompositionWindow(aIMEContext.get(), &compForm)) {
    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: SetIMERelatedWindowsPosOnPlugin, "
       "FAILED to set composition window"));
    return;
  }
}

void
nsIMM32Handler::ResolveIMECaretPos(nsIWidget* aReferenceWidget,
                                   nsIntRect& aCursorRect,
                                   nsIWidget* aNewOriginWidget,
                                   nsIntRect& aOutRect)
{
  aOutRect = aCursorRect;

  if (aReferenceWidget == aNewOriginWidget)
    return;

  if (aReferenceWidget)
    aOutRect.MoveBy(aReferenceWidget->WidgetToScreenOffsetUntyped());

  if (aNewOriginWidget)
    aOutRect.MoveBy(-aNewOriginWidget->WidgetToScreenOffsetUntyped());
}

static void
SetHorizontalFontToLogFont(const nsAString& aFontFace,
                           LOGFONTW& aLogFont)
{
  aLogFont.lfEscapement = aLogFont.lfOrientation = 0;
  if (NS_WARN_IF(aFontFace.Length() > LF_FACESIZE - 1)) {
    memcpy(aLogFont.lfFaceName, L"System", sizeof(L"System"));
    return;
  }
  memcpy(aLogFont.lfFaceName, aFontFace.BeginReading(),
         aFontFace.Length() * sizeof(wchar_t));
  aLogFont.lfFaceName[aFontFace.Length()] = 0;
}

static void
SetVerticalFontToLogFont(const nsAString& aFontFace,
                         LOGFONTW& aLogFont)
{
  aLogFont.lfEscapement = aLogFont.lfOrientation = 2700;
  if (NS_WARN_IF(aFontFace.Length() > LF_FACESIZE - 2)) {
    memcpy(aLogFont.lfFaceName, L"@System", sizeof(L"@System"));
    return;
  }
  aLogFont.lfFaceName[0] = '@';
  memcpy(&aLogFont.lfFaceName[1], aFontFace.BeginReading(),
         aFontFace.Length() * sizeof(wchar_t));
  aLogFont.lfFaceName[aFontFace.Length() + 1] = 0;
}

void
nsIMM32Handler::AdjustCompositionFont(const nsIMEContext& aIMEContext,
                                      const WritingMode& aWritingMode,
                                      bool aForceUpdate)
{
  // An instance of nsIMM32Handler is destroyed when active IME is changed.
  // Therefore, we need to store the information which are set to the IM
  // context to static variables since IM context is never recreated.
  static bool sCompositionFontsInitialized = false;
  static nsString sCompositionFont =
    Preferences::GetString("intl.imm.composition_font");

  // If composition font is customized by pref, we need to modify the
  // composition font of the IME context at first time even if the writing mode
  // is horizontal.
  bool setCompositionFontForcibly = aForceUpdate ||
    (!sCompositionFontsInitialized && !sCompositionFont.IsEmpty());

  static WritingMode sCurrentWritingMode;
  static nsString sCurrentIMEName;
  if (!setCompositionFontForcibly &&
      sWritingModeOfCompositionFont == aWritingMode &&
      sCurrentIMEName == sIMEName) {
    // Nothing to do if writing mode isn't being changed.
    return;
  }

  // Decide composition fonts for both horizontal writing mode and vertical
  // writing mode.  If the font isn't specified by the pref, use default
  // font which is already set to the IM context.  And also in vertical writing
  // mode, insert '@' to the start of the font.
  if (!sCompositionFontsInitialized) {
    sCompositionFontsInitialized = true;
    // sCompositionFontH must not start with '@' and its length is less than
    // LF_FACESIZE since it needs to end with null terminating character.
    if (sCompositionFont.IsEmpty() ||
        sCompositionFont.Length() > LF_FACESIZE - 1 ||
        sCompositionFont[0] == '@') {
      LOGFONTW defaultLogFont;
      if (NS_WARN_IF(!::ImmGetCompositionFont(aIMEContext.get(),
                                              &defaultLogFont))) {
        MOZ_LOG(gIMM32Log, LogLevel::Error,
          ("IMM32: AdjustCompositionFont, ::ImmGetCompositionFont() failed"));
        sCompositionFont.AssignLiteral("System");
      } else {
        // The font face is typically, "System".
        sCompositionFont.Assign(defaultLogFont.lfFaceName);
      }
    }

    MOZ_LOG(gIMM32Log, LogLevel::Info,
      ("IMM32: AdjustCompositionFont, sCompositionFont=\"%s\" is initialized",
       NS_ConvertUTF16toUTF8(sCompositionFont).get()));
  }

  static nsString sCompositionFontForJapanist2003;
  if (IsJapanist2003Active() && sCompositionFontForJapanist2003.IsEmpty()) {
    const char* kCompositionFontForJapanist2003 =
      "intl.imm.composition_font.japanist_2003";
    sCompositionFontForJapanist2003 =
      Preferences::GetString(kCompositionFontForJapanist2003);
    // If the font name is not specified properly, let's use
    // "MS PGothic" instead.
    if (sCompositionFontForJapanist2003.IsEmpty() ||
        sCompositionFontForJapanist2003.Length() > LF_FACESIZE - 2 ||
        sCompositionFontForJapanist2003[0] == '@') {
      sCompositionFontForJapanist2003.AssignLiteral("MS PGothic");
    }
  }

  sWritingModeOfCompositionFont = aWritingMode;
  sCurrentIMEName = sIMEName;

  LOGFONTW logFont;
  memset(&logFont, 0, sizeof(logFont));
  if (!::ImmGetCompositionFont(aIMEContext.get(), &logFont)) {
    MOZ_LOG(gIMM32Log, LogLevel::Error,
      ("IMM32: AdjustCompositionFont, ::ImmGetCompositionFont() failed"));
    logFont.lfFaceName[0] = 0;
  }
  // Need to reset some information which should be recomputed with new font.
  logFont.lfWidth = 0;
  logFont.lfWeight = FW_DONTCARE;
  logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
  logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  logFont.lfPitchAndFamily = DEFAULT_PITCH;

  if (!mIsComposingOnPlugin &&
      aWritingMode.IsVertical() && IsVerticalWritingSupported()) {
    SetVerticalFontToLogFont(
      IsJapanist2003Active() ? sCompositionFontForJapanist2003 :
                               sCompositionFont, logFont);
  } else {
    SetHorizontalFontToLogFont(
      IsJapanist2003Active() ? sCompositionFontForJapanist2003 :
                               sCompositionFont, logFont);
  }
  MOZ_LOG(gIMM32Log, LogLevel::Warning,
    ("IMM32: AdjustCompositionFont, calling ::ImmSetCompositionFont(\"%s\")",
     NS_ConvertUTF16toUTF8(nsDependentString(logFont.lfFaceName)).get()));
  ::ImmSetCompositionFontW(aIMEContext.get(), &logFont);
}

/* static */ nsresult
nsIMM32Handler::OnMouseButtonEvent(nsWindow* aWindow,
                                   const IMENotification& aIMENotification)
{
  // We don't need to create the instance of the handler here.
  if (!gIMM32Handler) {
    return NS_OK;
  }

  if (!sWM_MSIME_MOUSE || !IsComposingOnOurEditor() ||
      !ShouldDrawCompositionStringOurselves()) {
    return NS_OK;
  }

  // We need to handle only mousedown event.
  if (aIMENotification.mMouseButtonEventData.mEventMessage !=
        NS_MOUSE_BUTTON_DOWN) {
    return NS_OK;
  }

  // If the character under the cursor is not in the composition string,
  // we don't need to notify IME of it.
  uint32_t compositionStart = gIMM32Handler->mCompositionStart;
  uint32_t compositionEnd =
    compositionStart + gIMM32Handler->mCompositionString.Length();
  if (aIMENotification.mMouseButtonEventData.mOffset < compositionStart ||
      aIMENotification.mMouseButtonEventData.mOffset >= compositionEnd) {
    return NS_OK;
  }

  BYTE button;
  switch (aIMENotification.mMouseButtonEventData.mButton) {
    case WidgetMouseEventBase::eLeftButton:
      button = IMEMOUSE_LDOWN;
      break;
    case WidgetMouseEventBase::eMiddleButton:
      button = IMEMOUSE_MDOWN;
      break;
    case WidgetMouseEventBase::eRightButton:
      button = IMEMOUSE_RDOWN;
      break;
    default:
      return NS_OK;
  }

  // calcurate positioning and offset
  // char :            JCH1|JCH2|JCH3
  // offset:           0011 1122 2233
  // positioning:      2301 2301 2301
  nsIntPoint cursorPos =
    aIMENotification.mMouseButtonEventData.mCursorPos.AsIntPoint();
  nsIntRect charRect =
    aIMENotification.mMouseButtonEventData.mCharRect.AsIntRect();
  int32_t cursorXInChar = cursorPos.x - charRect.x;
  // The event might hit to zero-width character, see bug 694913.
  // The reason might be:
  // * There are some zero-width characters are actually.
  // * font-size is specified zero.
  // But nobody reproduced this bug actually...
  // We should assume that user clicked on right most of the zero-width
  // character in such case.
  int positioning = 1;
  if (charRect.width > 0) {
    positioning = cursorXInChar * 4 / charRect.width;
    positioning = (positioning + 2) % 4;
  }

  int offset =
    aIMENotification.mMouseButtonEventData.mOffset - compositionStart;
  if (positioning < 2) {
    offset++;
  }

  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnMouseButtonEvent, x,y=%ld,%ld, offset=%ld, positioning=%ld\n",
     cursorPos.x, cursorPos.y, offset, positioning));

  // send MS_MSIME_MOUSE message to default IME window.
  HWND imeWnd = ::ImmGetDefaultIMEWnd(aWindow->GetWindowHandle());
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  if (::SendMessageW(imeWnd, sWM_MSIME_MOUSE,
                     MAKELONG(MAKEWORD(button, positioning), offset),
                     (LPARAM) IMEContext.get()) == 1) {
    return NS_SUCCESS_EVENT_CONSUMED;
  }
  return NS_OK;
}

/* static */ bool
nsIMM32Handler::OnKeyDownEvent(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                               MSGResult& aResult)
{
  MOZ_LOG(gIMM32Log, LogLevel::Info,
    ("IMM32: OnKeyDownEvent, hWnd=%08x, wParam=%08x, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam, lParam));
  aResult.mConsumed = false;
  switch (wParam) {
    case VK_TAB:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_END:
    case VK_HOME:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
      // If IME didn't process the key message (the virtual key code wasn't
      // converted to VK_PROCESSKEY), and the virtual key code event causes
      // to move caret, we should cancel the composition here.  Then, this
      // event will be dispatched.
      // XXX I think that we should dispatch all key events during composition,
      //     and nsEditor should cancel/commit the composition if it *thinks*
      //     it's needed.
      if (IsComposingOnOurEditor()) {
        // NOTE: We don't need to cancel the composition on another window.
        CancelComposition(aWindow, false);
      }
      return false;
    default:
      return false;
  }
}
