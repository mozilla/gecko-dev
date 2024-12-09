/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

document.addEventListener(
  "MozBeforeInitialXULLayout",
  () => {
    // <commandset id="mainCommandSet"> defined in browser-sets.inc
    document
      .getElementById("mainCommandSet")
      // eslint-disable-next-line complexity
      .addEventListener("command", event => {
        switch (event.target.id) {
          case "cmd_newNavigator":
            OpenBrowserWindow();
            break;
          case "cmd_handleBackspace":
            BrowserCommands.handleBackspace();
            break;
          case "cmd_handleShiftBackspace":
            BrowserCommands.handleShiftBackspace();
            break;
          case "cmd_newNavigatorTab":
            BrowserCommands.openTab({ event });
            break;
          case "cmd_newNavigatorTabNoEvent":
            BrowserCommands.openTab();
            break;
          case "Browser:OpenFile":
            BrowserCommands.openFileWindow();
            break;
          case "Browser:SavePage":
            saveBrowser(gBrowser.selectedBrowser);
            break;
          case "Browser:SendLink":
            MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser);
            break;
          case "cmd_pageSetup":
            PrintUtils.showPageSetup();
            break;
          case "cmd_print":
            PrintUtils.startPrintWindow(
              gBrowser.selectedBrowser.browsingContext
            );
            break;
          case "cmd_printPreviewToggle":
            PrintUtils.togglePrintPreview(
              gBrowser.selectedBrowser.browsingContext
            );
            break;
          case "cmd_file_importFromAnotherBrowser":
            MigrationUtils.showMigrationWizard(window, {
              entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU,
            });
            break;
          case "cmd_help_importFromAnotherBrowser":
            MigrationUtils.showMigrationWizard(window, {
              entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.HELP_MENU,
            });
            break;
          case "cmd_close":
            BrowserCommands.closeTabOrWindow(event);
            break;
          case "cmd_closeWindow":
            BrowserCommands.tryToCloseWindow(event);
            break;
          case "cmd_minimizeWindow":
            window.minimize();
            break;
          case "cmd_maximizeWindow":
            window.maximize();
            break;
          case "cmd_restoreWindow":
            window.fullScreen ? BrowserCommands.fullScreen() : window.restore();
            break;
          case "cmd_toggleMute":
            gBrowser.toggleMuteAudioOnMultiSelectedTabs(gBrowser.selectedTab);
            break;
          case "cmd_CustomizeToolbars":
            gCustomizeMode.enter();
            break;
          case "cmd_toggleOfflineStatus":
            BrowserOffline.toggleOfflineStatus();
            break;
          case "cmd_quitApplication":
            goQuitApplication(event);
            break;
          case "View:AboutProcesses":
            switchToTabHavingURI("about:processes", true);
            break;
          case "View:PageSource":
            BrowserCommands.viewSource(window.gBrowser.selectedBrowser);
            break;
          case "View:PageInfo":
            BrowserCommands.pageInfo();
            break;
          case "View:FullScreen":
            BrowserCommands.fullScreen();
            break;
          case "View:ReaderView":
            AboutReaderParent.toggleReaderMode(event);
            break;
          case "View:PictureInPicture":
            PictureInPicture.onCommand(event);
            break;
          case "cmd_find":
            gLazyFindCommand("onFindCommand");
            break;
          case "cmd_findAgain":
            gLazyFindCommand("onFindAgainCommand", false);
            break;
          case "cmd_findPrevious":
            gLazyFindCommand("onFindAgainCommand", true);
            break;
          case "cmd_findSelection":
            gLazyFindCommand("onFindSelectionCommand");
            break;
          case "cmd_translate":
            FullPageTranslationsPanel.open(event);
            break;
          case "Browser:AddBookmarkAs":
            PlacesCommandHook.bookmarkPage();
            break;
          case "Browser:SearchBookmarks":
            PlacesCommandHook.searchBookmarks();
            break;
          case "Browser:BookmarkAllTabs":
            PlacesCommandHook.bookmarkTabs();
            break;
          case "Browser:Back":
            BrowserCommands.back();
            break;
          case "Browser:BackOrBackDuplicate":
            BrowserCommands.back(event);
            break;
          case "Browser:Forward":
            BrowserCommands.forward();
            break;
          case "Browser:ForwardOrForwardDuplicate":
            BrowserCommands.forward(event);
            break;
          case "Browser:Stop":
            BrowserCommands.stop();
            break;
          case "Browser:Reload":
            if (event.shiftKey) {
              BrowserCommands.reloadSkipCache();
            } else {
              BrowserCommands.reload();
            }
            break;
          case "Browser:ReloadOrDuplicate":
            BrowserCommands.reloadOrDuplicate(event);
            break;
          case "Browser:ReloadSkipCache":
            BrowserCommands.reloadSkipCache();
            break;
          case "Browser:NextTab":
            gBrowser.tabContainer.advanceSelectedTab(1, true);
            break;
          case "Browser:PrevTab":
            gBrowser.tabContainer.advanceSelectedTab(-1, true);
            break;
          case "Browser:ShowAllTabs":
            gTabsPanel.showAllTabsPanel();
            break;
          case "cmd_fullZoomReduce":
            FullZoom.reduce();
            break;
          case "cmd_fullZoomEnlarge":
            FullZoom.enlarge();
            break;
          case "cmd_fullZoomReset":
            FullZoom.reset();
            FullZoom.resetScalingZoom();
            break;
          case "cmd_fullZoomToggle":
            ZoomManager.toggleZoom();
            break;
          case "cmd_gestureRotateLeft":
            gGestureSupport.rotate(event.sourceEvent);
            break;
          case "cmd_gestureRotateRight":
            gGestureSupport.rotate(event.sourceEvent);
            break;
          case "cmd_gestureRotateEnd":
            gGestureSupport.rotateEnd();
            break;
          case "Browser:OpenLocation":
            openLocation(event);
            break;
          case "Browser:RestoreLastSession":
            SessionStore.restoreLastSession();
            break;
          case "Browser:NewUserContextTab":
            openNewUserContextTab(event.sourceEvent);
            break;
          case "Browser:OpenAboutContainers":
            openPreferences("paneContainers");
            break;
          // deliberate fallthrough
          case "Profiles:CreateProfile":
          case "Profiles:ManageProfiles":
          case "Profiles:LaunchProfile":
            gProfiles.handleCommand(event);
            break;
          case "Tools:Search":
            BrowserSearch.webSearch();
            break;
          case "Tools:Downloads":
            BrowserCommands.downloadsUI();
            break;
          case "Tools:Addons":
            BrowserAddonUI.openAddonsMgr();
            break;
          case "Tools:Sanitize":
            Sanitizer.showUI(window);
            break;
          case "Tools:PrivateBrowsing":
            OpenBrowserWindow({ private: true });
            break;
          case "Browser:Screenshot":
            ScreenshotsUtils.notify(window, "Shortcut");
            break;
          case "History:UndoCloseTab":
            undoCloseTab();
            break;
          case "History:UndoCloseWindow":
            undoCloseWindow();
            break;
          case "History:RestoreLastClosedTabOrWindowOrSession":
            restoreLastClosedTabOrWindowOrSession();
            break;
          case "History:SearchHistory":
            PlacesCommandHook.searchHistory();
            break;
          case "wrCaptureCmd":
            gGfxUtils.webrenderCapture();
            break;
          case "wrToggleCaptureSequenceCmd":
            gGfxUtils.toggleWebrenderCaptureSequence();
            break;
          case "windowRecordingCmd":
            gGfxUtils.toggleWindowRecording();
            break;
          case "zoomWindow":
            zoomWindow();
            break;
        }
      });

    document.getElementById("mainKeyset").addEventListener("command", event => {
      const SIDEBAR_REVAMP_PREF = "sidebar.revamp";
      const SIDEBAR_REVAMP_ENABLED = Services.prefs.getBoolPref(
        SIDEBAR_REVAMP_PREF,
        false
      );
      switch (event.target.id) {
        case "goHome":
          BrowserCommands.home();
          break;
        case "bookmarkAllTabsKb":
          PlacesCommandHook.bookmarkTabs();
          break;
        case "viewBookmarksSidebarKb":
          SidebarController.toggle("viewBookmarksSidebar");
          break;
        case "viewBookmarksToolbarKb":
          BookmarkingUI.toggleBookmarksToolbar("shortcut");
          break;
        case "toggleSidebarKb":
          if (SIDEBAR_REVAMP_ENABLED) {
            SidebarController.handleToolbarButtonClick();
          }
          break;
        case "key_gotoHistory":
          SidebarController.toggle("viewHistorySidebar");
          break;

        case "key_selectTab1":
        case "key_selectTab2":
        case "key_selectTab3":
        case "key_selectTab4":
        case "key_selectTab5":
        case "key_selectTab6":
        case "key_selectTab7":
        case "key_selectTab8": {
          let index = event.target.id.at(-1) - 1;
          gBrowser.selectTabAtIndex(index, event);
          break;
        }
        case "key_selectLastTab":
          gBrowser.selectTabAtIndex(-1, event);
          break;

        case "key_openHelpMac":
          openHelpLink("firefox-osxkey");
          break;
      }
    });
  },
  { once: true }
);
