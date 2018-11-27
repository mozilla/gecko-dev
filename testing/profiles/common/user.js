// Common preferences file used by both unittest and perf harnesses.
/* globals user_pref */
user_pref("app.update.checkInstallTime", false);
user_pref("app.update.disabledForTesting", true);
user_pref("browser.chrome.guess_favicon", false);
user_pref("browser.dom.window.dump.enabled", true);
user_pref("devtools.console.stdout.chrome", true);
// Use a python-eval-able empty JSON array even though asrouter expects plain object
user_pref("browser.newtabpage.activity-stream.asrouter.providers.snippets", "[]");
user_pref("browser.newtabpage.activity-stream.feeds.section.topstories", false);
user_pref("browser.newtabpage.activity-stream.feeds.snippets", false);
user_pref("browser.newtabpage.activity-stream.tippyTop.service.endpoint", "");
// For Activity Stream firstrun page, use an empty string to avoid fetching.
user_pref("browser.newtabpage.activity-stream.fxaccounts.endpoint", "");
// Tell the search service we are running in the US.  This also has the desired
// side-effect of preventing our geoip lookup.
user_pref("browser.search.region", "US");
// This will prevent HTTP requests for region defaults.
user_pref("browser.search.geoSpecificDefaults", false);
// Disable android snippets
user_pref("browser.snippets.enabled", false);
user_pref("browser.snippets.syncPromo.enabled", false);
// Disable webapp updates.  Yes, it is supposed to be an integer.
user_pref("browser.webapps.checkForUpdates", 0);
// We do not wish to display datareporting policy notifications as it might
// cause other tests to fail. Tests that wish to test the notification functionality
// should explicitly disable this pref.
user_pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);
user_pref("dom.max_chrome_script_run_time", 0);
user_pref("dom.max_script_run_time", 0); // no slow script dialogs
user_pref("dom.send_after_paint_to_content", true);
// Only load extensions from the application and user profile
// AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
user_pref("extensions.enabledScopes", 5);
user_pref("extensions.legacy.enabled", true);
// Turn off extension updates so they don't bother tests
user_pref("extensions.update.enabled", false);
// Disable useragent updates.
user_pref("general.useragent.updates.enabled", false);
// Ensure WR doesn't get enabled in tests unless we do it explicitly with the MOZ_WEBRENDER envvar.
user_pref("gfx.webrender.all.qualified", false);
user_pref("hangmonitor.timeout", 0); // no hang monitor
user_pref("media.gmp-manager.updateEnabled", false);
// Make enablePrivilege continue to work for test code. :-(
user_pref("security.turn_off_all_security_so_that_viruses_can_take_over_this_computer", true);
user_pref("xpinstall.signatures.required", false);
// Prevent Remote Settings to issue non local connections.
user_pref("services.settings.server", "http://localhost/remote-settings-dummy/v1");
// Ensure autoplay is enabled for all platforms.
user_pref("media.autoplay.default", 0); // 0=Allowed, 1=Blocked, 2=Prompt
user_pref("media.autoplay.enabled.user-gestures-needed", true);
user_pref("media.autoplay.ask-permission", false);
user_pref("media.autoplay.block-webaudio", false);
user_pref("media.allowed-to-play.enabled", true);
user_pref("toolkit.telemetry.coverage.endpoint.base", "http://localhost");
