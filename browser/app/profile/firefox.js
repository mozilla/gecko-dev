# -*- Mode: JavaScript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

pref("browser.chromeURL","chrome://browser/content/");
pref("browser.hiddenWindowChromeURL", "chrome://browser/content/hiddenWindow.xul");

// Enables some extra Extension System Logging (can reduce performance)
pref("extensions.logging.enabled", false);

// Disables strict compatibility, making addons compatible-by-default.
pref("extensions.strictCompatibility", false);

// Specifies a minimum maxVersion an addon needs to say it's compatible with
// for it to be compatible by default.
pref("extensions.minCompatibleAppVersion", "4.0");

// Preferences for AMO integration
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.maxResults", 15);
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/%LOCALE%/firefox/api/%API_VERSION%/search/guid:%IDS%?src=firefox&appOS=%OS%&appVersion=%VERSION%");
pref("extensions.getAddons.getWithPerformance.url", "https://services.addons.mozilla.org/%LOCALE%/firefox/api/%API_VERSION%/search/guid:%IDS%?src=firefox&appOS=%OS%&appVersion=%VERSION%&tMain=%TIME_MAIN%&tFirstPaint=%TIME_FIRST_PAINT%&tSessionRestored=%TIME_SESSION_RESTORED%");
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/firefox/search?q=%TERMS%&platform=%OS%&appver=%VERSION%");
pref("extensions.getAddons.search.url", "https://services.addons.mozilla.org/%LOCALE%/firefox/api/%API_VERSION%/search/%TERMS%/all/%MAX_RESULTS%/%OS%/%VERSION%/%COMPATIBILITY_MODE%?src=firefox");
pref("extensions.webservice.discoverURL", "https://services.addons.mozilla.org/%LOCALE%/firefox/discovery/pane/%VERSION%/%OS%/%COMPATIBILITY_MODE%");

// Blocklist preferences
pref("extensions.blocklist.enabled", true);
pref("extensions.blocklist.interval", 86400);
// Controls what level the blocklist switches from warning about items to forcibly
// blocking them.
pref("extensions.blocklist.level", 2);
pref("extensions.blocklist.url", "https://addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.com/%LOCALE%/blocklist/");
pref("extensions.blocklist.itemURL", "https://addons.mozilla.org/%LOCALE%/%APP%/blocked/%blockID%");

pref("extensions.update.autoUpdateDefault", true);

pref("extensions.hotfix.id", "firefox-hotfix@mozilla.org");
pref("extensions.hotfix.cert.checkAttributes", true);
pref("extensions.hotfix.certs.1.sha1Fingerprint", "CA:C4:7D:BF:63:4D:24:E9:DC:93:07:2F:E3:C8:EA:6D:C3:94:6E:89");

// Disable add-ons that are not installed by the user in all scopes by default.
// See the SCOPE constants in AddonManager.jsm for values to use here.
pref("extensions.autoDisableScopes", 15);

// Dictionary download preference
pref("browser.dictionaries.download.url", "https://addons.mozilla.org/%LOCALE%/firefox/dictionaries/");

// The minimum delay in seconds for the timer to fire.
// default=2 minutes
pref("app.update.timerMinimumDelay", 120);

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

// When |app.update.cert.requireBuiltIn| is true or not specified the
// final certificate and all certificates the connection is redirected to before
// the final certificate for the url specified in the |app.update.url|
// preference must be built-in.
pref("app.update.cert.requireBuiltIn", true);

// When |app.update.cert.checkAttributes| is true or not specified the
// certificate attributes specified in the |app.update.certs.| preference branch
// are checked against the certificate for the url specified by the
// |app.update.url| preference.
pref("app.update.cert.checkAttributes", true);

// The number of certificate attribute check failures to allow for background
// update checks before notifying the user of the failure. User initiated update
// checks always notify the user of the certificate attribute check failure.
pref("app.update.cert.maxErrors", 5);

// The |app.update.certs.| preference branch contains branches that are
// sequentially numbered starting at 1 that contain attribute name / value
// pairs for the certificate used by the server that hosts the update xml file
// as specified in the |app.update.url| preference. When these preferences are
// present the following conditions apply for a successful update check:
// 1. the uri scheme must be https
// 2. the preference name must exist as an attribute name on the certificate and
//    the value for the name must be the same as the value for the attribute name
//    on the certificate.
// If these conditions aren't met it will be treated the same as when there is
// no update available. This validation will not be performed when the
// |app.update.url.override| user preference has been set for testing updates or
// when the |app.update.cert.checkAttributes| preference is set to false. Also,
// the |app.update.url.override| preference should ONLY be used for testing.
pref("app.update.certs.1.issuerName", "OU=Equifax Secure Certificate Authority,O=Equifax,C=US");
pref("app.update.certs.1.commonName", "aus3.mozilla.org");

pref("app.update.certs.2.issuerName", "CN=Thawte SSL CA,O=\"Thawte, Inc.\",C=US");
pref("app.update.certs.2.commonName", "aus3.mozilla.org");

// Whether or not app updates are enabled
pref("app.update.enabled", true);

// This preference turns on app.update.mode and allows automatic download and
// install to take place. We use a separate boolean toggle for this to make
// the UI easier to construct.
pref("app.update.auto", true);

// Defines how the Application Update Service notifies the user about updates:
//
// AUM Set to:        Minor Releases:     Major Releases:
// 0                  download no prompt  download no prompt
// 1                  download no prompt  download no prompt if no incompatibilities
// 2                  download no prompt  prompt
//
// See chart in nsUpdateService.js source for more details
//
pref("app.update.mode", 1);

// If set to true, the Update Service will present no UI for any event.
pref("app.update.silent", false);

// If set to true, the Update Service will apply updates in the background
// when it finishes downloading them.
pref("app.update.staging.enabled", true);

// Update service URL:
pref("app.update.url", "https://aus3.mozilla.org/update/3/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");
// app.update.url.manual is in branding section
// app.update.url.details is in branding section

// User-settable override to app.update.url for testing purposes.
//pref("app.update.url.override", "");

// app.update.interval is in branding section
// app.update.promptWaitTime is in branding section

// Show the Update Checking/Ready UI when the user was idle for x seconds
pref("app.update.idletime", 60);

// Whether or not we show a dialog box informing the user that the update was
// successfully applied. This is off in Firefox by default since we show a 
// upgrade start page instead! Other apps may wish to show this UI, and supply
// a whatsNewURL field in their brand.properties that contains a link to a page
// which tells users what's new in this new update.
pref("app.update.showInstalledUI", false);

// 0 = suppress prompting for incompatibilities if there are updates available
//     to newer versions of installed addons that resolve them.
// 1 = suppress prompting for incompatibilities only if there are VersionInfo
//     updates available to installed addons that resolve them, not newer
//     versions.
pref("app.update.incompatible.mode", 0);

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
// Non-symmetric (not shared by extensions) extension-specific [update] preferences
pref("extensions.dss.enabled", false);          // Dynamic Skin Switching                                               
pref("extensions.dss.switchPending", false);    // Non-dynamic switch pending after next
                                                // restart.

pref("extensions.{972ce4c6-7e08-4474-a285-3208198ce6fd}.name", "chrome://browser/locale/browser.properties");
pref("extensions.{972ce4c6-7e08-4474-a285-3208198ce6fd}.description", "chrome://browser/locale/browser.properties");

pref("xpinstall.whitelist.add", "addons.mozilla.org");
pref("xpinstall.whitelist.add.36", "getpersonas.com");
pref("xpinstall.whitelist.add.180", "marketplace.firefox.com");

pref("lightweightThemes.update.enabled", true);

pref("keyword.enabled", true);
// Override the default keyword.URL. Empty value means
// "use the search service's default engine"
pref("keyword.URL", "");

pref("general.useragent.locale", "@AB_CD@");
pref("general.skins.selectedSkin", "classic/1.0");

pref("general.smoothScroll", true);
#ifdef UNIX_BUT_NOT_MAC
pref("general.autoScroll", false);
#else
pref("general.autoScroll", true);
#endif

pref("general.useragent.complexOverride.moodle", false); // bug 797703

// At startup, check if we're the default browser and prompt user if not.
pref("browser.shell.checkDefaultBrowser", true);

// 0 = blank, 1 = home (browser.startup.homepage), 2 = last visited page, 3 = resume previous browser session
// The behavior of option 3 is detailed at: http://wiki.mozilla.org/Session_Restore
pref("browser.startup.page",                1);
pref("browser.startup.homepage",            "chrome://branding/locale/browserconfig.properties");

pref("browser.slowStartup.notificationDisabled", false);
pref("browser.slowStartup.timeThreshold", 60000);
pref("browser.slowStartup.maxSamples", 5);

// This url, if changed, MUST continue to point to an https url. Pulling arbitrary content to inject into
// this page over http opens us up to a man-in-the-middle attack that we'd rather not face. If you are a downstream
// repackager of this code using an alternate snippet url, please keep your users safe
pref("browser.aboutHomeSnippets.updateUrl", "https://snippets.mozilla.com/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/");

pref("browser.enable_automatic_image_resizing", true);
pref("browser.chrome.site_icons", true);
pref("browser.chrome.favicons", true);
// browser.warnOnQuit == false will override all other possible prompts when quitting or restarting
pref("browser.warnOnQuit", true);
// browser.showQuitWarning specifically controls the quit warning dialog. We
// might still show the window closing dialog with showQuitWarning == false.
pref("browser.showQuitWarning", false);
pref("browser.fullscreen.autohide", true);
pref("browser.fullscreen.animateUp", 1);
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
pref("browser.urlbar.autoFill", true);
pref("browser.urlbar.autoFill.typed", true);
// 0: Match anywhere (e.g., middle of words)
// 1: Match on word boundaries and then try matching anywhere
// 2: Match only on word boundaries (e.g., after / or .)
// 3: Match at the beginning of the url or title
pref("browser.urlbar.matchBehavior", 1);
pref("browser.urlbar.filter.javascript", true);

// the maximum number of results to show in autocomplete when doing richResults
pref("browser.urlbar.maxRichResults", 12);
// The amount of time (ms) to wait after the user has stopped typing
// before starting to perform autocomplete.  50 is the default set in
// autocomplete.xml.
pref("browser.urlbar.delay", 50);

// The special characters below can be typed into the urlbar to either restrict
// the search to visited history, bookmarked, tagged pages; or force a match on
// just the title text or url.
pref("browser.urlbar.restrict.history", "^");
pref("browser.urlbar.restrict.bookmark", "*");
pref("browser.urlbar.restrict.tag", "+");
pref("browser.urlbar.restrict.openpage", "%");
pref("browser.urlbar.restrict.typed", "~");
pref("browser.urlbar.match.title", "#");
pref("browser.urlbar.match.url", "@");

// The default behavior for the urlbar can be configured to use any combination
// of the restrict or match filters with each additional filter restricting
// more (intersection). Add the following values to set the behavior as the
// default: 1: history, 2: bookmark, 4: tag, 8: title, 16: url, 32: typed,
//          64: javascript, 128: tabs
// E.g., 0 = show all results (no filtering), 1 = only visited pages in history,
// 2 = only bookmarks, 3 = visited bookmarks, 1+16 = history matching in the url
pref("browser.urlbar.default.behavior", 0);

pref("browser.urlbar.formatting.enabled", true);
pref("browser.urlbar.trimURLs", true);

pref("browser.altClickSave", false);

// Enable logging downloads operations to the Error Console.
pref("browser.download.debug", false);

// Number of milliseconds to wait for the http headers (and thus
// the Content-Disposition filename) before giving up and falling back to 
// picking a filename without that info in hand so that the user sees some
// feedback from their action.
pref("browser.download.saveLinkAsFilenameTimeout", 4000);

pref("browser.download.useDownloadDir", true);

pref("browser.download.folderList", 1);
pref("browser.download.manager.showAlertOnComplete", true);
pref("browser.download.manager.showAlertInterval", 2000);
pref("browser.download.manager.retention", 2);
pref("browser.download.manager.showWhenStarting", true);
pref("browser.download.manager.closeWhenDone", false);
pref("browser.download.manager.focusWhenStarting", false);
pref("browser.download.manager.flashCount", 2);
pref("browser.download.manager.addToRecentDocs", true);
pref("browser.download.manager.quitBehavior", 0);
pref("browser.download.manager.scanWhenDone", true);
pref("browser.download.manager.resumeOnWakeDelay", 10000);

// This allows disabling the Downloads Panel in favor of the old interface.
pref("browser.download.useToolkitUI", false);

// This records whether or not the panel has been shown at least once.
pref("browser.download.panel.shown", false);

// This records whether or not at least one session with the Downloads Panel
// enabled has been completed already.
pref("browser.download.panel.firstSessionCompleted", false);

// search engines URL
pref("browser.search.searchEnginesURL",      "https://addons.mozilla.org/%LOCALE%/firefox/search-engines/");

// pointer to the default engine name
pref("browser.search.defaultenginename",      "chrome://browser-region/locale/region.properties");

// disable logging for the search service by default
pref("browser.search.log", false);

// Ordering of Search Engines in the Engine list. 
pref("browser.search.order.1",                "chrome://browser-region/locale/region.properties");
pref("browser.search.order.2",                "chrome://browser-region/locale/region.properties");
pref("browser.search.order.3",                "chrome://browser-region/locale/region.properties");

// search bar results always open in a new tab
pref("browser.search.openintab", false);

// context menu searches open in the foreground
pref("browser.search.context.loadInBackground", false);

// send ping to the server to update
pref("browser.search.update", true);

// disable logging for the search service update system by default
pref("browser.search.update.log", false);

// Check whether we need to perform engine updates every 6 hours
pref("browser.search.update.interval", 21600);

// enable search suggestions by default
pref("browser.search.suggest.enabled", true);

#ifdef MOZ_OFFICIAL_BRANDING
// {moz:official} expands to "official"
pref("browser.search.official", true);
#endif

pref("browser.sessionhistory.max_entries", 50);

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

// Disable opening a new window via window.open if browser is in fullscreen mode
#ifdef XP_MACOSX
pref("browser.link.open_newwindow.disabled_in_fullscreen", true);
#else
pref("browser.link.open_newwindow.disabled_in_fullscreen", false);
#endif

// Tabbed browser
pref("browser.tabs.autoHide", false);
pref("browser.tabs.closeWindowWithLastTab", true);
pref("browser.tabs.insertRelatedAfterCurrent", true);
pref("browser.tabs.warnOnClose", true);
pref("browser.tabs.warnOnCloseOtherTabs", true);
pref("browser.tabs.warnOnOpen", true);
pref("browser.tabs.maxOpenBeforeWarn", 15);
pref("browser.tabs.loadInBackground", true);
pref("browser.tabs.opentabfor.middleclick", true);
pref("browser.tabs.loadDivertedInBackground", false);
pref("browser.tabs.loadBookmarksInBackground", false);
pref("browser.tabs.tabClipWidth", 140);
pref("browser.tabs.animate", true);
pref("browser.tabs.onTop", true);
#ifdef XP_WIN
pref("browser.tabs.drawInTitlebar", true);
#else
pref("browser.tabs.drawInTitlebar", false);
#endif

// Where to show tab close buttons:
// 0  on active tab only
// 1  on all tabs until tabClipWidth is reached, then active tab only
// 2  no close buttons at all
// 3  at the end of the tabstrip
pref("browser.tabs.closeButtons", 1);

// When tabs opened by links in other tabs via a combination of 
// browser.link.open_newwindow being set to 3 and target="_blank" etc are
// closed:
// true   return to the tab that opened this tab (its owner)
// false  return to the adjacent tab (old default)
pref("browser.tabs.selectOwnerOnClose", true);

pref("browser.ctrlTab.previews", false);
pref("browser.ctrlTab.recentlyUsedLimit", 7);

// By default, do not export HTML at shutdown.
// If true, at shutdown the bookmarks in your menu and toolbar will
// be exported as HTML to the bookmarks.html file.
pref("browser.bookmarks.autoExportHTML",          false);

// The maximum number of daily bookmark backups to 
// keep in {PROFILEDIR}/bookmarkbackups. Special values:
// -1: unlimited
//  0: no backups created (and deletes all existing backups)
pref("browser.bookmarks.max_backups",             10);

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
// prevent JS from setting status messages
pref("dom.disable_window_status_change",          true);
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
pref("privacy.clearOnShutdown.passwords",   false);
pref("privacy.clearOnShutdown.downloads",   true);
pref("privacy.clearOnShutdown.cookies",     true);
pref("privacy.clearOnShutdown.cache",       true);
pref("privacy.clearOnShutdown.sessions",    true);
pref("privacy.clearOnShutdown.offlineApps", false);
pref("privacy.clearOnShutdown.siteSettings", false);

pref("privacy.cpd.history",                 true);
pref("privacy.cpd.formdata",                true);
pref("privacy.cpd.passwords",               false);
pref("privacy.cpd.downloads",               true);
pref("privacy.cpd.cookies",                 true);
pref("privacy.cpd.cache",                   true);
pref("privacy.cpd.sessions",                true);
pref("privacy.cpd.offlineApps",             false);
pref("privacy.cpd.siteSettings",            false);

// What default should we use for the time span in the sanitizer:
// 0 - Clear everything
// 1 - Last Hour
// 2 - Last 2 Hours
// 3 - Last 4 Hours
// 4 - Today
pref("privacy.sanitize.timeSpan", 1);
pref("privacy.sanitize.sanitizeOnShutdown", false);

pref("privacy.sanitize.migrateFx3Prefs",    false);

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
#ifdef XP_WIN
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

// 0: Nothing happens
// 1: Scrolling contents
// 2: Go back or go forward, in your history
// 3: Zoom in or out.
#ifdef XP_MACOSX
// On OS X, if the wheel has one axis only, shift+wheel comes through as a
// horizontal scroll event. Thus, we can't assign anything other than normal
// scrolling to shift+wheel.
pref("mousewheel.with_alt.action", 2);
pref("mousewheel.with_shift.action", 1);
// On MacOS X, control+wheel is typically handled by system and we don't
// receive the event.  So, command key which is the main modifier key for
// acceleration is the best modifier for zoom-in/out.  However, we should keep
// the control key setting for backward compatibility.
pref("mousewheel.with_meta.action", 3); // command key on Mac
// Disable control-/meta-modified horizontal mousewheel events, since
// those are used on Mac as part of modified swipe gestures (e.g.
// Left swipe+Cmd = go back in a new tab).
pref("mousewheel.with_control.action.override_x", 0);
pref("mousewheel.with_meta.action.override_x", 0);
#else
pref("mousewheel.with_alt.action", 1);
pref("mousewheel.with_shift.action", 2);
pref("mousewheel.with_meta.action", 1); // win key on Win, Super/Hyper on Linux
#endif
pref("mousewheel.with_control.action",3);
pref("mousewheel.with_win.action", 1);

pref("browser.xul.error_pages.enabled", true);
pref("browser.xul.error_pages.expert_bad_cert", false);

// Work Offline is best manually managed by the user.
pref("network.manage-offline-status", false);

// We want to make sure mail URLs are handled externally...
pref("network.protocol-handler.external.mailto", true); // for mail
pref("network.protocol-handler.external.news", true);   // for news
pref("network.protocol-handler.external.snews", true);  // for secure news
pref("network.protocol-handler.external.nntp", true);   // also news
// ...without warning dialogs
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.news", false);
pref("network.protocol-handler.warn-external.snews", false);
pref("network.protocol-handler.warn-external.nntp", false);

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

// plugin finder service url
pref("pfs.datasource.url", "https://pfs.mozilla.org/plugins/PluginFinderService.php?mimetype=%PLUGIN_MIMETYPE%&appID=%APP_ID%&appVersion=%APP_VERSION%&clientOS=%CLIENT_OS%&chromeLocale=%CHROME_LOCALE%&appRelease=%APP_RELEASE%");

// by default we show an infobar message when pages require plugins the user has not installed, or are outdated
pref("plugins.hide_infobar_for_missing_plugin", false);
pref("plugins.hide_infobar_for_outdated_plugin", false);

pref("plugins.update.url", "https://www.mozilla.org/%LOCALE%/plugincheck/");
pref("plugins.update.notifyUser", false);

pref("plugins.click_to_play", false);

#ifdef XP_WIN
pref("browser.preferences.instantApply", false);
#else
pref("browser.preferences.instantApply", true);
#endif
#ifdef XP_MACOSX
pref("browser.preferences.animateFadeIn", true);
#else
pref("browser.preferences.animateFadeIn", false);
#endif

// Toggles between the two Preferences implementations, pop-up window and in-content
pref("browser.preferences.inContent", false);

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

/* initial web feed readers list */
pref("browser.contentHandlers.types.0.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.0.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.0.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.1.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.1.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.1.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.2.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.2.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.2.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.3.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.3.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.3.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.4.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.4.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.4.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.5.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.5.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.5.type", "application/vnd.mozilla.maybe.feed");

pref("browser.feeds.handler", "ask");
pref("browser.videoFeeds.handler", "ask");
pref("browser.audioFeeds.handler", "ask");

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

// By default, we don't want protocol/content handlers to be registered from a different host, see bug 402287
pref("gecko.handlerService.allowRegisterFromDifferentHost", false);

#ifdef MOZ_SAFE_BROWSING
pref("browser.safebrowsing.enabled", true);
pref("browser.safebrowsing.malware.enabled", true);
pref("browser.safebrowsing.debug", false);

pref("browser.safebrowsing.updateURL", "http://safebrowsing.clients.google.com/safebrowsing/downloads?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.keyURL", "https://sb-ssl.google.com/safebrowsing/newkey?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.gethashURL", "http://safebrowsing.clients.google.com/safebrowsing/gethash?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/report?");
pref("browser.safebrowsing.reportGenericURL", "http://%LOCALE%.phish-generic.mozilla.com/?hl=%LOCALE%");
pref("browser.safebrowsing.reportErrorURL", "http://%LOCALE%.phish-error.mozilla.com/?hl=%LOCALE%");
pref("browser.safebrowsing.reportPhishURL", "http://%LOCALE%.phish-report.mozilla.com/?hl=%LOCALE%");
pref("browser.safebrowsing.reportMalwareURL", "http://%LOCALE%.malware-report.mozilla.com/?hl=%LOCALE%");
pref("browser.safebrowsing.reportMalwareErrorURL", "http://%LOCALE%.malware-error.mozilla.com/?hl=%LOCALE%");

pref("browser.safebrowsing.warning.infoURL", "http://www.mozilla.com/%LOCALE%/firefox/phishing-protection/");
pref("browser.safebrowsing.malware.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");

#ifdef MOZILLA_OFFICIAL
// Normally the "client ID" sent in updates is appinfo.name, but for
// official Firefox releases from Mozilla we use a special identifier.
pref("browser.safebrowsing.id", "navclient-auto-ffox");
#endif

// Name of the about: page contributed by safebrowsing to handle display of error
// pages on phishing/malware hits.  (bug 399233)
pref("urlclassifier.alternate_error_page", "blocked");

// The number of random entries to send with a gethash request.
pref("urlclassifier.gethashnoise", 4);

// The list of tables that use the gethash request to confirm partial results.
pref("urlclassifier.gethashtables", "goog-phish-shavar,goog-malware-shavar");

// If an urlclassifier table has not been updated in this number of seconds,
// a gethash request will be forced to check that the result is still in
// the database.
pref("urlclassifier.max-complete-age", 2700);
#endif

pref("browser.geolocation.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/geolocation/");
pref("browser.mixedcontent.warning.infoURL", "http://support.mozilla.org/1/%APP%/%VERSION%/%OS%/%LOCALE%/mixed-content/");

pref("browser.EULA.version", 3);
pref("browser.rights.version", 3);
pref("browser.rights.3.shown", false);

#ifdef DEBUG
// Don't show the about:rights notification in debug builds.
pref("browser.rights.override", true);
#endif

pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.resume_session_once", false);

// minimal interval between two save operations in milliseconds
pref("browser.sessionstore.interval", 15000);
// maximum amount of POSTDATA to be saved in bytes per history entry (-1 = all of it)
// (NB: POSTDATA will be saved either entirely or not at all)
pref("browser.sessionstore.postdata", 0);
// on which sites to save text data, POSTDATA and cookies
// 0 = everywhere, 1 = unencrypted sites, 2 = nowhere
pref("browser.sessionstore.privacy_level", 0);
// the same as browser.sessionstore.privacy_level, but for saving deferred session data
pref("browser.sessionstore.privacy_level_deferred", 1);
// how many tabs can be reopened (per window)
pref("browser.sessionstore.max_tabs_undo", 10);
// how many windows can be reopened (per session) - on non-OS X platforms this
// pref may be ignored when dealing with pop-up windows to ensure proper startup
pref("browser.sessionstore.max_windows_undo", 3);
// number of crashes that can occur before the about:sessionrestore page is displayed
// (this pref has no effect if more than 6 hours have passed since the last crash)
pref("browser.sessionstore.max_resumed_crashes", 1);
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
pref("places.frecency.bookmarkVisitBonus", 75);
pref("places.frecency.downloadVisitBonus", 0);
pref("places.frecency.permRedirectVisitBonus", 0);
pref("places.frecency.tempRedirectVisitBonus", 0);
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

#ifndef RELEASE_BUILD
// Override submission of plugin hang reports to a different processing server
// for the smaller-volume nightly/aurora populations.
pref("toolkit.crashreporter.pluginHangSubmitURL",
     "https://hang-reports.mozilla.org/submit");
#endif

// URL for "Learn More" for Crash Reporter
pref("toolkit.crashreporter.infoURL",
     "https://www.mozilla.org/legal/privacy/firefox.html#crash-reporter");

// base URL for web-based support pages
pref("app.support.baseURL", "http://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/");

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

// Whether to start the private browsing mode at application startup
pref("browser.privatebrowsing.autostart", false);

// Don't try to alter this pref, it'll be reset the next time you use the
// bookmarking dialog
pref("browser.bookmarks.editDialog.firstEditField", "namePicker");

// Whether to use a panel that looks like an OS X sheet for customization
#ifdef XP_MACOSX
pref("toolbar.customization.usesheet", true);
#else
pref("toolbar.customization.usesheet", false);
#endif

// The default for this pref reflects whether the build is capable of IPC.
// (Turning it on in a no-IPC build will have no effect.)
#ifdef XP_MACOSX
// i386 ipc preferences
pref("dom.ipc.plugins.enabled.i386", false);
pref("dom.ipc.plugins.enabled.i386.flash player.plugin", true);
pref("dom.ipc.plugins.enabled.i386.javaplugin2_npapi.plugin", true);
pref("dom.ipc.plugins.enabled.i386.javaappletplugin.plugin", true);
pref("dom.ipc.plugins.enabled.i386.silverlight.plugin", true);
// x86_64 ipc preferences
pref("dom.ipc.plugins.enabled.x86_64", true);
#else
pref("dom.ipc.plugins.enabled", true);
#endif

#ifdef MOZ_E10S_COMPAT
pref("browser.tabs.remote", true);
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

#ifdef MOZ_SERVICES_SYNC
// The sync engines to use.
pref("services.sync.registerEngines", "Bookmarks,Form,History,Password,Prefs,Tab,Addons");
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
pref("services.sync.prefs.sync.app.update.mode", true);
pref("services.sync.prefs.sync.browser.download.manager.closeWhenDone", true);
pref("services.sync.prefs.sync.browser.download.manager.retention", true);
pref("services.sync.prefs.sync.browser.download.manager.scanWhenDone", true);
pref("services.sync.prefs.sync.browser.download.manager.showWhenStarting", true);
pref("services.sync.prefs.sync.browser.formfill.enable", true);
pref("services.sync.prefs.sync.browser.link.open_newwindow", true);
pref("services.sync.prefs.sync.browser.offline-apps.notify", true);
pref("services.sync.prefs.sync.browser.safebrowsing.enabled", true);
pref("services.sync.prefs.sync.browser.safebrowsing.malware.enabled", true);
pref("services.sync.prefs.sync.browser.search.selectedEngine", true);
pref("services.sync.prefs.sync.browser.search.update", true);
pref("services.sync.prefs.sync.browser.sessionstore.restore_on_demand", true);
pref("services.sync.prefs.sync.browser.startup.homepage", true);
pref("services.sync.prefs.sync.browser.startup.page", true);
pref("services.sync.prefs.sync.browser.tabs.autoHide", true);
pref("services.sync.prefs.sync.browser.tabs.closeButtons", true);
pref("services.sync.prefs.sync.browser.tabs.loadInBackground", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnOpen", true);
pref("services.sync.prefs.sync.browser.urlbar.autocomplete.enabled", true);
pref("services.sync.prefs.sync.browser.urlbar.default.behavior", true);
pref("services.sync.prefs.sync.browser.urlbar.maxRichResults", true);
pref("services.sync.prefs.sync.dom.disable_open_during_load", true);
pref("services.sync.prefs.sync.dom.disable_window_flip", true);
pref("services.sync.prefs.sync.dom.disable_window_move_resize", true);
pref("services.sync.prefs.sync.dom.event.contextmenu.enabled", true);
pref("services.sync.prefs.sync.extensions.personas.current", true);
pref("services.sync.prefs.sync.extensions.update.enabled", true);
pref("services.sync.prefs.sync.intl.accept_languages", true);
pref("services.sync.prefs.sync.javascript.enabled", true);
pref("services.sync.prefs.sync.layout.spellcheckDefault", true);
pref("services.sync.prefs.sync.lightweightThemes.isThemeSelected", true);
pref("services.sync.prefs.sync.lightweightThemes.usedThemes", true);
pref("services.sync.prefs.sync.network.cookie.cookieBehavior", true);
pref("services.sync.prefs.sync.network.cookie.lifetimePolicy", true);
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
pref("services.sync.prefs.sync.privacy.clearOnShutdown.passwords", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.sessions", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.siteSettings", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.enabled", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.value", true);
pref("services.sync.prefs.sync.privacy.sanitize.sanitizeOnShutdown", true);
pref("services.sync.prefs.sync.security.OCSP.disable_button.managecrl", true);
pref("services.sync.prefs.sync.security.OCSP.enabled", true);
pref("services.sync.prefs.sync.security.OCSP.require", true);
pref("services.sync.prefs.sync.security.default_personal_cert", true);
pref("services.sync.prefs.sync.security.enable_ssl3", true);
pref("services.sync.prefs.sync.security.enable_tls", true);
pref("services.sync.prefs.sync.signon.rememberSignons", true);
pref("services.sync.prefs.sync.spellchecker.dictionary", true);
pref("services.sync.prefs.sync.xpinstall.whitelist.required", true);
#endif

// Disable the error console
pref("devtools.errorconsole.enabled", false);

// Developer toolbar and GCLI preferences
pref("devtools.toolbar.enabled", true);
pref("devtools.toolbar.visible", false);
pref("devtools.gcli.allowSet", false);
pref("devtools.commands.dir", "");

// Toolbox preferences
pref("devtools.toolbox.footer.height", 250);
pref("devtools.toolbox.sidebar.width", 500);
pref("devtools.toolbox.host", "bottom");
pref("devtools.toolbox.selectedTool", "webconsole");
pref("devtools.toolbox.toolbarSpec", '["paintflashing toggle","tilt toggle","scratchpad","resize toggle"]');
pref("devtools.toolbox.sideEnabled", true);

// Enable the Inspector
pref("devtools.inspector.enabled", true);
pref("devtools.inspector.activeSidebar", "ruleview");
pref("devtools.inspector.markupPreview", false);

// Enable the Layout View
pref("devtools.layoutview.enabled", true);
pref("devtools.layoutview.open", false);

// Enable the Responsive UI tool
pref("devtools.responsiveUI.enabled", true);

// Enable the Debugger
pref("devtools.debugger.enabled", true);
pref("devtools.debugger.chrome-enabled", true);
pref("devtools.debugger.chrome-debugging-host", "localhost");
pref("devtools.debugger.chrome-debugging-port", 6080);
pref("devtools.debugger.remote-host", "localhost");
pref("devtools.debugger.remote-autoconnect", false);
pref("devtools.debugger.remote-connection-retries", 3);
pref("devtools.debugger.remote-timeout", 20000);

// The default Debugger UI settings
pref("devtools.debugger.ui.win-x", 0);
pref("devtools.debugger.ui.win-y", 0);
pref("devtools.debugger.ui.win-width", 900);
pref("devtools.debugger.ui.win-height", 400);
pref("devtools.debugger.ui.panes-sources-width", 200);
pref("devtools.debugger.ui.panes-instruments-width", 300);
pref("devtools.debugger.ui.pause-on-exceptions", false);
pref("devtools.debugger.ui.panes-visible-on-startup", false);
pref("devtools.debugger.ui.variables-sorting-enabled", true);
pref("devtools.debugger.ui.variables-only-enum-visible", false);
pref("devtools.debugger.ui.variables-searchbox-visible", false);

// Enable the Profiler
pref("devtools.profiler.enabled", true);

// Enable the Tilt inspector
pref("devtools.tilt.enabled", true);
pref("devtools.tilt.intro_transition", true);
pref("devtools.tilt.outro_transition", true);

// Enable the Scratchpad tool.
pref("devtools.scratchpad.enabled", true);

// The maximum number of recently-opened files stored.
// Setting this preference to 0 will not clear any recent files, but rather hide
// the 'Open Recent'-menu.
pref("devtools.scratchpad.recentFilesMax", 10);

// Enable the Style Editor.
pref("devtools.styleeditor.enabled", true);
pref("devtools.styleeditor.transitions", true);

// Enable tools for Chrome development.
pref("devtools.chrome.enabled", false);

// Default theme ("dark" or "light")
pref("devtools.theme", "light");

// Display the introductory text
pref("devtools.gcli.hideIntro", false);

// How eager are we to show help: never=1, sometimes=2, always=3
pref("devtools.gcli.eagerHelper", 2);

// Do we allow the 'pref set' command
pref("devtools.gcli.allowSet", false);

// Remember the Web Console filters
pref("devtools.webconsole.filter.network", true);
pref("devtools.webconsole.filter.networkinfo", true);
pref("devtools.webconsole.filter.csserror", true);
pref("devtools.webconsole.filter.cssparser", true);
pref("devtools.webconsole.filter.exception", true);
pref("devtools.webconsole.filter.jswarn", true);
pref("devtools.webconsole.filter.error", true);
pref("devtools.webconsole.filter.warn", true);
pref("devtools.webconsole.filter.info", true);
pref("devtools.webconsole.filter.log", true);

// Text size in the Web Console. Use 0 for the system default size.
pref("devtools.webconsole.fontSize", 0);

// The number of lines that are displayed in the web console for the Net,
// CSS, JS and Web Developer categories.
pref("devtools.hud.loglimit.network", 200);
pref("devtools.hud.loglimit.cssparser", 200);
pref("devtools.hud.loglimit.exception", 200);
pref("devtools.hud.loglimit.console", 200);

// The developer tools editor configuration:
// - tabsize: how many spaces to use when a Tab character is displayed.
// - expandtab: expand Tab characters to spaces.
pref("devtools.editor.tabsize", 4);
pref("devtools.editor.expandtab", true);

// Tells which component you want to use for source editing in developer tools.
//
// Available components:
//   "orion" - this is the Orion source code editor from the Eclipse project. It
//   provides programmer-specific editor features such as syntax highlighting,
//   indenting and bracket recognition.
pref("devtools.editor.component", "orion");

// Enable the Font Inspector
pref("devtools.fontinspector.enabled", true);

// Whether the character encoding menu is under the main Firefox button. This
// preference is a string so that localizers can alter it.
pref("browser.menu.showCharacterEncoding", "chrome://browser/locale/browser.properties");

// Allow using tab-modal prompts when possible.
pref("prompts.tab_modal.enabled", true);
// Whether the Panorama should animate going in/out of tabs
pref("browser.panorama.animate_zoom", true);

// Defines the url to be used for new tabs.
pref("browser.newtab.url", "about:newtab");
// Activates preloading of the new tab url.
pref("browser.newtab.preload", false);

// Toggles the content of 'about:newtab'. Shows the grid when enabled.
pref("browser.newtabpage.enabled", true);

// number of rows of newtab grid
pref("browser.newtabpage.rows", 3);

// number of columns of newtab grid
pref("browser.newtabpage.columns", 3);

// Enable the DOM fullscreen API.
pref("full-screen-api.enabled", true);

// True if the fullscreen API requires approval upon a domain entering fullscreen.
// Domains that have already had fullscreen permission granted won't re-request
// approval.
pref("full-screen-api.approval-required", true);

// Startup Crash Tracking
// number of startup crashes that can occur before starting into safe mode automatically
// (this pref has no effect if more than 6 hours have passed since the last crash)
pref("toolkit.startup.max_resumed_crashes", 3);

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

// The maximum amount of decoded image data we'll willingly keep around (we
// might keep around more than this, but we'll try to get down to this value).
// (This is intentionally on the high side; see bug 746055.)
pref("image.mem.max_decoded_image_kb", 256000);

// Default social providers
pref("social.manifest.facebook", "{\"builtin\": \"true\",\"origin\":\"https://www.facebook.com\",\"name\":\"Facebook Messenger\",\"workerURL\":\"https://www.facebook.com/desktop/fbdesktop2/socialfox/fbworker.js.php\",\"iconURL\":\"data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8%2F9hAAAAX0lEQVQ4jWP4%2F%2F8%2FAyUYTFhHzjgDxP9JxGeQDSBVMxgTbUBCxer%2Fr999%2BQ8DJBuArJksA9A10s8AXIBoA0B%2BR%2FY%2FjD%2BEwoBoA1yT5v3PbdmCE8MAshhID%2FUMoDgzUYIBj0Cgi7ar4coAAAAASUVORK5CYII%3D\",\"sidebarURL\":\"https://www.facebook.com/desktop/fbdesktop2/?socialfox=true\",\"icon32URL\":\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAACXBIWXMAAAsTAAALEwEAmpwYAAADbklEQVRYCc1Xv08UQRj99tctexAuCEFjRE0kGBEtLDSGqIWNxkYKbTAxNlY2JhaGWltNtNFeKgsKKxITK43/gCYW+IsoRhA4D47bH7fn9+bcvdm5JR7sefolC3Ozu9978+bNN7PayUv3HN3umdY0Y6IWBtSJ0HSTarXqTOiuTep6Lj+tdxAcA8RAgSmwdd2aCDs0clldYALb/FvgYVhjmfliVA2XpjEgWo0Attn42Z6WH1RFor5ehwo9XQIUZMoVn4qlCoVMSo62EvD8Kh0b3U2Xz43R2PBO6mUCGDlAf65V6MadZzT/rUimoccc2kYA4BfPHqJb105RzjJigKhRq9kEJUBIjgYVuXeL7SAI6eD+Abp5dTwVHOmEHxT50d8WBYJqSOdPj5BjW8gZR8UNqFR2xagx/65XFYaMH+BGWwiYpi4UkBPPLxTp9v1Z+lHc4DWvCQXWmIy6EjITgKowVd5Jjv7N3Hd6y5esigoOwpkJIAmMpZpLJGdiaaC4F0UmAj6bD84GCEwmB/qxMmRilmnwb/mpjAocHh4UEoNAt5NLZB7oy9OJo0PxqkAtePdhiSqunyC1LQUwWMPQaOr6GRre258Ajn4cP7KHcEXhsxpXbj+lT19X2TMNGTLVAcjcalS8gDwsQ2UOMhH4k8FkcrEn5E5ub2sKohxLK2VR77Hl9RUcsrgeRIEiVOT6z+tDbIeLy+vk+kGTCbXxycet6xhl//3f6bJEkdHYhA+mLtDIvoH4ieev5+juoxdk5+pjhALYEdXIpEB5w+NlSKSzqVQ/+H7IO6BLtl3fngGMiqhGJgIwlM6qpyUGFjySdk8m0Zg0ubeD7X9OIDEFajltRQgUJaUKx69tdgaQa0FMADuahZPMFtcEwNPm2hA7ZI5sK4aoE2NvYI+o8hkCIe7CwTv68zS0q9Dk5vpbm/8FXxitSzmMFHpsGj0wyLUheTwD2Y9fVgh1Ae0EPUgD9241ZEnld+v5kgnVZ/8fE0brVh5BK+1oCqKKF72Dk7HwBsssB/pklU1dfChy3S659H5+uelgIb+8WRv1/uGTV9Sdb5wJFlfW6fPCalMhwhSU1j2xKwKbP838GcOwJja4TqO0bjdmXxYTy1EYjFdCWoCEYZhseH/GDL3yJPHnuW6YmT7P1SlIA4768Hke4vOcsX8BE346lLHhDUQAAAAASUVORK5CYII=\", \"icon64URL\":\"data:image/png;base64,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\", \"description\":\"Keep up with friends wherever you go on the web.\",\"author\":\"Facebook\",\"homepageURL\":\"https://www.facebook.com/about/messenger-for-firefox\"}");
pref("social.manifest.mixi", "{\"builtin\": \"true\",\"origin\":\"http://mixi.jp\",\"name\":\"mixi\",\"description\":\"好きなWebサイトを見ながらmixiのチェックもできる便利なツールです。友人がつけてくれたイイネやコメントを通知するので、見落とさずにすぐにお返事ができます。また友人が今何をしているかをいつでも確認できます。\",\"workerURL\":\"http://mixi.jp/static/public/js/sidebar/firefox/worker.js\",\"sidebarURL\":\"http://mixi.jp/firefox_sidebar.pl\",\"iconURL\":\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAyRpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+IDx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuMC1jMDYxIDY0LjE0MDk0OSwgMjAxMC8xMi8wNy0xMDo1NzowMSAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvIiB4bWxuczp4bXBNTT0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wL21tLyIgeG1sbnM6c3RSZWY9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9zVHlwZS9SZXNvdXJjZVJlZiMiIHhtcDpDcmVhdG9yVG9vbD0iQWRvYmUgUGhvdG9zaG9wIENTNS4xIE1hY2ludG9zaCIgeG1wTU06SW5zdGFuY2VJRD0ieG1wLmlpZDozMDZFOUZCMDkyQzIxMUUyOEM1RTgyQ0UwMUE5RjI0RiIgeG1wTU06RG9jdW1lbnRJRD0ieG1wLmRpZDozMDZFOUZCMTkyQzIxMUUyOEM1RTgyQ0UwMUE5RjI0RiI+IDx4bXBNTTpEZXJpdmVkRnJvbSBzdFJlZjppbnN0YW5jZUlEPSJ4bXAuaWlkOkQ0QkM3NDc1OTJDMTExRTI4QzVFODJDRTAxQTlGMjRGIiBzdFJlZjpkb2N1bWVudElEPSJ4bXAuZGlkOkQ0QkM3NDc2OTJDMTExRTI4QzVFODJDRTAxQTlGMjRGIi8+IDwvcmRmOkRlc2NyaXB0aW9uPiA8L3JkZjpSREY+IDwveDp4bXBtZXRhPiA8P3hwYWNrZXQgZW5kPSJyIj8+fT8vWQAAAkxJREFUeNpi/P//PwMlgBFdIDtST4GLg6Weg50lgJmJUQAm/vff/w8/fv7Z8O3Hn8apyy89wGpAWZJxv7AAR4GYEBeDAC87AxsrEwMjIyMDyJW/fv9j+PD5J8PLt98YXr/7nti78NwCFANq0k3ny0vyJUiL8wA1szGwszEzMDOBDGBgAPny779/DD9//WV49/EHw8NnnxkePv+c2DXv7AJmkObqNNMCZVn+CiBm0LaMZuBgZ2YQVnBg4BSQZ/j27g6DqKoHA7+kAdhFrP8/AOVZGP4xMLlqKAhMYwEZIC8rWi0vyc7AD3S2lE4Iw9/f3xg+PTvDwCflwSCuGczw4+NDsCtFVDwYvu/IZxAGhgwwLLh//JGIZKnLNHMQ4mEQ4eNhBzoZ4qNnlxYzvH94iEH82xsGQXlbhruHWhiYWbkYtH1ng13159d1Bn4eNgYFRRkHJkv7QA1eLjYGVhYmeGD+/vYaif0GEgtAV8FDHmgPSD03Nw8vExMzIwMLEDMykp4GePjF3zFpGXvc/kdiWgLFyn9G9n8aJr47GYFxzHft2IJ7P57uFGZhZiLKgD9//zGwSTg/07FN0WQCJpRPWlYJjRyipp/+EuEUUKL6xyb+E6i5A6SXEUmi/cn1HQnv7mwW/v/rAyu2MPkHtIBVUPuLskXWIU4eIW+MpAw0JAhI5Xz7+Ezh++eXYEPePjjE9+X5ST5GZo5/ohphr6Q1PdYB1VSCbMfnTGkgNgPhS4fmbXl4ddvzn98+noNagJobicjOME17gBjDVoAAAwDPu94Fx/JhBAAAAABJRU5ErkJggg==\",\"icon32URL\":\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAyRpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+IDx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuMC1jMDYxIDY0LjE0MDk0OSwgMjAxMC8xMi8wNy0xMDo1NzowMSAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvIiB4bWxuczp4bXBNTT0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wL21tLyIgeG1sbnM6c3RSZWY9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9zVHlwZS9SZXNvdXJjZVJlZiMiIHhtcDpDcmVhdG9yVG9vbD0iQWRvYmUgUGhvdG9zaG9wIENTNS4xIE1hY2ludG9zaCIgeG1wTU06SW5zdGFuY2VJRD0ieG1wLmlpZDpENEJDNzQ3MzkyQzExMUUyOEM1RTgyQ0UwMUE5RjI0RiIgeG1wTU06RG9jdW1lbnRJRD0ieG1wLmRpZDpENEJDNzQ3NDkyQzExMUUyOEM1RTgyQ0UwMUE5RjI0RiI+IDx4bXBNTTpEZXJpdmVkRnJvbSBzdFJlZjppbnN0YW5jZUlEPSJ4bXAuaWlkOkQ0QkM3NDcxOTJDMTExRTI4QzVFODJDRTAxQTlGMjRGIiBzdFJlZjpkb2N1bWVudElEPSJ4bXAuZGlkOkQ0QkM3NDcyOTJDMTExRTI4QzVFODJDRTAxQTlGMjRGIi8+IDwvcmRmOkRlc2NyaXB0aW9uPiA8L3JkZjpSREY+IDwveDp4bXBtZXRhPiA8P3hwYWNrZXQgZW5kPSJyIj8+0Zya+QAABRJJREFUeNrEV11sFFUUPjOzMzs7u92Z2W532227u6Ur7VJsS02VBqnwYAIvNYJREQnhgRcIAaNI6hPxxZ+YgJFoeDCGNBhCDNSEGB54wBpFTZtWjC3B8tNq20CX7S5s9292Zz331sXd7fYvLfYkNzNz5977nfOdc+49l8lkMrCawixl8KE3mrYIPIeNfYHn2WaWYZTc/8QWLaVfTWrpgYSW/v6zs791L1uBA683Kiaj4YhF4vdaLYJXtgiA74B9YDCwwDD/LZFO6xCLpyAS0+BhJAmhR4m7j6a1M7FE6uTn566HlqzA23tbjqiKeNyhirJdMUGJWUBgDpAFCo4MQA4+EHciA5DUdIgnUhBGJQJTMbgXjI8GQ7HDJ7r6uxelwMFdjYrNKl50OcxbXGVmUGURJBEt5tg8wPmEuCOFjESRjUAoDmP3IjB2f/rkh1/2vpU7jiucePjN5uYKh2XA55brayqtYFdNlG6OYxYNTi3DsRzLgFEwUJeRhmtsbKyze3v6xr8tqsCRPc1et8v661q3bK1yWsCKlHNLsHpORXANERkkLGLMNK9/qjT8Q9/4z3kKHNrdpLjL1Su+6pKqSocFRKMhL8CWKyReBIGjjKQzbHu9V/n6p4GJkCE7oMJpO1ZdLjaUl0o0yAi2IJWBYLZDOhmFWHgEOF4Ck+IBUfZAHL8jk0N5ICbsF/F/WotCPDQCyehk3n/iEpJFLrsoRRNlH2HXa9TEo/tavHVe9c5ajwKYao8td/p3YtsB04EhCI70gKtxD1UiKwRg5NoJ+u5q2gNmuz8PMDB8Gcavd80KzmkMzOHREAzeDtZQF7y6vfG0p8LUYMNo51j28WBL2TpsfgQ1g1LVBizHU2WIhbyo0H6p1Ad233YwlrioQvHwKO0nYyWbj45PRgOzYiKjZyDDqzMusMmGDtVqpGlWTIjVxAXE2iytdt82ygihnQixlFicHV+/7VP6VN3ts1xFXEGyQpWlDvb4gee2yBIjGgVu3iCaQIBcn06hS3Lfs+B0R0SGHo730nfBXFZ0PRJnIp9xszW1dbvNJEdZdk5wAlxoBQHJSniir8icwLwGkZ1UTwbtrMNZVSkY5s91bYHFdG16yWlpwI1NRNZZssOxLLMqR7HiqIsaVqsO0FIZSELmAWsQpNhq1CTkoLLY1oyy5d4NA/oqaKBpaSivab3I+lu2f8PwivZ/6qDjJiTan45U+1rP0ei72XthMDx83o+lVn6uFpwFhUJ2SSIx3Pdz03KhuVGsmtwbj56vrG2dOQuwktnRe6mzi0+MSCt5AhaTNFovOJ6famg/sAmxhtiZ/Zm5sH7ru5cSaf6JhgMp2XTemfA07TxFwOkxnf0pmtX9vs2dNzK8/ETigVbMuqC7W/Zds8jOT4rWhKihPxELn7517dSGZPAPy0ptUAQ8DYJes+nYDbW8/pWs9UWLUlTCio8PHvzV3zH++9kyPTZuXE5cENoZsSJR/cz+W4Xg85blOPFZfLwXvn+zOXDnqjUe/FNKRceWpAxJN15tiNS2HewRJeWdQvBFXUyIW/DxMjaikP/B3/1Sfq0HcPvHj6t4A5tHOSPImnPdrkln7WZyF+hE8IdFD6WFq1qq9VABM3kSiWq/kIKGBi9n1K3VW6dc6zoGjSb5fZx/ZcXuhnPJd1+8lKl2+xK2NS9O2T1tE4LJ+hV2n5nL6hWXybGhu8hMN9nQ/g3ixd8bVuh67s9101LkHwEGAHiA8Hoi/Qh6AAAAAElFTkSuQmCC\",\"icon64URL\":\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAyRpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+IDx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuMC1jMDYxIDY0LjE0MDk0OSwgMjAxMC8xMi8wNy0xMDo1NzowMSAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvIiB4bWxuczp4bXBNTT0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wL21tLyIgeG1sbnM6c3RSZWY9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9zVHlwZS9SZXNvdXJjZVJlZiMiIHhtcDpDcmVhdG9yVG9vbD0iQWRvYmUgUGhvdG9zaG9wIENTNS4xIE1hY2ludG9zaCIgeG1wTU06SW5zdGFuY2VJRD0ieG1wLmlpZDpENEJDNzQ2RjkyQzExMUUyOEM1RTgyQ0UwMUE5RjI0RiIgeG1wTU06RG9jdW1lbnRJRD0ieG1wLmRpZDpENEJDNzQ3MDkyQzExMUUyOEM1RTgyQ0UwMUE5RjI0RiI+IDx4bXBNTTpEZXJpdmVkRnJvbSBzdFJlZjppbnN0YW5jZUlEPSJ4bXAuaWlkOkQ0QkM3NDZEOTJDMTExRTI4QzVFODJDRTAxQTlGMjRGIiBzdFJlZjpkb2N1bWVudElEPSJ4bXAuZGlkOkQ0QkM3NDZFOTJDMTExRTI4QzVFODJDRTAxQTlGMjRGIi8+IDwvcmRmOkRlc2NyaXB0aW9uPiA8L3JkZjpSREY+IDwveDp4bXBtZXRhPiA8P3hwYWNrZXQgZW5kPSJyIj8+zVczFAAACZBJREFUeNrkW3ls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\",\"author\":\"mixi\",\"homepageURL\":\"http://mixi.jp/promotion.pl?id=social_sidebar\"}");
pref("social.manifest.cliqz", "{\"builtin\": \"true\",\"origin\": \"https://mozsocial.cliqz.com\",\"name\": \"Cliqz\",\"description\": \"Cliqz for Firefox Social\",\"author\": \"Cliqz\",\"homepageURL\": \"https://mozsocial.cliqz.com/\",\"version\": \"0.9\",\"workerURL\": \"https://mozsocial.cliqz.com/worker\",\"sidebarURL\": \"https://mozsocial.cliqz.com/sidebar\",\"iconURL\": \"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAAZiS0dEAP8A/wD/oL2nkwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB9wKEgkPNcdyEiIAAANaSURBVDjLVZNPUJR1HMY/39/77vLXZYFhKf4sbEKRTlrjFFIzFgtlJmQyddAmrOlgjdIAt+yQp04Wzti9sUZwPFgzOdg0sIuThaGUOBROEqLAimsoiqy77Lvvt0s09ZyeZ+Z5ntPzCKvo2w+7Poe+/YXAQeBtEINqX9gX+2JT3mKpgznT0zhwpyvaTE/jAAACQG8H7D4CJ/YZHFkA/ICi4JW01FtXWFco5NjcPxweXMN/YADw/KMyEkHEj7qKugJQQZxlR4klUEvI74o0H/hfQXc0DG8cgd7uLah5Gle1oWSt7KzYyIH1WyXotYknYHBGJDIHk/fc1tXw5lPbsIaPXuXg2aZ1Wcn5L/3OzbKkeGVTyXpa86u044lmuRC7pkOxCbGMhTGqapuKqnfqArPHrvTP9k4inZFwgSLXz8cTvnDldj0zPcXrj+5gauZPQUT7fo9KVWUJo7f/wJDi2eoACSt1yxa79VzL6Z+lK9L0yVJ65cONpW26u/ZdOT87wUJqCUln9NvpC9IQXK8dT+2Qk5NDvFq7BdsYfWton4zfu9ydbbyHbYW2omw/raGXpGf4OP6cAurL6xCF0O0Ae598hUPnTuDPd4gtzVOc6xdLLIyx6tWYLAPUPJRbzuJyhkNj/fy6cFXzvblqWbZU+AM6Mjshlf4AP8Z+I+Wm+GFmmLG743iNXSyqxlZ0xWNl54SKgnzz2kfM3YnL7L1bpB1H60qCUr6mRPOycqTS9yZluaWMLPzC3sf28P3ckDt6+1LGatjzyDaQ4ONFm/jp2hQ3kotcvxvn1uJfYtse8myvLK08IJlOUVZQSI0vpM8FnuHFsudrkunUBemKNrcLenQxZeuxcVtWXME4GVh6oJlsj3Q07NQNxdXSffYzNqzNxRjFqKXv1bZLla/yvnFd97gqA/leR8r9adRRzSSSmlFXWF7WUEGpXIxfpcAnOLpC2nVIaWp810Bb9Wh87CsB6Iy+kJVlrA8u3+X9Gzfd0Mik8+9NPF6vbq57WJJ5N7FtC4X7ItI+vL3/awDpijbR0zgIQOdguEqMmU6kVCdiLhkX2Ri0UIuxiw88EYWMoB8Pt3yXWJ2zrJLOSBOHw4N0RppeFuGkCDkArnLJQrZ+2jgwv+ptOLWN4ZbTAPwNmUptT0KshT0AAAAASUVORK5CYII=\",\"icon32URL\": \"data:image/png;base64,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\",\"icon64URL\": \"data:image/png;base64,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\"}");
pref("social.manifest.msnnow", "{\"builtin\": \"true\",\"origin\": \"http://now.msn.com\",\"name\": \"msnNOW\",\"workerURL\": \"http://now.msn.com/js/firefoxworker\",\"iconURL\": \"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAA2RpVFh0WE1MomNvbS5hZG9iZS54bXAAAAAAADw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+IDx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuMy1jMDExIDY2LjE0NTY2MSwgMjAxMi8wMi8wNi0xNDo1NjoyNyAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wTU09Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9tbS8iIHhtbG5zOnN0UmVmPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VSZWYjIiB4bWxuczp4bXA9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8iIHhtcE1NOk9yaWdpbmFsRG9jdW1lbnRJRD0ieG1wLmRpZDozMkY0QkZFREQxM0JFMTExQUE0MEYwQTFBRTFEN0QwMyIgeG1wTU06RG9jdW1lbnRJRD0ieG1wLmRpZDo5RjUxOThGOTJDRjkxMUUyQTQ5NzkxRkQxQjI1Mzg2NCIgeG1wTU06SW5zdGFuY2VJRD0ieG1wLmlpZDo5RjUxOThGODJDRjkxMUUyQTQ5NzkxRkQxQjI1Mzg2NCIgeG1wOkNyZWF0b3JUb29sPSJBZG9iZSBQaG90b3Nob3AgQ1M1IFdpbmRvd3MiPiA8eG1wTU06RGVyaXZlZEZyb20gc3RSZWY6aW5zdGFuY2VJRD0ieG1wLmlpZDowMTVCMTdDQ0M1MkFFMjExODNDRENFQTc0Q0EwNzEyMCIgc3RSZWY6ZG9jdW1lbnRJRD0ieG1wLmRpZDozMkY0QkZFREQxM0JFMTExQUE0MEYwQTFBRTFEN0QwMyIvPiA8L3JkZjpEZXNjcmlwdGlvbj4gPC9yZGY6UkRGPiA8L3g6eG1wbWV0YT4gPD94cGFja2V0IGVuZD0iciI/Ppp4JKMAAABiSURBVHjaYvz//z8DJYCJgULAAmNwJEwk2Sk/FuQzUuyCUQMGgwGM+FIitrQBintKXKCG0wVA27Cl0t9IfEUgfoDmGuwuAEkAAbLzJdE14/UC1DUwOW6ggS/ICkRiAECAAQBDhCEtt+sSkAAAAABJRU5ErkJggg==\",\"icon32URL\": \"data:image/gif;base64,R0lGODlhIAAgAMQAAABgkm+lwf///y99psDY5K/N3c/h6k+RtA9pmO/1+I+5zz+HrR9zn7/X5N/r8V+bu3+vyJ/D1oCwySB0oGCcu+Ht80CIrRBqmTB+pwAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAAHAP8ALAAAAAAgACAAAAXAICCOZGmeaKqubOu+cCzPdG3fraDvfO//OhFwSNwJi0jfUQBpKCCRnQNCdQgMDV0iK9haBcvS4ooYIZyAAjMtUAC4YUYiwQAkDgBIAgIYJAAPAnWBDAxGAEZjAgtvYYgHCAYAFpEACoeJOowNZVZ/CAIFfRcOfQBfYIhBipsPfRADgDoTshgABzxhrG8JjCILCToBb21smEUODahIS0nOzc7MAATU1dbX1qjYBDUSOhQ4JN7g4SMS5OXp6uvs7eshADs=\",\"icon64URL\": \"data:image/gif;base64, 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\",\"sidebarURL\": \"http://now.msn.com/sidebar\",\"description\": \"See what's trending in realtime on Twitter, Facebook, Bing, YouTube and more.\",\"author\": \"Microsoft\",\"homepageURL\": \"http://now.msn.com/now-for-firefox\"}");

// comma separated list of domain origins (e.g. https://domain.com) for
// providers that can install from their own website without user warnings.
// entries are
pref("social.whitelist", "");
// omma separated list of domain origins (e.g. https://domain.com) for directory
// websites (e.g. AMO) that can install providers for other sites
pref("social.directories", "https://addons.mozilla.org");
// remote-install allows any website to activate a provider, with extended UI
// notifying user of installation. we can later pref off remote install if
// necessary. This does not affect whitelisted and directory installs.
pref("social.remote-install.enabled", false);

pref("social.sidebar.open", true);
pref("social.sidebar.unload_timeout_ms", 10000);
pref("social.toast-notifications.enabled", true);

pref("dom.identity.enabled", false);

// Override the Gecko-default value of false for Firefox.
pref("plain_text.wrap_long_lines", true);

#ifndef RELEASE_BUILD
// Enable Web Audio for Firefox Desktop in Nightly and Aurora
pref("media.webaudio.enabled", true);
#endif

// If this turns true, Moz*Gesture events are not called stopPropagation()
// before content.
pref("dom.debug.propagate_gesture_events_through_content", false);
