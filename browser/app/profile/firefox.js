# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// XXX Toolkit-specific preferences should be moved into toolkit.js

#filter substitution

#
# SYNTAX HINTS:
#
#  - Dashes are delimiters; use underscores instead.
#  - The first character after a period must be alphabetic.
#  - Computed values (e.g. 50 * 1024) don't work.
#

#ifdef XP_UNIX
#ifndef XP_MACOSX
#define UNIX_BUT_NOT_MAC
#endif
#endif

pref("browser.hiddenWindowChromeURL", "chrome://browser/content/hiddenWindow.xul");

// Enables some extra Extension System Logging (can reduce performance)
pref("extensions.logging.enabled", false);

// Disables strict compatibility, making addons compatible-by-default.
pref("extensions.strictCompatibility", false);

// Temporary preference to forcibly make themes more safe with Australis even if
// extensions.checkCompatibility=false has been set.
pref("extensions.checkCompatibility.temporaryThemeOverride_minAppVersion", "29.0a1");

pref("extensions.webextPermissionPrompts", true);
pref("extensions.webextOptionalPermissionPrompts", true);

// Preferences for AMO integration
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/api/v3/addons/search/?guid=%IDS%&lang=%LOCALE%");
pref("extensions.getAddons.compatOverides.url", "https://services.addons.mozilla.org/api/v3/addons/compat-override/?guid=%IDS%&lang=%LOCALE%");
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/firefox/search?q=%TERMS%&platform=%OS%&appver=%VERSION%");
pref("extensions.webservice.discoverURL", "https://discovery.addons.mozilla.org/%LOCALE%/firefox/discovery/pane/%VERSION%/%OS%/%COMPATIBILITY_MODE%");
pref("extensions.getAddons.link.url", "https://addons.mozilla.org/%LOCALE%/firefox/");
pref("extensions.getAddons.langpacks.url", "https://services.addons.mozilla.org/api/v3/addons/language-tools/?app=firefox&type=language&appversion=%VERSION%");

pref("extensions.update.autoUpdateDefault", true);

// Check AUS for system add-on updates.
pref("extensions.systemAddon.update.url", "https://aus5.mozilla.org/update/3/SystemAddons/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");
pref("extensions.systemAddon.update.enabled", true);

// Disable add-ons that are not installed by the user in all scopes by default.
// See the SCOPE constants in AddonManager.jsm for values to use here.
pref("extensions.autoDisableScopes", 15);
// Scopes to scan for changes at startup.
pref("extensions.startupScanScopes", 0);

pref("extensions.geckoProfiler.acceptedExtensionIds", "geckoprofiler@mozilla.com,quantum-foxfooding@mozilla.com,raptor@mozilla.org");
#if defined(XP_LINUX) || defined (XP_MACOSX)
pref("extensions.geckoProfiler.getSymbolRules", "localBreakpad,nm");
#else // defined(XP_WIN)
pref("extensions.geckoProfiler.getSymbolRules", "localBreakpad,dump_syms.exe");
#endif


// Add-on content security policies.
pref("extensions.webextensions.base-content-security-policy", "script-src 'self' https://* moz-extension: blob: filesystem: 'unsafe-eval' 'unsafe-inline'; object-src 'self' https://* moz-extension: blob: filesystem:;");
pref("extensions.webextensions.default-content-security-policy", "script-src 'self'; object-src 'self';");

pref("extensions.webextensions.remote", true);
pref("extensions.webextensions.background-delayed-startup", true);

// Extensions that should not be flagged as legacy in about:addons
pref("extensions.legacy.exceptions", "testpilot@cliqz.com,@testpilot-containers,jid1-NeEaf3sAHdKHPA@jetpack,@activity-streams,pulse@mozilla.com,@testpilot-addon,@min-vid,tabcentertest1@mozilla.com,snoozetabs@mozilla.com,speaktome@mozilla.com,hoverpad@mozilla.com");

// Require signed add-ons by default
pref("extensions.langpacks.signatures.required", true);
pref("xpinstall.signatures.required", true);
pref("xpinstall.signatures.devInfoURL", "https://wiki.mozilla.org/Addons/Extension_Signing");

// Dictionary download preference
pref("browser.dictionaries.download.url", "https://addons.mozilla.org/%LOCALE%/firefox/dictionaries/");

// At startup, should we check to see if the installation
// date is older than some threshold
pref("app.update.checkInstallTime", true);

// The number of days a binary is permitted to be old without checking is defined in
// firefox-branding.js (app.update.checkInstallTime.days)

// The minimum delay in seconds for the timer to fire between the notification
// of each consumer of the timer manager.
// minimum=30 seconds, default=120 seconds, and maximum=300 seconds
pref("app.update.timerMinimumDelay", 120);

// The minimum delay in milliseconds for the first firing after startup of the timer
// to notify consumers of the timer manager.
// minimum=10 seconds, default=30 seconds, and maximum=120 seconds
pref("app.update.timerFirstInterval", 30000);

// App-specific update preferences

// The interval to check for updates (app.update.interval) is defined in
// firefox-branding.js

// Alternative windowtype for an application update user interface window. When
// a window with this windowtype is open the application update service won't
// open the normal application update user interface window.
pref("app.update.altwindowtype", "Browser:About");

// Enables some extra Application Update Logging (can reduce performance)
pref("app.update.log", false);

// The number of general background check failures to allow before notifying the
// user of the failure. User initiated update checks always notify the user of
// the failure.
pref("app.update.backgroundMaxErrors", 10);

// Whether or not to use the doorhanger application update UI.
pref("app.update.doorhanger", true);

// Ids of the links to the "What's new" update documentation
pref("app.update.link.updateAvailableWhatsNew", "update-available-whats-new");
pref("app.update.link.updateManualWhatsNew", "update-manual-whats-new");

// How many times we should let downloads fail before prompting the user to
// download a fresh installer.
pref("app.update.download.promptMaxAttempts", 2);

// How many times we should let an elevation prompt fail before prompting the user to
// download a fresh installer.
pref("app.update.elevation.promptMaxAttempts", 2);

// If set to true, the Update Service will automatically download updates if the
// user can apply updates. This pref is no longer used on Windows, except as the
// default value to migrate to the new location that this data is now stored
// (which is in a file in the update directory). Because of this, this pref
// should no longer be used directly. Instead,
// nsIUpdateService::getAutoUpdateIsEnabled and
// nsIUpdateService::setAutoUpdateIsEnabled should be used.
#ifndef XP_WIN
pref("app.update.auto", true);
#endif

// If set to true, the Update Service will present no UI for any event.
pref("app.update.silent", false);

// app.update.badgeWaitTime is in branding section

// If set to true, the Update Service will apply updates in the background
// when it finishes downloading them. Disabled in bug 1397562.
pref("app.update.staging.enabled", false);

// Update service URL:
pref("app.update.url", "https://aus5.mozilla.org/update/6/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%SYSTEM_CAPABILITIES%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");
// app.update.url.manual is in branding section
// app.update.url.details is in branding section

// app.update.interval is in branding section
// app.update.promptWaitTime is in branding section

// Show the Update Checking/Ready UI when the user was idle for x seconds
pref("app.update.idletime", 60);

// Whether or not to attempt using the service for updates.
#ifdef MOZ_MAINTENANCE_SERVICE
pref("app.update.service.enabled", true);
#endif

// Symmetric (can be overridden by individual extensions) update preferences.
// e.g.
//  extensions.{GUID}.update.enabled
//  extensions.{GUID}.update.url
//  .. etc ..
//
pref("extensions.update.enabled", true);
pref("extensions.update.url", "https://versioncheck.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");
pref("extensions.update.background.url", "https://versioncheck-bg.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");
pref("extensions.update.interval", 86400);  // Check for updates to Extensions and
                                            // Themes every day

pref("extensions.webextensions.themes.icons.buttons", "back,forward,reload,stop,bookmark_star,bookmark_menu,downloads,home,app_menu,cut,copy,paste,new_window,new_private_window,save_page,print,history,full_screen,find,options,addons,developer,synced_tabs,open_file,sidebars,share_page,subscribe,text_encoding,email_link,forget,pocket");

pref("lightweightThemes.update.enabled", true);
pref("lightweightThemes.getMoreURL", "https://addons.mozilla.org/%LOCALE%/firefox/themes");
pref("lightweightThemes.recommendedThemes", "[{\"id\":\"recommended-1\",\"homepageURL\":\"https://addons.mozilla.org/firefox/addon/a-web-browser-renaissance/\",\"headerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/1.header.jpg\",\"textcolor\":\"#000000\",\"accentcolor\":\"#834d29\",\"iconURL\":\"resource:///chrome/browser/content/browser/defaultthemes/1.icon.jpg\",\"previewURL\":\"resource:///chrome/browser/content/browser/defaultthemes/1.preview.jpg\",\"author\":\"Sean.Martell\",\"version\":\"0\"},{\"id\":\"recommended-2\",\"homepageURL\":\"https://addons.mozilla.org/firefox/addon/space-fantasy/\",\"headerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/2.header.jpg\",\"textcolor\":\"#ffffff\",\"accentcolor\":\"#d9d9d9\",\"iconURL\":\"resource:///chrome/browser/content/browser/defaultthemes/2.icon.jpg\",\"previewURL\":\"resource:///chrome/browser/content/browser/defaultthemes/2.preview.jpg\",\"author\":\"fx5800p\",\"version\":\"1.0\"},{\"id\":\"recommended-4\",\"homepageURL\":\"https://addons.mozilla.org/firefox/addon/pastel-gradient/\",\"headerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/4.header.png\",\"textcolor\":\"#000000\",\"accentcolor\":\"#000000\",\"iconURL\":\"resource:///chrome/browser/content/browser/defaultthemes/4.icon.png\",\"previewURL\":\"resource:///chrome/browser/content/browser/defaultthemes/4.preview.png\",\"author\":\"darrinhenein\",\"version\":\"1.0\"}]");

#if defined(MOZ_WIDEVINE_EME)
pref("browser.eme.ui.enabled", true);
#else
pref("browser.eme.ui.enabled", false);
#endif

// UI tour experience.
pref("browser.uitour.enabled", true);
pref("browser.uitour.loglevel", "Error");
pref("browser.uitour.requireSecure", true);
pref("browser.uitour.themeOrigin", "https://addons.mozilla.org/%LOCALE%/firefox/themes/");
pref("browser.uitour.url", "https://www.mozilla.org/%LOCALE%/firefox/%VERSION%/tour/");
// How long to show a Hearbeat survey (two hours, in seconds)
pref("browser.uitour.surveyDuration", 7200);

pref("keyword.enabled", true);
pref("browser.fixup.domainwhitelist.localhost", true);

pref("general.smoothScroll", true);
#ifdef UNIX_BUT_NOT_MAC
pref("general.autoScroll", false);
#else
pref("general.autoScroll", true);
#endif

pref("browser.stopReloadAnimation.enabled", true);
pref("browser.schedulePressure.enabled", true);
pref("browser.schedulePressure.defaultCount", 3);
pref("browser.schedulePressure.timeoutMs", 300);

// UI density of the browser chrome. This mostly affects toolbarbutton
// and urlbar spacing. The possible values are 0=normal, 1=compact, 2=touch.
pref("browser.uidensity", 0);
// Whether Firefox will automatically override the uidensity to "touch"
// while the user is in a touch environment (such as Windows tablet mode).
pref("browser.touchmode.auto", true);

// At startup, check if we're the default browser and prompt user if not.
pref("browser.shell.checkDefaultBrowser", true);
pref("browser.shell.shortcutFavicons",true);
pref("browser.shell.mostRecentDateSetAsDefault", "");
pref("browser.shell.skipDefaultBrowserCheckOnFirstRun", true);
pref("browser.shell.didSkipDefaultBrowserCheckOnFirstRun", false);
pref("browser.shell.defaultBrowserCheckCount", 0);
pref("browser.defaultbrowser.notificationbar", false);

// 0 = blank, 1 = home (browser.startup.homepage), 2 = last visited page, 3 = resume previous browser session
// The behavior of option 3 is detailed at: http://wiki.mozilla.org/Session_Restore
pref("browser.startup.page",                1);
pref("browser.startup.homepage",            "about:home");
// Whether we should skip the homepage when opening the first-run page
pref("browser.startup.firstrunSkipsHomepage", true);

// Show an about:blank window as early as possible for quick startup feedback.
// Held to nightly on Linux due to bug 1450626.
// Disabled on Mac because the bouncing dock icon already provides feedback.
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) && defined(NIGHTLY_BUILD)
pref("browser.startup.blankWindow", true);
#else
pref("browser.startup.blankWindow", false);
#endif

pref("browser.slowStartup.notificationDisabled", false);
pref("browser.slowStartup.timeThreshold", 20000);
pref("browser.slowStartup.maxSamples", 5);

// This url, if changed, MUST continue to point to an https url. Pulling arbitrary content to inject into
// this page over http opens us up to a man-in-the-middle attack that we'd rather not face. If you are a downstream
// repackager of this code using an alternate snippet url, please keep your users safe
pref("browser.aboutHomeSnippets.updateUrl", "https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/");

pref("browser.enable_automatic_image_resizing", true);
pref("browser.chrome.site_icons", true);
// browser.warnOnQuit == false will override all other possible prompts when quitting or restarting
pref("browser.warnOnQuit", true);
pref("browser.fullscreen.autohide", true);
pref("browser.overlink-delay", 80);

#ifdef UNIX_BUT_NOT_MAC
pref("browser.urlbar.clickSelectsAll", false);
#else
pref("browser.urlbar.clickSelectsAll", true);
#endif
#ifdef UNIX_BUT_NOT_MAC
pref("browser.urlbar.doubleClickSelectsAll", true);
#else
pref("browser.urlbar.doubleClickSelectsAll", false);
#endif

// Whether using `ctrl` when hitting return/enter in the URL bar
// (or clicking 'go') should prefix 'www.' and suffix
// browser.fixup.alternate.suffix to the URL bar value prior to
// navigating.
pref("browser.urlbar.ctrlCanonizesURLs", true);

// Control autoFill behavior
pref("browser.urlbar.autoFill", true);
pref("browser.urlbar.speculativeConnect.enabled", true);

// Whether bookmarklets should be filtered out of Address Bar matches.
// This is enabled for security reasons, when true it is still possible to
// search for bookmarklets typing "javascript: " followed by the actual query.
pref("browser.urlbar.filter.javascript", true);

// the maximum number of results to show in autocomplete when doing richResults
pref("browser.urlbar.maxRichResults", 10);
// The amount of time (ms) to wait after the user has stopped typing
// before starting to perform autocomplete.  50 is the default set in
// autocomplete.xml.
pref("browser.urlbar.delay", 50);

// The maximum number of historical search results to show.
pref("browser.urlbar.maxHistoricalSearchSuggestions", 0);

// The default behavior for the urlbar can be configured to use any combination
// of the match filters with each additional filter adding more results (union).
pref("browser.urlbar.suggest.history",              true);
pref("browser.urlbar.suggest.bookmark",             true);
pref("browser.urlbar.suggest.openpage",             true);
pref("browser.urlbar.suggest.searches",             true);

// Whether the user made a choice in the old search suggestions opt-in bar.
pref("browser.urlbar.userMadeSearchSuggestionsChoice", false);
// The suggestion opt-out hint will be hidden after being shown 4 times.
pref("browser.urlbar.timesBeforeHidingSuggestionsHint", 4);

// Limit the number of characters sent to the current search engine to fetch
// suggestions.
pref("browser.urlbar.maxCharsForSearchSuggestions", 20);

pref("browser.urlbar.formatting.enabled", true);
pref("browser.urlbar.trimURLs", true);

pref("browser.urlbar.oneOffSearches", true);

// If changed to true, copying the entire URL from the location bar will put the
// human readable (percent-decoded) URL on the clipboard.
pref("browser.urlbar.decodeURLsOnCopy", false);

// Whether or not to move tabs into the active window when using the "Switch to
// Tab" feature of the awesomebar.
pref("browser.urlbar.switchTabs.adoptIntoActiveWindow", false);

// Whether addresses and search results typed into the address bar
// should be opened in new tabs by default.
pref("browser.urlbar.openintab", false);

// This is disabled until Bug 1340663 figures out the remaining requirements.
pref("browser.urlbar.usepreloadedtopurls.enabled", false);
pref("browser.urlbar.usepreloadedtopurls.expire_days", 14);

// Toggle the new work in progress Address Bar code.
pref("browser.urlbar.quantumbar", false);

pref("browser.altClickSave", false);

// Enable logging downloads operations to the Console.
pref("browser.download.loglevel", "Error");

// Number of milliseconds to wait for the http headers (and thus
// the Content-Disposition filename) before giving up and falling back to
// picking a filename without that info in hand so that the user sees some
// feedback from their action.
pref("browser.download.saveLinkAsFilenameTimeout", 4000);

pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 1);
pref("browser.download.manager.addToRecentDocs", true);
pref("browser.download.manager.resumeOnWakeDelay", 10000);

// This allows disabling the animated notifications shown by
// the Downloads Indicator when a download starts or completes.
pref("browser.download.animateNotifications", true);

// This records whether or not the panel has been shown at least once.
pref("browser.download.panel.shown", false);

// This controls whether the button is automatically shown/hidden depending
// on whether there are downloads to show.
pref("browser.download.autohideButton", true);

#ifndef XP_MACOSX
pref("browser.helperApps.deleteTempFileOnExit", true);
#endif

// search engines URL
pref("browser.search.searchEnginesURL",      "https://addons.mozilla.org/%LOCALE%/firefox/search-engines/");

// Market-specific search defaults
pref("browser.search.geoSpecificDefaults", true);
pref("browser.search.geoSpecificDefaults.url", "https://search.services.mozilla.com/1/%APP%/%VERSION%/%CHANNEL%/%LOCALE%/%REGION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%");

// search bar results always open in a new tab
pref("browser.search.openintab", false);

// context menu searches open in the foreground
pref("browser.search.context.loadInBackground", false);

// comma seperated list of of engines to hide in the search panel.
pref("browser.search.hiddenOneOffs", "");

// Mirrors whether the search-container widget is in the navigation toolbar.
pref("browser.search.widget.inNavBar", false);

#ifndef RELEASE_OR_BETA
pref("browser.search.reset.enabled", true);
#endif

pref("browser.sessionhistory.max_entries", 50);

// Built-in default permissions.
pref("permissions.manager.defaultsUrl", "resource://app/defaults/permissions");

// Set default fallback values for site permissions we want
// the user to be able to globally change.
pref("permissions.default.camera", 0);
pref("permissions.default.microphone", 0);
pref("permissions.default.geo", 0);
pref("permissions.default.desktop-notification", 0);
pref("permissions.default.shortcuts", 0);

// handle links targeting new windows
// 1=current window/tab, 2=new window, 3=new tab in most recent window
pref("browser.link.open_newwindow", 3);

// handle external links (i.e. links opened from a different application)
// default: use browser.link.open_newwindow
// 1-3: see browser.link.open_newwindow for interpretation
pref("browser.link.open_newwindow.override.external", -1);

// 0: no restrictions - divert everything
// 1: don't divert window.open at all
// 2: don't divert window.open with features
pref("browser.link.open_newwindow.restriction", 2);

// If true, this pref causes windows opened by window.open to be forced into new
// tabs (rather than potentially opening separate windows, depending on
// window.open arguments) when the browser is in fullscreen mode.
// We set this differently on Mac because the fullscreen implementation there is
// different.
#ifdef XP_MACOSX
pref("browser.link.open_newwindow.disabled_in_fullscreen", true);
#else
pref("browser.link.open_newwindow.disabled_in_fullscreen", false);
#endif

// Tabbed browser
pref("browser.tabs.multiselect", true);
pref("browser.tabs.20FpsThrobber", false);
pref("browser.tabs.30FpsThrobber", false);
pref("browser.tabs.closeTabByDblclick", false);
pref("browser.tabs.closeWindowWithLastTab", true);
// Open related links to a tab, e.g., link in current tab, at next to the
// current tab if |insertRelatedAfterCurrent| is true.  Otherwise, always
// append new tab to the end.
pref("browser.tabs.insertRelatedAfterCurrent", true);
// Open all links, e.g., bookmarks, history items at next to current tab
// if |insertAfterCurrent| is true.  Otherwise, append new tab to the end
// for non-related links. Note that if this is set to true, it will trump
// the value of browser.tabs.insertRelatedAfterCurrent.
pref("browser.tabs.insertAfterCurrent", false);
pref("browser.tabs.warnOnClose", true);
pref("browser.tabs.warnOnCloseOtherTabs", true);
pref("browser.tabs.warnOnOpen", true);
pref("browser.tabs.maxOpenBeforeWarn", 15);
pref("browser.tabs.loadInBackground", true);
pref("browser.tabs.opentabfor.middleclick", true);
pref("browser.tabs.loadDivertedInBackground", false);
pref("browser.tabs.loadBookmarksInBackground", false);
pref("browser.tabs.loadBookmarksInTabs", false);
pref("browser.tabs.tabClipWidth", 140);
pref("browser.tabs.tabMinWidth", 76);
#ifdef UNIX_BUT_NOT_MAC
pref("browser.tabs.drawInTitlebar", false);
#else
pref("browser.tabs.drawInTitlebar", true);
#endif

// Offer additional drag space to the user. The drag space
// will only be shown if browser.tabs.drawInTitlebar is true.
pref("browser.tabs.extraDragSpace", false);

// When tabs opened by links in other tabs via a combination of
// browser.link.open_newwindow being set to 3 and target="_blank" etc are
// closed:
// true   return to the tab that opened this tab (its owner)
// false  return to the adjacent tab (old default)
pref("browser.tabs.selectOwnerOnClose", true);

pref("browser.tabs.showAudioPlayingIcon", true);
// This should match Chromium's audio indicator delay.
pref("browser.tabs.delayHidingAudioPlayingIconMS", 3000);

// Pref to control whether we use separate privileged content processes.
#if defined(NIGHTLY_BUILD) && !defined(MOZ_ASAN)
pref("browser.tabs.remote.separatePrivilegedContentProcess", true);
#endif

pref("browser.ctrlTab.recentlyUsedOrder", true);

// By default, do not export HTML at shutdown.
// If true, at shutdown the bookmarks in your menu and toolbar will
// be exported as HTML to the bookmarks.html file.
pref("browser.bookmarks.autoExportHTML",          false);

// The maximum number of daily bookmark backups to
// keep in {PROFILEDIR}/bookmarkbackups. Special values:
// -1: unlimited
//  0: no backups created (and deletes all existing backups)
pref("browser.bookmarks.max_backups",             15);

// Whether menu should close after Ctrl-click, middle-click, etc.
pref("browser.bookmarks.openInTabClosesMenu", true);

// Scripts & Windows prefs
pref("dom.disable_open_during_load",              true);
pref("javascript.options.showInConsole",          true);
#ifdef DEBUG
pref("general.warnOnAboutConfig",                 false);
#endif

// This is the pref to control the location bar, change this to true to
// force this - this makes the origin of popup windows more obvious to avoid
// spoofing. We would rather not do it by default because it affects UE for web
// applications, but without it there isn't a really good way to prevent chrome
// spoofing, see bug 337344
pref("dom.disable_window_open_feature.location",  true);
// allow JS to move and resize existing windows
pref("dom.disable_window_move_resize",            false);
// prevent JS from monkeying with window focus, etc
pref("dom.disable_window_flip",                   true);

// popups.policy 1=allow,2=reject
pref("privacy.popups.policy",               1);
pref("privacy.popups.usecustom",            true);
pref("privacy.popups.showBrowserMessage",   true);

pref("privacy.item.cookies",                false);

pref("privacy.clearOnShutdown.history",     true);
pref("privacy.clearOnShutdown.formdata",    true);
pref("privacy.clearOnShutdown.downloads",   true);
pref("privacy.clearOnShutdown.cookies",     true);
pref("privacy.clearOnShutdown.cache",       true);
pref("privacy.clearOnShutdown.sessions",    true);
pref("privacy.clearOnShutdown.offlineApps", false);
pref("privacy.clearOnShutdown.siteSettings", false);
pref("privacy.clearOnShutdown.openWindows", false);

pref("privacy.cpd.history",                 true);
pref("privacy.cpd.formdata",                true);
pref("privacy.cpd.passwords",               false);
pref("privacy.cpd.downloads",               true);
pref("privacy.cpd.cookies",                 true);
pref("privacy.cpd.cache",                   true);
pref("privacy.cpd.sessions",                true);
pref("privacy.cpd.offlineApps",             false);
pref("privacy.cpd.siteSettings",            false);
pref("privacy.cpd.openWindows",             false);

pref("privacy.history.custom",              false);

// What default should we use for the time span in the sanitizer:
// 0 - Clear everything
// 1 - Last Hour
// 2 - Last 2 Hours
// 3 - Last 4 Hours
// 4 - Today
// 5 - Last 5 minutes
// 6 - Last 24 hours
pref("privacy.sanitize.timeSpan", 1);
pref("privacy.sanitize.sanitizeOnShutdown", false);

pref("privacy.sanitize.migrateFx3Prefs",    false);

pref("privacy.panicButton.enabled",         true);

// Time until temporary permissions expire, in ms
pref("privacy.temporary_permission_expire_time_ms",  3600000);

// If Accept-Language should be spoofed by en-US
// 0 - will prompt
// 1 - don't spoof
// 2 - spoof
pref("privacy.spoof_english", 0);

pref("network.proxy.share_proxy_settings",  false); // use the same proxy settings for all protocols

// simple gestures support
pref("browser.gesture.swipe.left", "Browser:BackOrBackDuplicate");
pref("browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate");
pref("browser.gesture.swipe.up", "cmd_scrollTop");
pref("browser.gesture.swipe.down", "cmd_scrollBottom");
#ifdef XP_MACOSX
pref("browser.gesture.pinch.latched", true);
pref("browser.gesture.pinch.threshold", 150);
#else
pref("browser.gesture.pinch.latched", false);
pref("browser.gesture.pinch.threshold", 25);
#endif
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
// Enabled for touch input display zoom.
pref("browser.gesture.pinch.out", "cmd_fullZoomEnlarge");
pref("browser.gesture.pinch.in", "cmd_fullZoomReduce");
pref("browser.gesture.pinch.out.shift", "cmd_fullZoomReset");
pref("browser.gesture.pinch.in.shift", "cmd_fullZoomReset");
#else
// Disabled by default due to issues with track pad input.
pref("browser.gesture.pinch.out", "");
pref("browser.gesture.pinch.in", "");
pref("browser.gesture.pinch.out.shift", "");
pref("browser.gesture.pinch.in.shift", "");
#endif
pref("browser.gesture.twist.latched", false);
pref("browser.gesture.twist.threshold", 0);
pref("browser.gesture.twist.right", "cmd_gestureRotateRight");
pref("browser.gesture.twist.left", "cmd_gestureRotateLeft");
pref("browser.gesture.twist.end", "cmd_gestureRotateEnd");
pref("browser.gesture.tap", "cmd_fullZoomReset");

pref("browser.history_swipe_animation.disabled", false);

// 0: Nothing happens
// 1: Scrolling contents
// 2: Go back or go forward, in your history
// 3: Zoom in or out (reflowing zoom).
// 4: Treat vertical wheel as horizontal scroll
// 5: Zoom in or out (pinch zoom).
#ifdef XP_MACOSX
// On macOS, if the wheel has one axis only, shift+wheel comes through as a
// horizontal scroll event. Thus, we can't assign anything other than normal
// scrolling to shift+wheel.
pref("mousewheel.with_shift.action", 1);
pref("mousewheel.with_alt.action", 2);
// On MacOS X, control+wheel is typically handled by system and we don't
// receive the event.  So, command key which is the main modifier key for
// acceleration is the best modifier for zoom-in/out.  However, we should keep
// the control key setting for backward compatibility.
pref("mousewheel.with_meta.action", 3); // command key on Mac
// Disable control-/meta-modified horizontal wheel events, since those are
// used on Mac as part of modified swipe gestures (e.g. Left swipe+Cmd is
// "go back" in a new tab).
pref("mousewheel.with_control.action.override_x", 0);
pref("mousewheel.with_meta.action.override_x", 0);
#else
// On the other platforms (non-macOS), user may use legacy mouse which supports
// only vertical wheel but want to scroll horizontally.  For such users, we
// should provide horizontal scroll with shift+wheel (same as Chrome).
// However, shift+wheel was used for navigating history.  For users who want
// to keep using this feature, let's enable it with alt+wheel.  This is better
// for consistency with macOS users.
pref("mousewheel.with_shift.action", 4);
pref("mousewheel.with_alt.action", 2);
pref("mousewheel.with_meta.action", 1); // win key on Win, Super/Hyper on Linux
#endif
pref("mousewheel.with_control.action",3);
pref("mousewheel.with_win.action", 1);

pref("browser.xul.error_pages.enabled", true);
pref("browser.xul.error_pages.expert_bad_cert", false);
pref("browser.xul.error_pages.show_safe_browsing_details_on_load", false);

// Enable captive portal detection.
pref("network.captive-portal-service.enabled", true);

// If true, network link events will change the value of navigator.onLine
pref("network.manage-offline-status", true);

// We want to make sure mail URLs are handled externally...
pref("network.protocol-handler.external.mailto", true); // for mail
pref("network.protocol-handler.external.news", true);   // for news
pref("network.protocol-handler.external.snews", true);  // for secure news
pref("network.protocol-handler.external.nntp", true);   // also news
#ifdef XP_WIN
pref("network.protocol-handler.external.ms-windows-store", true);
#endif

// ...without warning dialogs
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.news", false);
pref("network.protocol-handler.warn-external.snews", false);
pref("network.protocol-handler.warn-external.nntp", false);
#ifdef XP_WIN
pref("network.protocol-handler.warn-external.ms-windows-store", false);
#endif

// By default, all protocol handlers are exposed.  This means that
// the browser will respond to openURL commands for all URL types.
// It will also try to open link clicks inside the browser before
// failing over to the system handlers.
pref("network.protocol-handler.expose-all", true);
pref("network.protocol-handler.expose.mailto", false);
pref("network.protocol-handler.expose.news", false);
pref("network.protocol-handler.expose.snews", false);
pref("network.protocol-handler.expose.nntp", false);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.flashBar", 1);

// Accessibility indicator preferences such as support URL, enabled flag.
pref("accessibility.support.url", "https://support.mozilla.org/%LOCALE%/kb/accessibility-services");
pref("accessibility.indicator.enabled", false);

pref("plugins.click_to_play", true);
pref("plugins.testmode", false);

// Should plugins that are hidden show the infobar UI?
pref("plugins.show_infobar", false);

pref("plugin.default.state", 1);

// Plugins bundled in XPIs are enabled by default.
pref("plugin.defaultXpi.state", 2);


// Flash is Click-to-Activate by default on all channels.
pref("plugin.state.flash", 1);

// Enables the download and use of the flash blocklists.
pref("plugins.flashBlock.enabled", true);

// Prefer HTML5 video over Flash content, and don't
// load plugin instances with no src declared.
// These prefs are documented in details on all.js.
// With the "follow-ctp" setting, this will only
// apply to users that have plugin.state.flash = 1.
pref("plugins.favorfallback.mode", "follow-ctp");
pref("plugins.favorfallback.rules", "nosrc,video");


#ifdef XP_WIN
pref("browser.preferences.instantApply", false);
#else
pref("browser.preferences.instantApply", true);
#endif

// Toggling Search bar on and off in about:preferences
pref("browser.preferences.search", true);

pref("browser.preferences.defaultPerformanceSettings.enabled", true);

pref("browser.download.show_plugins_in_list", true);
pref("browser.download.hide_plugins_without_extensions", true);

// Backspace and Shift+Backspace behavior
// 0 goes Back/Forward
// 1 act like PgUp/PgDown
// 2 and other values, nothing
#ifdef UNIX_BUT_NOT_MAC
pref("browser.backspace_action", 2);
#else
pref("browser.backspace_action", 0);
#endif

// this will automatically enable inline spellchecking (if it is available) for
// editable elements in HTML
// 0 = spellcheck nothing
// 1 = check multi-line controls [default]
// 2 = check multi/single line controls
pref("layout.spellcheckDefault", 1);

pref("browser.send_pings", false);

// At startup, if the handler service notices that the version number in the
// region.properties file is newer than the version number in the handler
// service datastore, it will add any new handlers it finds in the prefs (as
// seeded by this file) to its datastore.
pref("gecko.handlerService.defaultHandlersVersion", "chrome://browser-region/locale/region.properties");

// The default set of web-based protocol handlers shown in the application
// selection dialog for webcal: ; I've arbitrarily picked 4 default handlers
// per protocol, but if some locale wants more than that (or defaults for some
// protocol not currently listed here), we should go ahead and add those.

// webcal
pref("gecko.handlerService.schemes.webcal.0.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.0.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.1.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.1.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.2.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.2.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.3.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.3.uriTemplate", "chrome://browser-region/locale/region.properties");

// mailto
pref("gecko.handlerService.schemes.mailto.0.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.0.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.1.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.1.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.2.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.2.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.3.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.3.uriTemplate", "chrome://browser-region/locale/region.properties");

// irc
pref("gecko.handlerService.schemes.irc.0.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.0.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.1.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.1.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.2.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.2.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.3.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.3.uriTemplate", "chrome://browser-region/locale/region.properties");

// ircs
pref("gecko.handlerService.schemes.ircs.0.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.0.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.1.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.1.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.2.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.2.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.3.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.3.uriTemplate", "chrome://browser-region/locale/region.properties");

pref("browser.geolocation.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/geolocation/");

pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.resume_session_once", false);
pref("browser.sessionstore.resuming_after_os_restart", false);

// Minimal interval between two save operations in milliseconds (while the user is active).
pref("browser.sessionstore.interval", 15000); // 15 seconds

// Minimal interval between two save operations in milliseconds (while the user is idle).
pref("browser.sessionstore.interval.idle", 3600000); // 1h

// Time (ms) before we assume that the user is idle and that we don't need to
// collect/save the session quite as often.
pref("browser.sessionstore.idleDelay", 180000); // 3 minutes

// on which sites to save text data, POSTDATA and cookies
// 0 = everywhere, 1 = unencrypted sites, 2 = nowhere
pref("browser.sessionstore.privacy_level", 0);
// how many tabs can be reopened (per window)
pref("browser.sessionstore.max_tabs_undo", 10);
// how many windows can be reopened (per session) - on non-OS X platforms this
// pref may be ignored when dealing with pop-up windows to ensure proper startup
pref("browser.sessionstore.max_windows_undo", 3);
// number of crashes that can occur before the about:sessionrestore page is displayed
// (this pref has no effect if more than 6 hours have passed since the last crash)
pref("browser.sessionstore.max_resumed_crashes", 1);
// number of back button session history entries to restore (-1 = all of them)
pref("browser.sessionstore.max_serialize_back", 10);
// number of forward button session history entries to restore (-1 = all of them)
pref("browser.sessionstore.max_serialize_forward", -1);
// restore_on_demand overrides MAX_CONCURRENT_TAB_RESTORES (sessionstore constant)
// and restore_hidden_tabs. When true, tabs will not be restored until they are
// focused (also applies to tabs that aren't visible). When false, the values
// for MAX_CONCURRENT_TAB_RESTORES and restore_hidden_tabs are respected.
// Selected tabs are always restored regardless of this pref.
pref("browser.sessionstore.restore_on_demand", true);
// Whether to automatically restore hidden tabs (i.e., tabs in other tab groups) or not
pref("browser.sessionstore.restore_hidden_tabs", false);
// If restore_on_demand is set, pinned tabs are restored on startup by default.
// When set to true, this pref overrides that behavior, and pinned tabs will only
// be restored when they are focused.
pref("browser.sessionstore.restore_pinned_tabs_on_demand", false);
// The version at which we performed the latest upgrade backup
pref("browser.sessionstore.upgradeBackup.latestBuildID", "");
// How many upgrade backups should be kept
pref("browser.sessionstore.upgradeBackup.maxUpgradeBackups", 3);
// End-users should not run sessionstore in debug mode
pref("browser.sessionstore.debug", false);
// Causes SessionStore to ignore non-final update messages from
// browser tabs that were not caused by a flush from the parent.
// This is a testing flag and should not be used by end-users.
pref("browser.sessionstore.debug.no_auto_updates", false);
// Forget closed windows/tabs after two weeks
pref("browser.sessionstore.cleanup.forget_closed_after", 1209600000);
// Maximum number of bytes of DOMSessionStorage data we collect per origin.
pref("browser.sessionstore.dom_storage_limit", 2048);
// Amount of failed SessionFile writes until we restart the worker.
pref("browser.sessionstore.max_write_failures", 5);

// allow META refresh by default
pref("accessibility.blockautorefresh", false);

// Whether history is enabled or not.
pref("places.history.enabled", true);

// the (maximum) number of the recent visits to sample
// when calculating frecency
pref("places.frecency.numVisits", 10);

// buckets (in days) for frecency calculation
pref("places.frecency.firstBucketCutoff", 4);
pref("places.frecency.secondBucketCutoff", 14);
pref("places.frecency.thirdBucketCutoff", 31);
pref("places.frecency.fourthBucketCutoff", 90);

// weights for buckets for frecency calculations
pref("places.frecency.firstBucketWeight", 100);
pref("places.frecency.secondBucketWeight", 70);
pref("places.frecency.thirdBucketWeight", 50);
pref("places.frecency.fourthBucketWeight", 30);
pref("places.frecency.defaultBucketWeight", 10);

// bonus (in percent) for visit transition types for frecency calculations
pref("places.frecency.embedVisitBonus", 0);
pref("places.frecency.framedLinkVisitBonus", 0);
pref("places.frecency.linkVisitBonus", 100);
pref("places.frecency.typedVisitBonus", 2000);
// The bookmarks bonus is always added on top of any other bonus, including
// the redirect source and the typed ones.
pref("places.frecency.bookmarkVisitBonus", 75);
// The redirect source bonus overwrites any transition bonus.
// 0 would hide these pages, instead we want them low ranked.  Thus we use
// linkVisitBonus - bookmarkVisitBonus, so that a bookmarked source is in par
// with a common link.
pref("places.frecency.redirectSourceVisitBonus", 25);
pref("places.frecency.downloadVisitBonus", 0);
// The perm/temp redirects here relate to redirect targets, not sources.
pref("places.frecency.permRedirectVisitBonus", 50);
pref("places.frecency.tempRedirectVisitBonus", 40);
pref("places.frecency.reloadVisitBonus", 0);
pref("places.frecency.defaultVisitBonus", 0);

// bonus (in percent) for place types for frecency calculations
pref("places.frecency.unvisitedBookmarkBonus", 140);
pref("places.frecency.unvisitedTypedBonus", 200);

// Controls behavior of the "Add Exception" dialog launched from SSL error pages
// 0 - don't pre-populate anything
// 1 - pre-populate site URL, but don't fetch certificate
// 2 - pre-populate site URL and pre-fetch certificate
pref("browser.ssl_override_behavior", 2);

// True if the user should be prompted when a web application supports
// offline apps.
pref("browser.offline-apps.notify", true);

// if true, use full page zoom instead of text zoom
pref("browser.zoom.full", true);

// Whether or not to save and restore zoom levels on a per-site basis.
pref("browser.zoom.siteSpecific", true);

// Whether or not to update background tabs to the current zoom level.
pref("browser.zoom.updateBackgroundTabs", true);

// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.com/report/index/");

// URL for "Learn More" for DataCollection
pref("toolkit.datacollection.infoURL",
     "https://www.mozilla.org/legal/privacy/firefox.html");

// URL for "Learn More" for Crash Reporter
pref("toolkit.crashreporter.infoURL",
     "https://www.mozilla.org/legal/privacy/firefox.html#crash-reporter");

// base URL for web-based support pages
pref("app.support.baseURL", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/");

// base url for web-based feedback pages
#ifdef MOZ_DEV_EDITION
pref("app.feedback.baseURL", "https://input.mozilla.org/%LOCALE%/feedback/firefoxdev/%VERSION%/");
#else
pref("app.feedback.baseURL", "https://input.mozilla.org/%LOCALE%/feedback/%APP%/%VERSION%/");
#endif

// base URL for web-based marketing pages
pref("app.productInfo.baseURL", "https://www.mozilla.org/firefox/features/");

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

// Enable the new certificate error pages.
#ifdef EARLY_BETA_OR_EARLIER
pref("browser.security.newcerterrorpage.enabled", true);
#else
pref("browser.security.newcerterrorpage.enabled", false);
#endif

pref("security.certerrors.recordEventTelemetry", true);

// Whether to start the private browsing mode at application startup
pref("browser.privatebrowsing.autostart", false);

// Whether the bookmark panel should be shown when bookmarking a page.
pref("browser.bookmarks.editDialog.showForNewBookmarks", true);

// Don't try to alter this pref, it'll be reset the next time you use the
// bookmarking dialog
pref("browser.bookmarks.editDialog.firstEditField", "namePicker");

pref("dom.ipc.plugins.flash.disable-protected-mode", false);

// Feature-disable the protected-mode auto-flip
pref("browser.flash-protected-mode-flip.enable", false);

// Whether we've already flipped protected mode automatically
pref("browser.flash-protected-mode-flip.done", false);

pref("dom.ipc.shims.enabledWarnings", false);

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
// Controls whether and how the Windows NPAPI plugin process is sandboxed.
// To get a different setting for a particular plugin replace "default", with
// the plugin's nice file name, see: nsPluginTag::GetNiceFileName.
// On windows these levels are:
// 0 - no sandbox
// 1 - sandbox with USER_NON_ADMIN access token level
// 2 - a more strict sandbox, which might cause functionality issues. This now
//     includes running at low integrity.
// 3 - the strongest settings we seem to be able to use without breaking
//     everything, but will probably cause some functionality restrictions
pref("dom.ipc.plugins.sandbox-level.default", 0);
#if defined(_AMD64_)
// The base sandbox level in nsPluginTag::InitSandboxLevel must be
// updated to keep in sync with this value.
pref("dom.ipc.plugins.sandbox-level.flash", 3);
#else
pref("dom.ipc.plugins.sandbox-level.flash", 0);
#endif

#if defined(MOZ_CONTENT_SANDBOX)
// This controls the strength of the Windows content process sandbox for testing
// purposes. This will require a restart.
// On windows these levels are:
// See - security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp
// SetSecurityLevelForContentProcess() for what the different settings mean.
pref("security.sandbox.content.level", 5);

// This controls the depth of stack trace that is logged when Windows sandbox
// logging is turned on.  This is only currently available for the content
// process because the only other sandbox (for GMP) has too strict a policy to
// allow stack tracing.  This does not require a restart to take effect.
pref("security.sandbox.windows.log.stackTraceDepth", 0);
#endif

// This controls the strength of the Windows GPU process sandbox.  Changes
// will require restart.
// For information on what the level number means, see
// SetSecurityLevelForGPUProcess() in
// security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp
pref("security.sandbox.gpu.level", 0);

// Controls whether we disable win32k for the GMP processes.
// true means that win32k system calls are not permitted.
// Note: win32k is currently _not_ disabled due to intermittent test failures,
// where the GMP process fails very early. See bug 1449348.
pref("security.sandbox.gmp.win32k-disable", false);
#endif

#if defined(NIGHTLY_BUILD) && defined(XP_MACOSX) && defined(MOZ_SANDBOX)
// Start the Mac sandbox early during child process startup instead
// of when messaged by the parent after the message loop is running.
pref("security.sandbox.content.mac.earlyinit", true);
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX) && defined(MOZ_CONTENT_SANDBOX)
// This pref is discussed in bug 1083344, the naming is inspired from its
// Windows counterpart, but on Mac it's an integer which means:
// 0 -> "no sandbox" (nightly only)
// 1 -> "preliminary content sandboxing enabled: write access to
//       home directory is prevented"
// 2 -> "preliminary content sandboxing enabled with profile protection:
//       write access to home directory is prevented, read and write access
//       to ~/Library and profile directories are prevented (excluding
//       $PROFILE/{extensions,chrome})"
// 3 -> "no global read/write access, read access permitted to
//       $PROFILE/{extensions,chrome}"
// This setting is read when the content process is started. On Mac the content
// process is killed when all windows are closed, so a change will take effect
// when the 1st window is opened.
pref("security.sandbox.content.level", 3);
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
// Prefs for controlling whether and how the Mac NPAPI Flash plugin process is
// sandboxed. On Mac these levels are:
// 0 - "no sandbox"
// 1 - "global read access, limited write access for Flash functionality"
// 2 - "read access triggered by file dialog activity, limited read/write"
//     "access for Flash functionality"
// 3 - "limited read/write access for Flash functionality"
pref("dom.ipc.plugins.sandbox-level.flash", 1);
// Controls the level used on older OS X versions. Is overriden when the
// "dom.ipc.plugins.sandbox-level.flash" is set to 0.
pref("dom.ipc.plugins.sandbox-level.flash.legacy", 1);
// The max OS minor version where we use the above legacy sandbox level.
pref("dom.ipc.plugins.sandbox-level.flash.max-legacy-os-minor", 10);
// Controls the sandbox level used by plugins other than Flash. On Mac,
// no other plugins are supported and this pref is only used for test
// plugins used in automated tests.
pref("dom.ipc.plugins.sandbox-level.default", 1);
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX) && defined(MOZ_CONTENT_SANDBOX)
// This pref is introduced as part of bug 742434, the naming is inspired from
// its Windows/Mac counterpart, but on Linux it's an integer which means:
// 0 -> "no sandbox"
// 1 -> "content sandbox using seccomp-bpf when available" + ipc restrictions
// 2 -> "seccomp-bpf + write file broker"
// 3 -> "seccomp-bpf + read/write file brokering"
// 4 -> all of the above + network/socket restrictions + chroot
//
// The purpose of this setting is to allow Linux users or distros to disable
// the sandbox while we fix their problems, or to allow running Firefox with
// exotic configurations we can't reasonably support out of the box.
//
pref("security.sandbox.content.level", 4);
pref("security.sandbox.content.write_path_whitelist", "");
pref("security.sandbox.content.read_path_whitelist", "");
pref("security.sandbox.content.syscall_whitelist", "");
#endif

#if defined(XP_OPENBSD) && defined(MOZ_SANDBOX)
// default pledge strings for the main & content processes, cf bug 1457092
// broad list for now, has to be refined over time
pref("security.sandbox.pledge.main", "stdio rpath wpath cpath inet proc exec prot_exec flock ps sendfd recvfd dns vminfo tty drm unix fattr getpw mcast");
#if defined(MOZ_CONTENT_SANDBOX)
pref("security.sandbox.content.level", 1);
pref("security.sandbox.pledge.content", "stdio rpath wpath cpath inet recvfd sendfd prot_exec unix drm ps");
#endif
#endif

#if defined(MOZ_SANDBOX)
#if defined(MOZ_CONTENT_SANDBOX)
// ID (a UUID when set by gecko) that is used to form the name of a
// sandbox-writable temporary directory to be used by content processes
// when a temporary writable file is required in a level 1 sandbox.
pref("security.sandbox.content.tempDirSuffix", "");
#endif
pref("security.sandbox.plugin.tempDirSuffix", "");
#endif

#if defined(MOZ_SANDBOX)
// This pref determines if messages relevant to sandbox violations are
// logged.
#if defined(XP_WIN) || defined(XP_MACOSX)
pref("security.sandbox.logging.enabled", false);
#endif
#endif

// This pref governs whether we attempt to work around problems caused by
// plugins using OS calls to manipulate the cursor while running out-of-
// process.  These workarounds all involve intercepting (hooking) certain
// OS calls in the plugin process, then arranging to make certain OS calls
// in the browser process.  Eventually plugins will be required to use the
// NPAPI to manipulate the cursor, and these workarounds will be removed.
// See bug 621117.
#ifdef XP_MACOSX
pref("dom.ipc.plugins.nativeCursorSupport", true);
#endif

#ifdef XP_WIN
pref("browser.taskbar.previews.enable", false);
pref("browser.taskbar.previews.max", 20);
pref("browser.taskbar.previews.cachetime", 5);
pref("browser.taskbar.lists.enabled", true);
pref("browser.taskbar.lists.frequent.enabled", true);
pref("browser.taskbar.lists.recent.enabled", false);
pref("browser.taskbar.lists.maxListItemCount", 7);
pref("browser.taskbar.lists.tasks.enabled", true);
pref("browser.taskbar.lists.refreshInSeconds", 120);
#endif

// Preferences to be synced by default
pref("services.sync.prefs.sync.accessibility.blockautorefresh", true);
pref("services.sync.prefs.sync.accessibility.browsewithcaret", true);
pref("services.sync.prefs.sync.accessibility.typeaheadfind", true);
pref("services.sync.prefs.sync.accessibility.typeaheadfind.linksonly", true);
pref("services.sync.prefs.sync.addons.ignoreUserEnabledChanges", true);
// The addons prefs related to repository verification are intentionally
// not synced for security reasons. If a system is compromised, a user
// could weaken the pref locally, install an add-on from an untrusted
// source, and this would propagate automatically to other,
// uncompromised Sync-connected devices.
pref("services.sync.prefs.sync.browser.ctrlTab.recentlyUsedOrder", true);
pref("services.sync.prefs.sync.browser.download.useDownloadDir", true);
pref("services.sync.prefs.sync.browser.formfill.enable", true);
pref("services.sync.prefs.sync.browser.link.open_newwindow", true);
pref("services.sync.prefs.sync.browser.newtabpage.enabled", true);
pref("services.sync.prefs.sync.browser.newtabpage.pinned", true);
pref("services.sync.prefs.sync.browser.offline-apps.notify", true);
pref("services.sync.prefs.sync.browser.safebrowsing.phishing.enabled", true);
pref("services.sync.prefs.sync.browser.safebrowsing.malware.enabled", true);
pref("services.sync.prefs.sync.browser.safebrowsing.downloads.enabled", true);
pref("services.sync.prefs.sync.browser.safebrowsing.passwords.enabled", true);
pref("services.sync.prefs.sync.browser.search.update", true);
pref("services.sync.prefs.sync.browser.sessionstore.restore_on_demand", true);
pref("services.sync.prefs.sync.browser.startup.homepage", true);
pref("services.sync.prefs.sync.browser.startup.page", true);
pref("services.sync.prefs.sync.browser.tabs.loadInBackground", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnOpen", true);
pref("services.sync.prefs.sync.browser.urlbar.matchBuckets", true);
pref("services.sync.prefs.sync.browser.urlbar.maxRichResults", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.bookmark", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.history", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.openpage", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.searches", true);
pref("services.sync.prefs.sync.dom.disable_open_during_load", true);
pref("services.sync.prefs.sync.dom.disable_window_flip", true);
pref("services.sync.prefs.sync.dom.disable_window_move_resize", true);
pref("services.sync.prefs.sync.dom.event.contextmenu.enabled", true);
pref("services.sync.prefs.sync.extensions.personas.current", true);
pref("services.sync.prefs.sync.extensions.update.enabled", true);
pref("services.sync.prefs.sync.intl.accept_languages", true);
pref("services.sync.prefs.sync.layout.spellcheckDefault", true);
pref("services.sync.prefs.sync.lightweightThemes.selectedThemeID", true);
pref("services.sync.prefs.sync.lightweightThemes.usedThemes", true);
pref("services.sync.prefs.sync.network.cookie.cookieBehavior", true);
pref("services.sync.prefs.sync.network.cookie.lifetimePolicy", true);
pref("services.sync.prefs.sync.network.cookie.thirdparty.sessionOnly", true);
pref("services.sync.prefs.sync.permissions.default.image", true);
pref("services.sync.prefs.sync.pref.advanced.images.disable_button.view_image", true);
pref("services.sync.prefs.sync.pref.advanced.javascript.disable_button.advanced", true);
pref("services.sync.prefs.sync.pref.downloads.disable_button.edit_actions", true);
pref("services.sync.prefs.sync.pref.privacy.disable_button.cookie_exceptions", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.cache", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.cookies", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.downloads", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.formdata", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.history", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.offlineApps", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.sessions", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.siteSettings", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.enabled", true);
pref("services.sync.prefs.sync.privacy.fuzzyfox.enabled", false);
pref("services.sync.prefs.sync.privacy.fuzzyfox.clockgrainus", false);
pref("services.sync.prefs.sync.privacy.sanitize.sanitizeOnShutdown", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.enabled", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.pbmode.enabled", true);
pref("services.sync.prefs.sync.privacy.resistFingerprinting", true);
pref("services.sync.prefs.sync.privacy.reduceTimerPrecision", true);
pref("services.sync.prefs.sync.privacy.resistFingerprinting.reduceTimerPrecision.microseconds", true);
pref("services.sync.prefs.sync.privacy.resistFingerprinting.reduceTimerPrecision.jitter", true);
pref("services.sync.prefs.sync.security.OCSP.enabled", true);
pref("services.sync.prefs.sync.security.OCSP.require", true);
pref("services.sync.prefs.sync.security.default_personal_cert", true);
pref("services.sync.prefs.sync.security.tls.version.min", true);
pref("services.sync.prefs.sync.security.tls.version.max", true);
pref("services.sync.prefs.sync.services.sync.syncedTabs.showRemoteIcons", true);
pref("services.sync.prefs.sync.signon.rememberSignons", true);
pref("services.sync.prefs.sync.spellchecker.dictionary", true);
pref("services.sync.prefs.sync.xpinstall.whitelist.required", true);

// A preference that controls whether we should show the icon for a remote tab.
// This pref has no UI but exists because some people may be concerned that
// fetching these icons to show remote tabs may leak information about that
// user's tabs and bookmarks. Note this pref is also synced.
pref("services.sync.syncedTabs.showRemoteIcons", true);

// Developer edition preferences
#ifdef MOZ_DEV_EDITION
pref("lightweightThemes.selectedThemeID", "firefox-compact-dark@mozilla.org",
     sticky);
#else
pref("lightweightThemes.selectedThemeID", "default-theme@mozilla.org", sticky);
#endif

// Whether the character encoding menu is under the main Firefox button. This
// preference is a string so that localizers can alter it.
pref("browser.menu.showCharacterEncoding", "chrome://browser/locale/browser.properties");

// Allow using tab-modal prompts when possible.
pref("prompts.tab_modal.enabled", true);

// Activates preloading of the new tab url.
pref("browser.newtab.preload", true);

// Indicates if about:newtab shows content (enabled) or just blank
pref("browser.newtabpage.enabled", true);

// Activity Stream prefs that control to which page to redirect
pref("browser.newtabpage.activity-stream.prerender", true);
#ifndef RELEASE_OR_BETA
pref("browser.newtabpage.activity-stream.debug", false);
#endif

pref("browser.library.activity-stream.enabled", true);

// The remote FxA root content URL for the Activity Stream firstrun page.
pref("browser.newtabpage.activity-stream.fxaccounts.endpoint", "https://accounts.firefox.com/");

// The pref that controls if the search shortcuts experiment is on
#ifdef EARLY_BETA_OR_EARLIER
pref("browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts", true);
#else
pref("browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts", false);
#endif

// ASRouter provider configuration
#if defined(NIGHTLY_BUILD)
pref("browser.newtabpage.activity-stream.asrouter.providers.snippets", "{\"id\":\"snippets\",\"enabled\":true,\"type\":\"remote\",\"url\":\"https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/\",\"updateCycleInMs\":14400000}");
pref("browser.newtabpage.activity-stream.asrouter.providers.cfr", "{\"id\":\"cfr\",\"enabled\":true,\"type\":\"local\",\"localProvider\":\"CFRMessageProvider\",\"frequency\":{\"custom\":[{\"period\":\"daily\",\"cap\":1}]}}");
#else
pref("browser.newtabpage.activity-stream.asrouter.providers.snippets", "{\"id\":\"snippets\",\"enabled\":false,\"type\":\"remote\",\"url\":\"https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/\",\"updateCycleInMs\":14400000}");
pref("browser.newtabpage.activity-stream.asrouter.providers.cfr", "{\"id\":\"cfr\",\"enabled\":false,\"type\":\"local\",\"localProvider\":\"CFRMessageProvider\",\"frequency\":{\"custom\":[{\"period\":\"daily\",\"cap\":1}]}}");
#endif


// Enable the DOM fullscreen API.
pref("full-screen-api.enabled", true);

// Startup Crash Tracking
// number of startup crashes that can occur before starting into safe mode automatically
// (this pref has no effect if more than 6 hours have passed since the last crash)
pref("toolkit.startup.max_resumed_crashes", 3);

// Whether to use RegisterApplicationRestart to restart the browser and resume
// the session on next Windows startup
#if defined(XP_WIN)
pref("toolkit.winRegisterApplicationRestart", true);
#endif

// Whether we use pdfium to view content with the pdf mime type.
// Note: if the pref is set to false while Firefox is open, it won't
// take effect until there are no open pdfium tabs.
pref("pdfium.enabled", false);

// Completely disable pdf.js as an option to preview pdfs within firefox.
// Note: if this is not disabled it does not necessarily mean pdf.js is the pdf
// handler just that it is an option.
pref("pdfjs.disabled", false);
// Used by pdf.js to know the first time firefox is run with it installed so it
// can become the default pdf viewer.
pref("pdfjs.firstRun", true);
// The values of preferredAction and alwaysAskBeforeHandling before pdf.js
// became the default.
pref("pdfjs.previousHandler.preferredAction", 0);
pref("pdfjs.previousHandler.alwaysAskBeforeHandling", false);

// Is the sidebar positioned ahead of the content browser
pref("sidebar.position_start", true);

pref("security.identitypopup.recordEventElemetry", true);

// Block insecure active content on https pages
pref("security.mixed_content.block_active_content", true);

// Show degraded UI for http pages with password fields.
pref("security.insecure_password.ui.enabled", true);

// Show in-content login form warning UI for insecure login fields
pref("security.insecure_field_warning.contextual.enabled", true);

// Show degraded UI for http pages; disabled for now
pref("security.insecure_connection_icon.enabled", false);
// Show degraded UI for http pages in private mode only for Nightly: Bug 1434626
#if defined(NIGHTLY_BUILD)
pref("security.insecure_connection_icon.pbmode.enabled", true);
#else
pref("security.insecure_connection_icon.pbmode.enabled", false);
#endif

// Show "Not Secure" text for http pages; disabled for now
pref("security.insecure_connection_text.enabled", false);
pref("security.insecure_connection_text.pbmode.enabled", false);

// 1 = allow MITM for certificate pinning checks.
pref("security.cert_pinning.enforcement_level", 1);


// Override the Gecko-default value of false for Firefox.
pref("plain_text.wrap_long_lines", true);

// If this turns true, Moz*Gesture events are not called stopPropagation()
// before content.
pref("dom.debug.propagate_gesture_events_through_content", false);

// All the Geolocation preferences are here.
//

// Geolocation preferences for the RELEASE and "later" Beta channels.
// Some of these prefs are specified even though they are redundant; they are
// here for clarity and end-user experiments.
#ifndef EARLY_BETA_OR_EARLIER
pref("geo.wifi.uri", "https://www.googleapis.com/geolocation/v1/geolocate?key=%GOOGLE_API_KEY%");

#ifdef XP_MACOSX
pref("geo.provider.use_corelocation", false);
#endif

#ifdef XP_WIN
pref("geo.provider.ms-windows-location", false);
#endif

#ifdef MOZ_WIDGET_GTK
pref("geo.provider.use_gpsd", false);
#endif

#else

// Geolocation preferences for Nightly/Aurora/Beta.
pref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");

#ifdef XP_MACOSX
pref("geo.provider.use_corelocation", true);
#endif

// The native Windows location provider is only enabled in Nightly and likely to
// be unstable. Set to false if things are really broken.
#if defined(XP_WIN) && defined(NIGHTLY_BUILD)
pref("geo.provider.ms-windows-location", true);
#endif

#if defined(MOZ_WIDGET_GTK) && defined(MOZ_GPSD)
pref("geo.provider.use_gpsd", true);
#endif

#endif

// CustomizableUI debug logging.
pref("browser.uiCustomization.debug", false);

// CustomizableUI state of the browser's user interface
pref("browser.uiCustomization.state", "");

// If set to false, FxAccounts and Sync will be unavailable.
// A restart is mandatory after flipping that preference.
pref("identity.fxaccounts.enabled", true);

// The remote FxA root content URL. Must use HTTPS.
pref("identity.fxaccounts.remote.root", "https://accounts.firefox.com/");

// The value of the context query parameter passed in fxa requests.
pref("identity.fxaccounts.contextParam", "fx_desktop_v3");

// The remote URL of the FxA Profile Server
pref("identity.fxaccounts.remote.profile.uri", "https://profile.accounts.firefox.com/v1");

// The remote URL of the FxA OAuth Server
pref("identity.fxaccounts.remote.oauth.uri", "https://oauth.accounts.firefox.com/v1");

// Token server used by the FxA Sync identity.
pref("identity.sync.tokenserver.uri", "https://token.services.mozilla.com/1.0/sync/1.5");

// Auto-config URL for FxA self-hosters, makes an HTTP request to
// [identity.fxaccounts.autoconfig.uri]/.well-known/fxa-client-configuration
// This is now the prefered way of pointing to a custom FxA server, instead
// of making changes to "identity.fxaccounts.*.uri".
pref("identity.fxaccounts.autoconfig.uri", "");

// URLs for promo links to mobile browsers. Note that consumers are expected to
// append a value for utm_campaign.
pref("identity.mobilepromo.android", "https://www.mozilla.org/firefox/android/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=");
pref("identity.mobilepromo.ios", "https://www.mozilla.org/firefox/ios/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=");

// Migrate any existing Firefox Account data from the default profile to the
// Developer Edition profile.
#ifdef MOZ_DEV_EDITION
pref("identity.fxaccounts.migrateToDevEdition", true);
#else
pref("identity.fxaccounts.migrateToDevEdition", false);
#endif

// If activated, send tab will use the new FxA commands backend.
pref("identity.fxaccounts.commands.enabled", true);
// How often should we try to fetch missed FxA commands on sync (in seconds).
// Default is 24 hours.
pref("identity.fxaccounts.commands.missed.fetch_interval", 86400);

// On GTK, we now default to showing the menubar only when alt is pressed:
#ifdef MOZ_WIDGET_GTK
pref("ui.key.menuAccessKeyFocuses", true);
#endif

#ifdef NIGHTLY_BUILD
pref("media.eme.vp9-in-mp4.enabled", true);
#else
pref("media.eme.vp9-in-mp4.enabled", false);
#endif

pref("media.eme.hdcp-policy-check.enabled", false);

// Whether we should run a test-pattern through EME GMPs before assuming they'll
// decode H.264.
pref("media.gmp.trial-create.enabled", true);

// Note: when media.gmp-*.visible is true, provided we're running on a
// supported platform/OS version, the corresponding CDM appears in the
// plugins list, Firefox will download the GMP/CDM if enabled, and our
// UI to re-enable EME prompts the user to re-enable EME if it's disabled
// and script requests EME. If *.visible is false, we won't show the UI
// to enable the CDM if its disabled; it's as if the keysystem is completely
// unsupported.

#ifdef MOZ_WIDEVINE_EME
pref("media.gmp-widevinecdm.visible", true);
pref("media.gmp-widevinecdm.enabled", true);
#endif

#ifdef NIGHTLY_BUILD
// Switch block autoplay logic to v2, and enable UI.
pref("media.autoplay.enabled.user-gestures-needed", true);
// Allow asking for permission to autoplay to appear in UI.
pref("media.autoplay.ask-permission", true);
// Set Firefox to block autoplay, asking for permission by default.
pref("media.autoplay.default", 2); // 0=Allowed, 1=Blocked, 2=Prompt
#else
pref("media.autoplay.default", 0); // 0=Allowed, 1=Blocked, 2=Prompt
pref("media.autoplay.enabled.user-gestures-needed", false);
pref("media.autoplay.ask-permission", false);
#endif


// Play with different values of the decay time and get telemetry,
// 0 means to randomize (and persist) the experiment value in users' profiles,
// -1 means no experiment is run and we use the preferred value for frecency (6h)
pref("browser.cache.frecency_experiment", 0);

pref("browser.translation.detectLanguage", false);
pref("browser.translation.neverForLanguages", "");
// Show the translation UI bits, like the info bar, notification icon and preferences.
pref("browser.translation.ui.show", false);
// Allows to define the translation engine. Google is default, Bing or Yandex are other options.
pref("browser.translation.engine", "Google");

// Telemetry settings.
// Determines if Telemetry pings can be archived locally.
pref("toolkit.telemetry.archive.enabled", true);
// Enables sending the shutdown ping when Firefox shuts down.
pref("toolkit.telemetry.shutdownPingSender.enabled", true);
// Enables sending the shutdown ping using the pingsender from the first session.
pref("toolkit.telemetry.shutdownPingSender.enabledFirstSession", false);
// Enables sending a duplicate of the first shutdown ping from the first session.
pref("toolkit.telemetry.firstShutdownPing.enabled", true);
// Enables sending the 'new-profile' ping on new profiles.
pref("toolkit.telemetry.newProfilePing.enabled", true);
// Enables sending 'update' pings on Firefox updates.
pref("toolkit.telemetry.updatePing.enabled", true);
// Enables sending 'bhr' pings when the browser hangs.
pref("toolkit.telemetry.bhrPing.enabled", true);
// Enables using Hybrid Content Telemetry from Mozilla privileged pages.
pref("toolkit.telemetry.hybridContent.enabled", true);

// Ping Centre Telemetry settings.
pref("browser.ping-centre.telemetry", true);
pref("browser.ping-centre.log", false);
pref("browser.ping-centre.staging.endpoint", "https://onyx_tiles.stage.mozaws.net/v3/links/ping-centre");
pref("browser.ping-centre.production.endpoint", "https://tiles.services.mozilla.com/v3/links/ping-centre");

// Enable GMP support in the addon manager.
pref("media.gmp-provider.enabled", true);

// Enable blocking access to storage from tracking resources by default
pref("network.cookie.cookieBehavior", 4 /* BEHAVIOR_REJECT_TRACKER */);

pref("browser.contentblocking.allowlist.storage.enabled", true);

#ifdef NIGHTLY_BUILD
// Enable the Storage Access API in Nightly
pref("dom.storage_access.enabled", true);
#endif

// Define a set of default features for the Content Blocking UI.
pref("browser.contentblocking.trackingprotection.control-center.ui.enabled", true);
pref("browser.contentblocking.rejecttrackers.control-center.ui.enabled", true);

// Enable the Report Breakage UI on Nightly and Beta but not on Release yet.
#ifdef EARLY_BETA_OR_EARLIER
pref("browser.contentblocking.reportBreakage.enabled", true);
#else
pref("browser.contentblocking.reportBreakage.enabled", false);
#endif
// Show report breakage for tracking cookies in all channels.
pref("browser.contentblocking.rejecttrackers.reportBreakage.enabled", true);

pref("browser.contentblocking.reportBreakage.url", "https://tracking-protection-issues.herokuapp.com/new");

pref("browser.contentblocking.introCount", 0);

pref("privacy.trackingprotection.introURL", "https://www.mozilla.org/%LOCALE%/firefox/%VERSION%/tracking-protection/start/");

// Workaround for Google Recaptcha
pref("urlclassifier.trackingAnnotationSkipURLs", "google.com/recaptcha/,*.google.com/recaptcha/");

// Always enable newtab segregation using containers
pref("privacy.usercontext.about_newtab_segregation.enabled", true);
// Enable Contextual Identity Containers
#ifdef NIGHTLY_BUILD
pref("privacy.userContext.enabled", true);
pref("privacy.userContext.ui.enabled", true);

// 0 disables long press, 1 when clicked, the menu is shown, 2 the menu is shown after X milliseconds.
pref("privacy.userContext.longPressBehavior", 2);
#else
pref("privacy.userContext.enabled", false);
pref("privacy.userContext.ui.enabled", false);

// 0 disables long press, 1 when clicked, the menu is shown, 2 the menu is shown after X milliseconds.
pref("privacy.userContext.longPressBehavior", 0);
#endif
pref("privacy.userContext.extension", "");

// Start the browser in e10s mode
pref("browser.tabs.remote.autostart", true);
pref("browser.tabs.remote.desktopbehavior", true);

// For speculatively warming up tabs to improve perceived
// performance while using the async tab switcher.
pref("browser.tabs.remote.warmup.enabled", true);

// Caches tab layers to improve perceived performance
// of tab switches.
pref("browser.tabs.remote.tabCacheSize", 0);

pref("browser.tabs.remote.warmup.maxTabs", 3);
pref("browser.tabs.remote.warmup.unloadDelayMs", 2000);

// For the about:tabcrashed page
pref("browser.tabs.crashReporting.sendReport", true);
pref("browser.tabs.crashReporting.includeURL", false);
pref("browser.tabs.crashReporting.requestEmail", false);
pref("browser.tabs.crashReporting.emailMe", false);
pref("browser.tabs.crashReporting.email", "");

pref("extensions.legacy.enabled", false);

// How often to check for CPOW timeouts. CPOWs are only timed out by
// the hang monitor.
pref("dom.ipc.cpow.timeout", 500);

// Causes access on unsafe CPOWs from browser code to throw by default.
pref("dom.ipc.cpows.forbid-unsafe-from-browser", true);

// Enable e10s hang monitoring (slow script checking and plugin hang
// detection).
pref("dom.ipc.processHangMonitor", true);

#ifdef DEBUG
// Don't report hangs in DEBUG builds. They're too slow and often a
// debugger is attached.
pref("dom.ipc.reportProcessHangs", false);
#else
pref("dom.ipc.reportProcessHangs", true);
#endif

// Don't limit how many nodes we care about on desktop:
pref("reader.parse-node-limit", 0);

// On desktop, we want the URLs to be included here for ease of debugging,
// and because (normally) these errors are not persisted anywhere.
pref("reader.errors.includeURLs", true);

pref("dom.serviceWorkers.enabled", true);

// Enable Push API.
pref("dom.push.enabled", true);

// These are the thumbnail width/height set in about:newtab.
// If you change this, ENSURE IT IS THE SAME SIZE SET
// by about:newtab. These values are in CSS pixels.
pref("toolkit.pageThumbs.minWidth", 280);
pref("toolkit.pageThumbs.minHeight", 190);

// Enable speech synthesis
pref("media.webspeech.synth.enabled", true);

pref("browser.esedbreader.loglevel", "Error");

pref("browser.laterrun.enabled", false);

pref("dom.ipc.processPrelaunch.enabled", true);

// See comments in bug 1340115 on how we got to these numbers.
pref("browser.migrate.chrome.history.limit", 2000);
pref("browser.migrate.chrome.history.maxAgeInDays", 180);

// Enable browser frames for use on desktop.  Only exposed to chrome callers.
pref("dom.mozBrowserFramesEnabled", true);

pref("extensions.pocket.enabled", true);
pref("extensions.pocket.oAuthConsumerKey", "40249-e88c401e1b1f2242d9e441c4");

pref("signon.schemeUpgrades", true);

// Enable the "Simplify Page" feature in Print Preview. This feature
// is disabled by default in toolkit.
pref("print.use_simplify_page", true);

// Space separated list of URLS that are allowed to send objects (instead of
// only strings) through webchannels. This list is duplicated in mobile/android/app/mobile.js
pref("webchannel.allowObject.urlWhitelist", "https://content.cdn.mozilla.net https://input.mozilla.org https://support.mozilla.org https://install.mozilla.org");

// Whether or not the browser should scan for unsubmitted
// crash reports, and then show a notification for submitting
// those reports.
#ifdef NIGHTLY_BUILD
pref("browser.crashReports.unsubmittedCheck.enabled", true);
#else
pref("browser.crashReports.unsubmittedCheck.enabled", false);
#endif

// chancesUntilSuppress is how many times we'll show the unsubmitted
// crash report notification across different days and shutdown
// without a user choice before we suppress the notification for
// some number of days.
pref("browser.crashReports.unsubmittedCheck.chancesUntilSuppress", 4);
pref("browser.crashReports.unsubmittedCheck.autoSubmit2", false);

// Preferences for the form autofill system extension
// The truthy values of "extensions.formautofill.available" are "on" and "detect",
// any other value means autofill isn't available.
// "detect" means it's enabled if conditions defined in the extension are met.
#ifdef NIGHTLY_BUILD
pref("extensions.formautofill.available", "on");
pref("extensions.formautofill.creditCards.available", true);
#else
pref("extensions.formautofill.available", "detect");
pref("extensions.formautofill.creditCards.available", false);
#endif
pref("extensions.formautofill.addresses.enabled", true);
pref("extensions.formautofill.creditCards.enabled", true);
// Pref for shield/heartbeat to recognize users who have used Credit Card
// Autofill. The valid values can be:
// 0: none
// 1: submitted a manually-filled credit card form (but didn't see the doorhanger
//    because of a duplicate profile in the storage)
// 2: saw the doorhanger
// 3: submitted an autofill'ed credit card form
pref("extensions.formautofill.creditCards.used", 0);
pref("extensions.formautofill.firstTimeUse", true);
pref("extensions.formautofill.heuristics.enabled", true);
pref("extensions.formautofill.section.enabled", true);
pref("extensions.formautofill.loglevel", "Warn");

#ifdef NIGHTLY_BUILD
// Comma separated list of countries Form Autofill is available in.
pref("extensions.formautofill.supportedCountries", "US,CA,DE");
pref("extensions.formautofill.supportRTL", true);
#else
pref("extensions.formautofill.supportedCountries", "US");
pref("extensions.formautofill.supportRTL", false);
#endif

// Whether or not to restore a session with lazy-browser tabs.
pref("browser.sessionstore.restore_tabs_lazily", true);

pref("browser.suppress_first_window_animation", true);

// Preference that allows individual users to disable Screenshots.
pref("extensions.screenshots.disabled", false);
// Preference that allows individual users to leave Screenshots enabled, but
// disable uploading to the server.
pref("extensions.screenshots.upload-disabled", false);

// Preferences for BrowserErrorReporter.jsm
// Only collect errors on Nightly, and specifically not local builds
#if defined(NIGHTLY_BUILD) && MOZ_UPDATE_CHANNEL != default
pref("browser.chrome.errorReporter.enabled", true);
#else
pref("browser.chrome.errorReporter.enabled", false);
#endif
pref("browser.chrome.errorReporter.sampleRate", "0.001");
pref("browser.chrome.errorReporter.publicKey", "c709cb7a2c0b4f0882fcc84a5af161ec");
pref("browser.chrome.errorReporter.projectId", "339");
pref("browser.chrome.errorReporter.submitUrl", "https://sentry.prod.mozaws.net/api/339/store/");
pref("browser.chrome.errorReporter.logLevel", "Error");

// URL for Learn More link for browser error logging in preferences
pref("browser.chrome.errorReporter.infoURL",
     "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/nightly-error-collection");

// Normandy client preferences
pref("app.normandy.api_url", "https://normandy.cdn.mozilla.net/api/v1");
pref("app.normandy.dev_mode", false);
pref("app.normandy.enabled", true);
pref("app.normandy.first_run", true);
pref("app.normandy.logging.level", 50); // Warn
pref("app.normandy.run_interval_seconds", 21600); // 6 hours
pref("app.normandy.shieldLearnMoreUrl", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/shield");
#ifdef MOZ_DATA_REPORTING
pref("app.shield.optoutstudies.enabled", true);
#else
pref("app.shield.optoutstudies.enabled", false);
#endif

// Multi-lingual preferences
pref("intl.multilingual.enabled", false);
// AMO only serves language packs for release and beta versions.
#ifdef RELEASE_OR_BETA
pref("intl.multilingual.downloadEnabled", true);
#else
pref("intl.multilingual.downloadEnabled", false);
#endif

// Simulate conditions that will happen when the browser
// is running with Fission enabled. This is meant to assist
// development and testing of Fission.
// The current simulated conditions are:
// - Don't propagate events from subframes to JS child actors
pref("browser.fission.simulate", false);

// Prio preferences
// Only enable by default on Nightly.
// On platforms that do not build libprio, do not set these prefs at all, which gives us a way to detect support.

// Curve25519 public keys for Prio servers
#ifdef MOZ_LIBPRIO
pref("prio.publicKeyA", "35AC1C7576C7C6EDD7FED6BCFC337B34D48CB4EE45C86BEEFB40BD8875707733");
pref("prio.publicKeyB", "26E6674E65425B823F1F1D5F96E3BB3EF9E406EC7FBA7DEF8B08A35DD135AF50");
#endif

// Coverage ping is disabled by default.
pref("toolkit.coverage.enabled", false);
pref("toolkit.coverage.endpoint.base", "https://coverage.mozilla.org");
// Whether or not Prio-encoded Telemetry will be sent along with the main ping.
#if defined(NIGHTLY_BUILD) && defined(MOZ_LIBPRIO)
pref("prio.enabled", true);
#endif

// Discovery prefs
pref("browser.discovery.enabled", false);
pref("browser.discovery.containers.enabled", true);
pref("browser.discovery.sites", "addons.mozilla.org");
