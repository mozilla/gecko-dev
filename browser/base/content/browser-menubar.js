/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

document.addEventListener(
  "DOMContentLoaded",
  () => {
    let mainMenuBar = document.getElementById("main-menubar");

    mainMenuBar.addEventListener("command", event => {
      switch (event.target.id) {
        case "menu_preferences":
          openPreferences(undefined);
          break;
        case "menu_pageStyleNoStyle":
          gPageStyleMenu.disableStyle();
          break;
        case "menu_pageStylePersistentOnly":
          gPageStyleMenu.switchStyleSheet(null);
          break;
        case "repair-text-encoding":
          BrowserCommands.forceEncodingDetection();
          break;
        case "documentDirection-swap":
          gBrowser.selectedBrowser.sendMessageToActor(
            "SwitchDocumentDirection",
            {},
            "SwitchDocumentDirection",
            "roots"
          );
          break;
        case "historyMenuPopup":
          event.target.parentNode._placesView._onCommand(event);
          break;
        case "sync-tabs-menuitem":
          gSync.openSyncedTabsPanel();
          break;
        case "hiddenTabsMenu":
          gTabsPanel.showHiddenTabsPanel(event, "hidden-tabs-menuitem");
          break;
        case "sync-setup":
          gSync.openPrefs("menubar");
          break;
        case "sync-enable":
          gSync.openPrefs("menubar");
          break;
        case "sync-unverifieditem":
          gSync.openPrefs("menubar");
          break;
        case "sync-syncnowitem":
          gSync.doSync(event);
          break;
        case "sync-reauthitem":
          gSync.openSignInAgainPage("menubar");
          break;
        case "menu_openFirefoxView":
          FirefoxViewHandler.openTab();
          break;
        case "menu_openHelp":
          openHelpLink("firefox-help");
          break;
        case "menu_layout_debugger":
          toOpenWindowByType(
            "mozapp:layoutdebug",
            "chrome://layoutdebug/content/layoutdebug.xhtml"
          );
          break;
        case "feedbackPage":
          openFeedbackPage();
          break;
        case "helpSafeMode":
          safeModeRestart();
          break;
        case "troubleShooting":
          openTroubleshootingPage();
          break;
        case "help_reportSiteIssue":
          ReportSiteIssue();
          break;
        case "menu_HelpPopup_reportPhishingtoolmenu":
          openUILink(gSafeBrowsing.getReportURL("Phish"), event, {
            triggeringPrincipal:
              Services.scriptSecurityManager.createNullPrincipal({}),
          });
          break;
        case "menu_HelpPopup_reportPhishingErrortoolmenu":
          ReportFalseDeceptiveSite();
          break;
        case "helpSwitchDevice":
          openSwitchingDevicesPage();
          break;
        case "aboutName":
          openAboutDialog();
          break;
        case "helpPolicySupport":
          openTrustedLinkIn(Services.policies.getSupportMenu().URL.href, "tab");
          break;
      }
    });
  },
  { once: true }
);
