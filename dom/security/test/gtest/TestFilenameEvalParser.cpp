/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <string.h>
#include <stdlib.h>

#include "nsContentSecurityUtils.h"
#include "nsStringFwd.h"

#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

static constexpr auto kChromeURI = "chromeuri"_ns;
static constexpr auto kResourceURI = "resourceuri"_ns;
static constexpr auto kBlobUri = "bloburi"_ns;
static constexpr auto kDataUri = "dataurl"_ns;
static constexpr auto kAboutUri = "abouturi"_ns;
static constexpr auto kSingleString = "singlestring"_ns;
static constexpr auto kMozillaExtensionFile = "mozillaextension_file"_ns;
static constexpr auto kExtensionURI = "extension_uri"_ns;
static constexpr auto kSuspectedUserChromeJS = "suspectedUserChromeJS"_ns;
#if defined(XP_WIN)
static constexpr auto kSanitizedWindowsURL = "sanitizedWindowsURL"_ns;
static constexpr auto kSanitizedWindowsPath = "sanitizedWindowsPath"_ns;
#endif
static constexpr auto kOther = "other"_ns;

#define ASSERT_AND_PRINT(first, second, condition)                      \
  fprintf(stderr, "First: %s\n", first.get());                          \
  fprintf(stderr, "Second: %s\n", NS_ConvertUTF16toUTF8(second).get()); \
  ASSERT_TRUE((condition));
// Usage: ASSERT_AND_PRINT(ret.first, ret.second.value(), ...

#define ASSERT_AND_PRINT_FIRST(first, condition) \
  fprintf(stderr, "First: %s\n", (first).get()); \
  ASSERT_TRUE((condition));
// Usage: ASSERT_AND_PRINT_FIRST(ret.first, ...

TEST(FilenameEvalParser, ResourceChrome)
{
  {
    constexpr auto str = "chrome://firegestures/content/browser.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kChromeURI && ret.second.isSome() &&
                ret.second.value() == str);
  }
  {
    constexpr auto str = "resource://firegestures/content/browser.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kResourceURI && ret.second.isSome() &&
                ret.second.value() == str);
  }
  {
    constexpr auto str = "resource://foo/bar.js#foobar"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kResourceURI);
    ASSERT_EQ(ret.second.value(), "resource://foo/bar.js"_ns);
  }
  {
    constexpr auto str = "chrome://foo/bar.js?foo"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kChromeURI);
    ASSERT_EQ(ret.second.value(), "chrome://foo/bar.js"_ns);
  }
  {
    constexpr auto str = "chrome://foo/bar.js?foo#bar"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kChromeURI);
    ASSERT_EQ(ret.second.value(), "chrome://foo/bar.js"_ns);
  }
}

TEST(FilenameEvalParser, BlobData)
{
  {
    constexpr auto str = "blob://000-000"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kBlobUri && !ret.second.isSome());
  }
  {
    constexpr auto str = "blob:000-000"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kBlobUri && !ret.second.isSome());
  }
  {
    constexpr auto str = "data://blahblahblah"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kDataUri && !ret.second.isSome());
  }
  {
    constexpr auto str = "data:blahblahblah"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kDataUri && !ret.second.isSome());
  }
}

TEST(FilenameEvalParser, MozExtension)
{
  {  // Test shield.mozilla.org replacing
    constexpr auto str =
        "jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/"
        "foo/"
        "extensions/federated-learning@shield.mozilla.org.xpi!/experiments/"
        "study/api.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kMozillaExtensionFile &&
                ret.second.value() ==
                    "federated-learning@s!/experiments/study/api.js"_ns);
  }
  {  // Test mozilla.org replacing
    constexpr auto str =
        "jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/"
        "foo/"
        "extensions/federated-learning@shigeld.mozilla.org.xpi!/experiments/"
        "study/api.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(
        ret.first == kMozillaExtensionFile &&
        ret.second.value() ==
            "federated-learning@shigeld.m!/experiments/study/api.js"_ns);
  }
  {  // Test truncating
    constexpr auto str =
        "jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/"
        "foo/"
        "extensions/federated-learning@shigeld.mozilla.org.xpi!/experiments/"
        "study/apiiiiiiiiiiiiiiiiiiiiiiiiiiiiii.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kMozillaExtensionFile &&
                ret.second.value() ==
                    "federated-learning@shigeld.m!/experiments/"
                    "study/apiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"_ns);
  }
}

TEST(FilenameEvalParser, UserChromeJS)
{
  {
    constexpr auto str = "firegestures/content/browser.uc.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_TRUE(ret.second.isNothing());
  }
  {
    constexpr auto str = "firegestures/content/browser.uc.js?"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_TRUE(ret.second.isNothing());
  }
  {
    constexpr auto str = "firegestures/content/browser.uc.js?243244224"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_TRUE(ret.second.isNothing());
  }
  {
    constexpr auto str =
        "file:///b:/fxprofiles/mark/chrome/"
        "addbookmarkherewithmiddleclick.uc.js?1558444389291"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_TRUE(ret.second.isNothing());
  }
  {
    constexpr auto str =
        "chrome://tabmix-resource/content/bootstrap/Overlays.jsm"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_EQ(ret.second.value(), str);
  }
  {
    constexpr auto str = "chrome://tabmixplus/content/utils.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_EQ(ret.second.value(), str);
  }
  {
    constexpr auto str = "chrome://searchwp/content/searchbox.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_EQ(ret.second.value(), str);
  }
  {
    constexpr auto str =
        "chrome://userscripts/content/Geckium_toolbarButtonCreator.uc.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_EQ(ret.second.value(), str);
  }
  {
    constexpr auto str = "chrome://userchromejs/content/boot.sys.mjs"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_EQ(ret.second.value(), str);
  }
  {
    constexpr auto str = "resource://usl-ucjs/UserScriptLoaderParent.jsm"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_EQ(ret.second.value(), str);
  }
  {
    constexpr auto str = "resource://cpmanager-legacy/CPManager.jsm"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_EQ(ret.second.value(), str);
  }
  {
    constexpr auto str = "resource://sfm-ucjs/SaveFolderModokiParent.mjs"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_EQ(ret.first, kSuspectedUserChromeJS);
    ASSERT_EQ(ret.second.value(), str);
  }
}

TEST(FilenameEvalParser, SingleFile)
{
  {
    constexpr auto str = "browser.uc.js?2456"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSingleString && ret.second.isSome() &&
                ret.second.value() == str);
  }
  {
    constexpr auto str = "debugger"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSingleString && ret.second.isSome() &&
                ret.second.value() == str);
  }
}

TEST(FilenameEvalParser, Other)
{
  {
    constexpr auto str = "firegestures--content"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
  }
  {
    constexpr auto str = "gallop://thing/fire"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == "gallop"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = "gallop://fire"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == "gallop"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = "firegestures/content"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == "content"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = "firegestures\\content"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == "content"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = "/home/tom/files/thing"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == "thing"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = "file://c/uers/tom/file.txt"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == "file://.../file.txt"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = "c:/uers/tom/file.txt"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == "file.txt"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = "http://example.com/"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == "http"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = "http://example.com/thing.html"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == "http"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
}

TEST(FilenameEvalParser, WebExtensionPathParser)
{
  {
    // Set up an Extension and register it so we can test against it.
    mozilla::dom::AutoJSAPI jsAPI;
    ASSERT_TRUE(jsAPI.Init(xpc::PrivilegedJunkScope()));
    JSContext* cx = jsAPI.cx();

    mozilla::dom::GlobalObject go(cx, xpc::PrivilegedJunkScope());
    auto* wEI = new mozilla::extensions::WebExtensionInit();

    JS::Rooted<JSObject*> func(
        cx, (JSObject*)JS_NewFunction(cx, (JSNative)1, 0, 0, "customMethodA"));
    JS::Rooted<JSObject*> tempGlobalRoot(cx, JS::CurrentGlobalOrNull(cx));
    wEI->mLocalizeCallback = new mozilla::dom::WebExtensionLocalizeCallback(
        cx, func, tempGlobalRoot, nullptr);

    wEI->mAllowedOrigins =
        mozilla::dom::OwningMatchPatternSetOrStringSequence();
    nsString* slotPtr =
        wEI->mAllowedOrigins.SetAsStringSequence().AppendElement(
            mozilla::fallible);
    ASSERT_TRUE(slotPtr != nullptr);
    nsString& slot = *slotPtr;
    slot.Truncate();
    slot = u"http://example.com"_ns;

    wEI->mName = u"gtest Test Extension"_ns;
    wEI->mId = u"gtesttestextension@mozilla.org"_ns;
    wEI->mBaseURL = u"file://foo"_ns;
    wEI->mMozExtensionHostname = "e37c3c08-beac-a04b-8032-c4f699a1a856"_ns;

    mozilla::ErrorResult eR;
    RefPtr<mozilla::WebExtensionPolicy> w =
        mozilla::extensions::WebExtensionPolicy::Constructor(go, *wEI, eR);
    w->SetActive(true, eR);

    constexpr auto str =
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/file.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);

    ASSERT_TRUE(ret.first == kExtensionURI &&
                ret.second.value() ==
                    "moz-extension://[gtesttestextension@mozilla.org: "
                    "gtest Test Extension]P=0/path/to/file.js"_ns);

    w->SetActive(false, eR);

    delete wEI;
  }
  {
    // Set up an Extension and register it so we can test against it.
    mozilla::dom::AutoJSAPI jsAPI;
    ASSERT_TRUE(jsAPI.Init(xpc::PrivilegedJunkScope()));
    JSContext* cx = jsAPI.cx();

    mozilla::dom::GlobalObject go(cx, xpc::PrivilegedJunkScope());
    auto wEI = new mozilla::extensions::WebExtensionInit();

    JS::Rooted<JSObject*> func(
        cx, (JSObject*)JS_NewFunction(cx, (JSNative)1, 0, 0, "customMethodA"));
    JS::Rooted<JSObject*> tempGlobalRoot(cx, JS::CurrentGlobalOrNull(cx));
    wEI->mLocalizeCallback = new mozilla::dom::WebExtensionLocalizeCallback(
        cx, func, tempGlobalRoot, NULL);

    wEI->mAllowedOrigins =
        mozilla::dom::OwningMatchPatternSetOrStringSequence();
    nsString* slotPtr =
        wEI->mAllowedOrigins.SetAsStringSequence().AppendElement(
            mozilla::fallible);
    nsString& slot = *slotPtr;
    slot.Truncate();
    slot = u"http://example.com"_ns;

    wEI->mName = u"gtest Test Extension"_ns;
    wEI->mId = u"gtesttestextension@mozilla.org"_ns;
    wEI->mBaseURL = u"file://foo"_ns;
    wEI->mMozExtensionHostname = "e37c3c08-beac-a04b-8032-c4f699a1a856"_ns;
    wEI->mIsPrivileged = true;

    mozilla::ErrorResult eR;
    RefPtr<mozilla::WebExtensionPolicy> w =
        mozilla::extensions::WebExtensionPolicy::Constructor(go, *wEI, eR);
    w->SetActive(true, eR);

    constexpr auto str =
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/file.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);

    ASSERT_TRUE(ret.first == kExtensionURI &&
                ret.second.value() ==
                    "moz-extension://[gtesttestextension@mozilla.org: "
                    "gtest Test Extension]P=1/path/to/file.js"_ns);

    w->SetActive(false, eR);

    delete wEI;
  }
  {
    constexpr auto str =
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/file.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kExtensionURI && !ret.second.isSome());
  }
  {
    constexpr auto str =
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/file.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);
    ASSERT_TRUE(
        ret.first == kExtensionURI &&
        ret.second.value() ==
            "moz-extension://[failed finding addon by host]/file.js"_ns);
  }
  {
    constexpr auto str =
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/"
        "file.js?querystringx=6"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);
    ASSERT_TRUE(ret.first == kExtensionURI &&
                ret.second.value() ==
                    "moz-extension://[failed finding addon "
                    "by host]/path/to/file.js"_ns);
  }
}

TEST(FilenameEvalParser, AboutPageParser)
{
  {
    constexpr auto str = "about:about"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kAboutUri &&
                ret.second.value() == "about:about"_ns);
  }
  {
    constexpr auto str = "about:about?hello"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kAboutUri &&
                ret.second.value() == "about:about"_ns);
  }
  {
    constexpr auto str = "about:about#mom"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kAboutUri &&
                ret.second.value() == "about:about"_ns);
  }
  {
    constexpr auto str = "about:about?hello=there#mom"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kAboutUri &&
                ret.second.value() == "about:about"_ns);
  }
}
