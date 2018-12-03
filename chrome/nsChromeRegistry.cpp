/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChromeRegistry.h"
#include "nsChromeRegistryChrome.h"
#include "nsChromeRegistryContent.h"

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsQueryObject.h"

#include "mozilla/dom/URL.h"
#include "nsDOMWindowList.h"
#include "nsIConsoleService.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIScriptError.h"
#include "nsIWindowMediator.h"
#include "nsIPrefService.h"
#include "mozilla/Preferences.h"
#include "mozilla/Printf.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/dom/Location.h"
#include "nsIURIMutator.h"

#include "unicode/uloc.h"

nsChromeRegistry* nsChromeRegistry::gChromeRegistry;

// DO NOT use namespace mozilla; it'll break due to a naming conflict between
// mozilla::TextRange and a TextRange in OSX headers.
using mozilla::StyleSheet;
using mozilla::dom::IsChromeURI;
using mozilla::dom::Location;

////////////////////////////////////////////////////////////////////////////////

void nsChromeRegistry::LogMessage(const char* aMsg, ...) {
  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) return;

  va_list args;
  va_start(args, aMsg);
  mozilla::SmprintfPointer formatted = mozilla::Vsmprintf(aMsg, args);
  va_end(args);
  if (!formatted) return;

  console->LogStringMessage(NS_ConvertUTF8toUTF16(formatted.get()).get());
}

void nsChromeRegistry::LogMessageWithContext(nsIURI* aURL, uint32_t aLineNumber,
                                             uint32_t flags, const char* aMsg,
                                             ...) {
  nsresult rv;

  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  if (!console || !error) return;

  va_list args;
  va_start(args, aMsg);
  mozilla::SmprintfPointer formatted = mozilla::Vsmprintf(aMsg, args);
  va_end(args);
  if (!formatted) return;

  nsCString spec;
  if (aURL) aURL->GetSpec(spec);

  rv = error->Init(NS_ConvertUTF8toUTF16(formatted.get()),
                   NS_ConvertUTF8toUTF16(spec), EmptyString(), aLineNumber, 0,
                   flags, "chrome registration",
                   false /* from private window */);

  if (NS_FAILED(rv)) return;

  console->LogMessage(error);
}

nsChromeRegistry::~nsChromeRegistry() { gChromeRegistry = nullptr; }

NS_INTERFACE_MAP_BEGIN(nsChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIXULChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIToolkitChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChromeRegistry)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsChromeRegistry)
NS_IMPL_RELEASE(nsChromeRegistry)

////////////////////////////////////////////////////////////////////////////////
// nsIChromeRegistry methods:

already_AddRefed<nsIChromeRegistry> nsChromeRegistry::GetService() {
  if (!gChromeRegistry) {
    // We don't actually want this ref, we just want the service to
    // initialize if it hasn't already.
    nsCOMPtr<nsIChromeRegistry> reg(
        do_GetService(NS_CHROMEREGISTRY_CONTRACTID));
    if (!gChromeRegistry) return nullptr;
  }
  nsCOMPtr<nsIChromeRegistry> registry = gChromeRegistry;
  return registry.forget();
}

nsresult nsChromeRegistry::Init() {
  // This initialization process is fairly complicated and may cause reentrant
  // getservice calls to resolve chrome URIs (especially locale files). We
  // don't want that, so we inform the protocol handler about our existence
  // before we are actually fully initialized.
  gChromeRegistry = this;

  mInitialized = true;

  return NS_OK;
}

nsresult nsChromeRegistry::GetProviderAndPath(nsIURI* aChromeURL,
                                              nsACString& aProvider,
                                              nsACString& aPath) {
  nsresult rv;

#ifdef DEBUG
  bool isChrome;
  aChromeURL->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI?");
#endif

  nsAutoCString path;
  rv = aChromeURL->GetPathQueryRef(path);
  NS_ENSURE_SUCCESS(rv, rv);

  if (path.Length() < 3) {
    LogMessage("Invalid chrome URI: %s", path.get());
    return NS_ERROR_FAILURE;
  }

  path.SetLength(nsUnescapeCount(path.BeginWriting()));
  NS_ASSERTION(path.First() == '/', "Path should always begin with a slash!");

  int32_t slash = path.FindChar('/', 1);
  if (slash == 1) {
    LogMessage("Invalid chrome URI: %s", path.get());
    return NS_ERROR_FAILURE;
  }

  if (slash == -1) {
    aPath.Truncate();
  } else {
    if (slash == (int32_t)path.Length() - 1)
      aPath.Truncate();
    else
      aPath.Assign(path.get() + slash + 1, path.Length() - slash - 1);

    --slash;
  }

  aProvider.Assign(path.get() + 1, slash);
  return NS_OK;
}

nsresult nsChromeRegistry::Canonify(nsCOMPtr<nsIURI>& aChromeURL) {
  NS_NAMED_LITERAL_CSTRING(kSlash, "/");

  nsresult rv;

  nsAutoCString provider, path;
  rv = GetProviderAndPath(aChromeURL, provider, path);
  NS_ENSURE_SUCCESS(rv, rv);

  if (path.IsEmpty()) {
    nsAutoCString package;
    rv = aChromeURL->GetHost(package);
    NS_ENSURE_SUCCESS(rv, rv);

    // we re-use the "path" local string to build a new URL path
    path.Assign(kSlash + provider + kSlash + package);
    if (provider.EqualsLiteral("content")) {
      path.AppendLiteral(".xul");
    } else if (provider.EqualsLiteral("locale")) {
      path.AppendLiteral(".dtd");
    } else if (provider.EqualsLiteral("skin")) {
      path.AppendLiteral(".css");
    } else {
      return NS_ERROR_INVALID_ARG;
    }
    return NS_MutateURI(aChromeURL).SetPathQueryRef(path).Finalize(aChromeURL);
  } else {
    // prevent directory traversals ("..")
    // path is already unescaped once, but uris can get unescaped twice
    const char* pos = path.BeginReading();
    const char* end = path.EndReading();
    // Must start with [a-zA-Z0-9].
    if (!('a' <= *pos && *pos <= 'z') && !('A' <= *pos && *pos <= 'Z') &&
        !('0' <= *pos && *pos <= '9')) {
      return NS_ERROR_DOM_BAD_URI;
    }
    while (pos < end) {
      switch (*pos) {
        case ':':
          return NS_ERROR_DOM_BAD_URI;
        case '.':
          if (pos[1] == '.') return NS_ERROR_DOM_BAD_URI;
          break;
        case '%':
          // chrome: URIs with double-escapes are trying to trick us.
          // watch for %2e, and %25 in case someone triple unescapes
          if (pos[1] == '2' &&
              (pos[2] == 'e' || pos[2] == 'E' || pos[2] == '5'))
            return NS_ERROR_DOM_BAD_URI;
          break;
        case '?':
        case '#':
          pos = end;
          continue;
      }
      ++pos;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::ConvertChromeURL(nsIURI* aChromeURI, nsIURI** aResult) {
  nsresult rv;
  if (NS_WARN_IF(!aChromeURI)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mOverrideTable.Get(aChromeURI, aResult)) return NS_OK;

  nsCOMPtr<nsIURL> chromeURL(do_QueryInterface(aChromeURI));
  NS_ENSURE_TRUE(chromeURL, NS_NOINTERFACE);

  nsAutoCString package, provider, path;
  rv = chromeURL->GetHostPort(package);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetProviderAndPath(chromeURL, provider, path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIURI* baseURI = GetBaseURIFromPackage(package, provider, path);

  uint32_t flags;
  rv = GetFlagsFromPackage(package, &flags);
  if (NS_FAILED(rv)) return rv;

  if (!baseURI) {
    LogMessage("No chrome package registered for chrome://%s/%s/%s",
               package.get(), provider.get(), path.get());
    return NS_ERROR_FILE_NOT_FOUND;
  }

  return NS_NewURI(aResult, path, nullptr, baseURI);
}

////////////////////////////////////////////////////////////////////////

// theme stuff

static void FlushSkinBindingsForWindow(nsPIDOMWindowOuter* aWindow) {
  // Get the document.
  nsCOMPtr<nsIDocument> document = aWindow->GetDoc();
  if (!document) return;

  // Annihilate all XBL bindings.
  document->FlushSkinBindings();
}

// XXXbsmedberg: move this to nsIWindowMediator
NS_IMETHODIMP nsChromeRegistry::RefreshSkins() {
  nsCOMPtr<nsIWindowMediator> windowMediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!windowMediator) return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
  bool more;
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsPIDOMWindowOuter> domWindow = do_QueryInterface(protoWindow);
      if (domWindow) FlushSkinBindingsForWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }

  FlushSkinCaches();

  windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsPIDOMWindowOuter> domWindow = do_QueryInterface(protoWindow);
      if (domWindow) RefreshWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }

  return NS_OK;
}

void nsChromeRegistry::FlushSkinCaches() {
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  NS_ASSERTION(obsSvc, "Couldn't get observer service.");

  obsSvc->NotifyObservers(static_cast<nsIChromeRegistry*>(this),
                          NS_CHROME_FLUSH_SKINS_TOPIC, nullptr);
}

// XXXbsmedberg: move this to windowmediator
nsresult nsChromeRegistry::RefreshWindow(nsPIDOMWindowOuter* aWindow) {
  // Deal with our subframes first.
  nsDOMWindowList* frames = aWindow->GetFrames();
  uint32_t length = frames->GetLength();
  for (uint32_t j = 0; j < length; j++) {
    nsCOMPtr<nsPIDOMWindowOuter> piWindow = frames->IndexedGetter(j);
    RefreshWindow(piWindow);
  }

  nsresult rv;
  // Get the document.
  nsCOMPtr<nsIDocument> document = aWindow->GetDoc();
  if (!document) return NS_OK;

  // Deal with the agent sheets first.  Have to do all the style sets by hand.
  nsCOMPtr<nsIPresShell> shell = document->GetShell();
  if (shell) {
    // Reload only the chrome URL agent style sheets.
    nsTArray<RefPtr<StyleSheet>> agentSheets;
    rv = shell->GetAgentStyleSheets(agentSheets);
    NS_ENSURE_SUCCESS(rv, rv);

    nsTArray<RefPtr<StyleSheet>> newAgentSheets;
    for (StyleSheet* sheet : agentSheets) {
      nsIURI* uri = sheet->GetSheetURI();

      if (IsChromeURI(uri)) {
        // Reload the sheet.
        RefPtr<StyleSheet> newSheet;
        rv = document->LoadChromeSheetSync(uri, true, &newSheet);
        if (NS_FAILED(rv)) return rv;
        if (newSheet) {
          newAgentSheets.AppendElement(newSheet);
          return NS_OK;
        }
      } else {  // Just use the same sheet.
        rv = newAgentSheets.AppendElement(sheet) ? NS_OK : NS_ERROR_FAILURE;
        if (NS_FAILED(rv)) return rv;
      }
    }

    rv = shell->SetAgentStyleSheets(newAgentSheets);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  size_t count = document->SheetCount();

  // Build an array of style sheets we need to reload.
  nsTArray<RefPtr<StyleSheet>> oldSheets(count);
  nsTArray<RefPtr<StyleSheet>> newSheets(count);

  // Iterate over the style sheets.
  for (size_t i = 0; i < count; i++) {
    // Get the style sheet
    oldSheets.AppendElement(document->SheetAt(i));
  }

  // Iterate over our old sheets and kick off a sync load of the new
  // sheet if and only if it's a non-inline sheet with a chrome URL.
  for (StyleSheet* sheet : oldSheets) {
    MOZ_ASSERT(sheet,
               "SheetAt shouldn't return nullptr for "
               "in-range sheet indexes");
    nsIURI* uri = sheet->GetSheetURI();

    if (!sheet->IsInline() && IsChromeURI(uri)) {
      // Reload the sheet.
      RefPtr<StyleSheet> newSheet;
      // XXX what about chrome sheets that have a title or are disabled?  This
      // only works by sheer dumb luck.
      document->LoadChromeSheetSync(uri, false, &newSheet);
      // Even if it's null, we put in in there.
      newSheets.AppendElement(newSheet);
    } else {
      // Just use the same sheet.
      newSheets.AppendElement(sheet);
    }
  }

  // Now notify the document that multiple sheets have been added and removed.
  document->UpdateStyleSheets(oldSheets, newSheets);
  return NS_OK;
}

void nsChromeRegistry::FlushAllCaches() {
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  NS_ASSERTION(obsSvc, "Couldn't get observer service.");

  obsSvc->NotifyObservers((nsIChromeRegistry*)this, NS_CHROME_FLUSH_TOPIC,
                          nullptr);
}

NS_IMETHODIMP
nsChromeRegistry::AllowScriptsForPackage(nsIURI* aChromeURI, bool* aResult) {
  nsresult rv;
  *aResult = false;

#ifdef DEBUG
  bool isChrome;
  aChromeURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to AllowScriptsForPackage!");
#endif

  nsCOMPtr<nsIURL> url(do_QueryInterface(aChromeURI));
  NS_ENSURE_TRUE(url, NS_NOINTERFACE);

  nsAutoCString provider, file;
  rv = GetProviderAndPath(url, provider, file);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!provider.EqualsLiteral("skin")) *aResult = true;

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::AllowContentToAccess(nsIURI* aURI, bool* aResult) {
  nsresult rv;

  *aResult = false;

#ifdef DEBUG
  bool isChrome;
  aURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to AllowContentToAccess!");
#endif

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  if (!url) {
    NS_ERROR("Chrome URL doesn't implement nsIURL.");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString package;
  rv = url->GetHostPort(package);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t flags;
  rv = GetFlagsFromPackage(package, &flags);

  if (NS_SUCCEEDED(rv)) {
    *aResult = !!(flags & CONTENT_ACCESSIBLE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::CanLoadURLRemotely(nsIURI* aURI, bool* aResult) {
  nsresult rv;

  *aResult = false;

#ifdef DEBUG
  bool isChrome;
  aURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to CanLoadURLRemotely!");
#endif

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  if (!url) {
    NS_ERROR("Chrome URL doesn't implement nsIURL.");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString package;
  rv = url->GetHostPort(package);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t flags;
  rv = GetFlagsFromPackage(package, &flags);

  if (NS_SUCCEEDED(rv)) {
    *aResult = !!(flags & REMOTE_ALLOWED);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::MustLoadURLRemotely(nsIURI* aURI, bool* aResult) {
  nsresult rv;

  *aResult = false;

#ifdef DEBUG
  bool isChrome;
  aURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to MustLoadURLRemotely!");
#endif

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  if (!url) {
    NS_ERROR("Chrome URL doesn't implement nsIURL.");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString package;
  rv = url->GetHostPort(package);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t flags;
  rv = GetFlagsFromPackage(package, &flags);

  if (NS_SUCCEEDED(rv)) {
    *aResult = !!(flags & REMOTE_REQUIRED);
  }
  return NS_OK;
}

bool nsChromeRegistry::GetDirectionForLocale(const nsACString& aLocale) {
  int pref = mozilla::Preferences::GetInt("intl.uidirection", -1);
  if (pref >= 0) {
    return (pref > 0);
  }
  nsAutoCString locale(aLocale);
  SanitizeForBCP47(locale);
  return uloc_isRightToLeft(locale.get());
}

already_AddRefed<nsChromeRegistry> nsChromeRegistry::GetSingleton() {
  if (gChromeRegistry) {
    RefPtr<nsChromeRegistry> registry = gChromeRegistry;
    return registry.forget();
  }

  RefPtr<nsChromeRegistry> cr;
  if (GeckoProcessType_Content == XRE_GetProcessType())
    cr = new nsChromeRegistryContent();
  else
    cr = new nsChromeRegistryChrome();

  if (NS_FAILED(cr->Init())) return nullptr;

  return cr.forget();
}

void nsChromeRegistry::SanitizeForBCP47(nsACString& aLocale) {
  // Currently, the only locale code we use that's not BCP47-conformant is
  // "ja-JP-mac" on OS X, but let's try to be more general than just
  // hard-coding that here.
  const int32_t LANG_TAG_CAPACITY = 128;
  char langTag[LANG_TAG_CAPACITY];
  nsAutoCString locale(aLocale);
  UErrorCode err = U_ZERO_ERROR;
  // This is a fail-safe method that will set langTag to "und" if it cannot
  // match any part of the input locale code.
  int32_t len =
      uloc_toLanguageTag(locale.get(), langTag, LANG_TAG_CAPACITY, false, &err);
  if (U_SUCCESS(err) && len > 0) {
    aLocale.Assign(langTag, len);
  }
}
