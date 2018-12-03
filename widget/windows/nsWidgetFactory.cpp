/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsdefs.h"
#include "nsWidgetsCID.h"
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/widget/ScreenManager.h"
#include "nsIServiceManager.h"
#include "nsIdleServiceWin.h"
#include "nsLookAndFeel.h"
#include "nsSound.h"
#include "WinMouseScrollHandler.h"
#include "KeyboardLayout.h"
#include "GfxInfo.h"
#include "nsToolkit.h"

// Modules that switch out based on the environment
#include "nsXULAppAPI.h"
// Desktop
#include "nsFilePicker.h"  // needs to be included before other shobjidl.h includes
#include "nsColorPicker.h"
// Content processes
#include "nsFilePickerProxy.h"

// Drag & Drop, Clipboard
#include "nsClipboardHelper.h"
#include "nsClipboard.h"
#include "HeadlessClipboard.h"
#include "nsDragService.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"

#include "WinTaskbar.h"
#include "JumpListBuilder.h"
#include "JumpListItem.h"
#include "TaskbarPreview.h"
// Toast notification support
#ifndef __MINGW32__
#include "ToastNotification.h"
#include "nsToolkitCompsCID.h"
#endif

#include "WindowsUIUtils.h"

#ifdef NS_PRINTING
#include "nsDeviceContextSpecWin.h"
#include "nsPrintDialogWin.h"
#include "nsPrintSettingsServiceWin.h"
#include "nsPrintSession.h"
#endif

using namespace mozilla;
using namespace mozilla::widget;

static nsresult FilePickerConstructor(nsISupports *aOuter, REFNSIID aIID,
                                      void **aResult) {
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIFilePicker> picker = new nsFilePicker;
  return picker->QueryInterface(aIID, aResult);
}

static nsresult ColorPickerConstructor(nsISupports *aOuter, REFNSIID aIID,
                                       void **aResult) {
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIColorPicker> picker = new nsColorPicker;
  return picker->QueryInterface(aIID, aResult);
}

static nsresult nsClipboardConstructor(nsISupports *aOuter, REFNSIID aIID,
                                       void **aResult) {
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCOMPtr<nsIClipboard> inst;
  if (gfxPlatform::IsHeadless()) {
    inst = new HeadlessClipboard();
  } else {
    inst = new nsClipboard();
  }
  return inst->QueryInterface(aIID, aResult);
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(ScreenManager,
                                         ScreenManager::GetAddRefedSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIdleServiceWin,
                                         nsIdleServiceWin::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsISound, nsSound::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(WinTaskbar)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListBuilder)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListItem)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListSeparator)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListLink)
NS_GENERIC_FACTORY_CONSTRUCTOR(JumpListShortcut)
NS_GENERIC_FACTORY_CONSTRUCTOR(WindowsUIUtils)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
#ifndef __MINGW32__
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(ToastNotification, Init)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(TaskbarPreviewCallback)
#ifdef NS_PRINTING
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintDialogServiceWin, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSettingsServiceWin, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorWin)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecWin)
#endif

namespace mozilla {
namespace widget {
// This constructor should really be shared with all platforms.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(GfxInfo, Init)
}  // namespace widget
}  // namespace mozilla

NS_DEFINE_NAMED_CID(NS_FILEPICKER_CID);
NS_DEFINE_NAMED_CID(NS_COLORPICKER_CID);
NS_DEFINE_NAMED_CID(NS_APPSHELL_CID);
NS_DEFINE_NAMED_CID(NS_SCREENMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_GFXINFO_CID);
NS_DEFINE_NAMED_CID(NS_IDLE_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARD_CID);
NS_DEFINE_NAMED_CID(NS_CLIPBOARDHELPER_CID);
NS_DEFINE_NAMED_CID(NS_SOUND_CID);
NS_DEFINE_NAMED_CID(NS_TRANSFERABLE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLFORMATCONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_WIN_TASKBAR_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTBUILDER_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTITEM_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTSEPARATOR_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTLINK_CID);
NS_DEFINE_NAMED_CID(NS_WIN_JUMPLISTSHORTCUT_CID);
NS_DEFINE_NAMED_CID(NS_WINDOWS_UIUTILS_CID);
NS_DEFINE_NAMED_CID(NS_DRAGSERVICE_CID);
#ifndef __MINGW32__
NS_DEFINE_NAMED_CID(NS_SYSTEMALERTSSERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_TASKBARPREVIEWCALLBACK_CID);
#ifdef NS_PRINTING
NS_DEFINE_NAMED_CID(NS_PRINTDIALOGSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSETTINGSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_PRINTER_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_PRINTSESSION_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_SPEC_CID);
#endif

static const mozilla::Module::CIDEntry kWidgetCIDs[] = {
    {&kNS_FILEPICKER_CID, false, nullptr, FilePickerConstructor,
     Module::MAIN_PROCESS_ONLY},
    {&kNS_COLORPICKER_CID, false, nullptr, ColorPickerConstructor,
     Module::MAIN_PROCESS_ONLY},
    {&kNS_APPSHELL_CID, false, nullptr, nsAppShellConstructor,
     Module::ALLOW_IN_GPU_AND_VR_PROCESS},
    {&kNS_SCREENMANAGER_CID, false, nullptr, ScreenManagerConstructor,
     Module::MAIN_PROCESS_ONLY},
    {&kNS_GFXINFO_CID, false, nullptr, GfxInfoConstructor,
     Module::ALLOW_IN_GPU_PROCESS},
    {&kNS_IDLE_SERVICE_CID, false, nullptr, nsIdleServiceWinConstructor},
    {&kNS_CLIPBOARD_CID, false, nullptr, nsClipboardConstructor,
     Module::MAIN_PROCESS_ONLY},
    {&kNS_CLIPBOARDHELPER_CID, false, nullptr, nsClipboardHelperConstructor},
    {&kNS_SOUND_CID, false, nullptr, nsISoundConstructor,
     Module::MAIN_PROCESS_ONLY},
    {&kNS_TRANSFERABLE_CID, false, nullptr, nsTransferableConstructor},
    {&kNS_HTMLFORMATCONVERTER_CID, false, nullptr,
     nsHTMLFormatConverterConstructor},
    {&kNS_WIN_TASKBAR_CID, false, nullptr, WinTaskbarConstructor},
    {&kNS_WIN_JUMPLISTBUILDER_CID, false, nullptr, JumpListBuilderConstructor},
    {&kNS_WIN_JUMPLISTITEM_CID, false, nullptr, JumpListItemConstructor},
    {&kNS_WIN_JUMPLISTSEPARATOR_CID, false, nullptr,
     JumpListSeparatorConstructor},
    {&kNS_WIN_JUMPLISTLINK_CID, false, nullptr, JumpListLinkConstructor},
    {&kNS_WIN_JUMPLISTSHORTCUT_CID, false, nullptr,
     JumpListShortcutConstructor},
    {&kNS_WINDOWS_UIUTILS_CID, false, nullptr, WindowsUIUtilsConstructor},
    {&kNS_DRAGSERVICE_CID, false, nullptr, nsDragServiceConstructor,
     Module::MAIN_PROCESS_ONLY},
#ifndef __MINGW32__
    {&kNS_SYSTEMALERTSSERVICE_CID, false, nullptr, ToastNotificationConstructor,
     Module::MAIN_PROCESS_ONLY},
#endif
    {&kNS_TASKBARPREVIEWCALLBACK_CID, false, nullptr,
     TaskbarPreviewCallbackConstructor},
#ifdef NS_PRINTING
    {&kNS_PRINTDIALOGSERVICE_CID, false, nullptr,
     nsPrintDialogServiceWinConstructor, Module::MAIN_PROCESS_ONLY},
    {&kNS_PRINTSETTINGSSERVICE_CID, false, nullptr,
     nsPrintSettingsServiceWinConstructor},
    {&kNS_PRINTER_ENUMERATOR_CID, false, nullptr,
     nsPrinterEnumeratorWinConstructor},
    {&kNS_PRINTSESSION_CID, false, nullptr, nsPrintSessionConstructor},
    {&kNS_DEVICE_CONTEXT_SPEC_CID, false, nullptr,
     nsDeviceContextSpecWinConstructor},
#endif
    {nullptr}};

static const mozilla::Module::ContractIDEntry kWidgetContracts[] = {
    {"@mozilla.org/filepicker;1", &kNS_FILEPICKER_CID,
     Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/colorpicker;1", &kNS_COLORPICKER_CID,
     Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/widget/appshell/win;1", &kNS_APPSHELL_CID,
     Module::ALLOW_IN_GPU_AND_VR_PROCESS},
    {"@mozilla.org/gfx/screenmanager;1", &kNS_SCREENMANAGER_CID,
     Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/gfx/info;1", &kNS_GFXINFO_CID, Module::ALLOW_IN_GPU_PROCESS},
    {"@mozilla.org/widget/idleservice;1", &kNS_IDLE_SERVICE_CID},
    {"@mozilla.org/widget/clipboard;1", &kNS_CLIPBOARD_CID,
     Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/widget/clipboardhelper;1", &kNS_CLIPBOARDHELPER_CID},
    {"@mozilla.org/sound;1", &kNS_SOUND_CID, Module::MAIN_PROCESS_ONLY},
    {"@mozilla.org/widget/transferable;1", &kNS_TRANSFERABLE_CID},
    {"@mozilla.org/widget/htmlformatconverter;1", &kNS_HTMLFORMATCONVERTER_CID},
    {"@mozilla.org/windows-taskbar;1", &kNS_WIN_TASKBAR_CID},
    {"@mozilla.org/windows-jumplistbuilder;1", &kNS_WIN_JUMPLISTBUILDER_CID},
    {"@mozilla.org/windows-jumplistitem;1", &kNS_WIN_JUMPLISTITEM_CID},
    {"@mozilla.org/windows-jumplistseparator;1",
     &kNS_WIN_JUMPLISTSEPARATOR_CID},
    {"@mozilla.org/windows-jumplistlink;1", &kNS_WIN_JUMPLISTLINK_CID},
    {"@mozilla.org/windows-jumplistshortcut;1", &kNS_WIN_JUMPLISTSHORTCUT_CID},
    {"@mozilla.org/windows-ui-utils;1", &kNS_WINDOWS_UIUTILS_CID},
    {"@mozilla.org/widget/dragservice;1", &kNS_DRAGSERVICE_CID,
     Module::MAIN_PROCESS_ONLY},
#ifndef __MINGW32__
    {NS_SYSTEMALERTSERVICE_CONTRACTID, &kNS_SYSTEMALERTSSERVICE_CID,
     Module::MAIN_PROCESS_ONLY},
#endif
    {"@mozilla.org/widget/taskbar-preview-callback;1",
     &kNS_TASKBARPREVIEWCALLBACK_CID},
#ifdef NS_PRINTING
    {NS_PRINTDIALOGSERVICE_CONTRACTID, &kNS_PRINTDIALOGSERVICE_CID},
    {"@mozilla.org/gfx/printsettings-service;1", &kNS_PRINTSETTINGSSERVICE_CID},
    {"@mozilla.org/gfx/printerenumerator;1", &kNS_PRINTER_ENUMERATOR_CID},
    {"@mozilla.org/gfx/printsession;1", &kNS_PRINTSESSION_CID},
    {"@mozilla.org/gfx/devicecontextspec;1", &kNS_DEVICE_CONTEXT_SPEC_CID},
#endif
    {nullptr}};

static void nsWidgetWindowsModuleDtor() {
  // Shutdown all XP level widget classes.
  WidgetUtils::Shutdown();

  KeyboardLayout::Shutdown();
  MouseScrollHandler::Shutdown();
  nsLookAndFeel::Shutdown();
  nsToolkit::Shutdown();
  nsAppShellShutdown();
}

static const mozilla::Module kWidgetModule = {
    mozilla::Module::kVersion,
    kWidgetCIDs,
    kWidgetContracts,
    nullptr,
    nullptr,
    nsAppShellInit,
    nsWidgetWindowsModuleDtor,
    Module::ALLOW_IN_GPU_AND_VR_PROCESS};

NSMODULE_DEFN(nsWidgetModule) = &kWidgetModule;
