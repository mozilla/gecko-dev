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
        // == edit-menu ==
        case "menu_preferences":
          openPreferences(undefined);
          break;

        // == view-menu ==
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

        // == history-menu ==
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

        // == menu_HelpPopup ==
        // (Duplicated in PanelUI._onHelpCommand)
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

    document
      .getElementById("historyMenuPopup")
      .addEventListener("command", event => {
        // Handle commands/clicks on the descending menuitems that are
        // history entries.
        let historyMenu = document.getElementById("history-menu");
        historyMenu._placesView._onCommand(event);
      });

    mainMenuBar.addEventListener("popupshowing", event => {
      // On macOS, we don't track whether activation of the native menubar happened
      // with the keyboard.
      if (AppConstants.platform != "macosx") {
        // We only set the "openedwithkey" if a specific menu like "Edit" was opened
        // instead of the general menu bar. (e.g. Alt+E instead of just Alt)
        if (event.target.parentNode.parentNode == this) {
          this.setAttribute(
            "openedwithkey",
            event.target.parentNode.openedWithKey
          );
        }
      }

      switch (event.target.id) {
        case "menu_FilePopup":
          gFileMenu.onPopupShowing(event);
          break;
        case "menu_newUserContextPopup":
          createUserContextMenu(event);
          break;
        case "menu_EditPopup":
          updateEditUIVisibility();
          break;
        case "view-menu-popup":
          onViewToolbarsPopupShowing(event);
          break;
        case "viewSidebarMenu":
          SidebarController.setMegalistMenubarVisibility(event);
          break;
        case "pageStyleMenuPopup":
          gPageStyleMenu.fillPopup(event.target);
          break;
        case "historyMenuPopup":
          if (!event.target.parentNode._placesView) {
            new HistoryMenu(event);
          }
          break;
        case "historyUndoPopup":
          document
            .getElementById("history-menu")
            ._placesView.populateUndoSubmenu();
          break;
        case "historyUndoWindowPopup":
          document
            .getElementById("history-menu")
            ._placesView.populateUndoWindowSubmenu();
          break;
        case "bookmarksMenuPopup":
          BookmarkingUI.onMainMenuPopupShowing(event);
          if (!event.target.parentNode._placesView) {
            new PlacesMenu(
              event,
              `place:parent=${PlacesUtils.bookmarks.menuGuid}`
            );
          }
          break;
        case "bookmarksToolbarFolderPopup":
          if (!event.target.parentNode._placesView) {
            new PlacesMenu(
              event,
              `place:parent=${PlacesUtils.bookmarks.toolbarGuid}`
            );
          }
          break;
        case "otherBookmarksFolderPopup":
          if (!event.target.parentNode._placesView) {
            new PlacesMenu(
              event,
              `place:parent=${PlacesUtils.bookmarks.unfiledGuid}`
            );
          }
          break;
        case "mobileBookmarksFolderPopup":
          if (!event.target.parentNode._placesView) {
            new PlacesMenu(
              event,
              `place:parent=${PlacesUtils.bookmarks.mobileGuid}`
            );
          }
          break;
        case "menu_HelpPopup":
          buildHelpMenu();
          break;
      }
    });

    document
      .getElementById("menu_EditPopup")
      .addEventListener("popuphidden", () => {
        updateEditUIVisibility();
      });
  },
  { once: true }
);
