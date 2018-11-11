/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
var isMulet = "ResponsiveUI" in browserWindow;

// Enable touch event shim on desktop that translates mouse events
// into touch ones
function enableTouch() {
  let require = Cu.import('resource://devtools/shared/Loader.jsm', {})
                  .devtools.require;
  let { TouchEventSimulator } = require('devtools/shared/touch/simulator');
  let touchEventSimulator = new TouchEventSimulator(shell.contentBrowser);
  touchEventSimulator.start();
}

// Some additional buttons are displayed on simulators to fake hardware buttons.
function setupButtons() {
  let link = document.createElement('link');
  link.type = 'text/css';
  link.rel = 'stylesheet';
  link.href = 'chrome://b2g/content/desktop.css';
  document.head.appendChild(link);

  let footer = document.createElement('footer');
  footer.id = 'controls';
  document.body.appendChild(footer);
  let homeButton = document.createElement('button');
  homeButton.id = 'home-button';
  footer.appendChild(homeButton);
  let rotateButton = document.createElement('button');
  rotateButton.id = 'rotate-button';
  footer.appendChild(rotateButton);

  homeButton.addEventListener('mousedown', function() {
    let window = shell.contentBrowser.contentWindow;
    let e = new window.KeyboardEvent('keydown', {key: 'Home'});
    window.dispatchEvent(e);
    homeButton.classList.add('active');
  });
  homeButton.addEventListener('mouseup', function() {
    let window = shell.contentBrowser.contentWindow;
    let e = new window.KeyboardEvent('keyup', {key: 'Home'});
    window.dispatchEvent(e);
    homeButton.classList.remove('active');
  });

  Cu.import("resource://gre/modules/GlobalSimulatorScreen.jsm");
  rotateButton.addEventListener('mousedown', function() {
    rotateButton.classList.add('active');
  });
  rotateButton.addEventListener('mouseup', function() {
    GlobalSimulatorScreen.flipScreen();
    rotateButton.classList.remove('active');
  });
}

function setupStorage() {
  let directory = null;

  // Get the --storage-path argument from the command line.
  try {
    let service = Cc['@mozilla.org/commandlinehandler/general-startup;1?type=b2gcmds'].getService(Ci.nsISupports);
    let args = service.wrappedJSObject.cmdLine;
    if (args) {
      let path = args.handleFlagWithParam('storage-path', false);
      directory = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
      directory.initWithPath(path);
    }
  } catch(e) {
    directory = null;
  }

  // Otherwise, default to 'storage' folder within current profile.
  if (!directory) {
    directory = Services.dirsvc.get('ProfD', Ci.nsIFile);
    directory.append('storage');
    if (!directory.exists()) {
      directory.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("755", 8));
    }
  }
  dump("Set storage path to: " + directory.path + "\n");

  // This is the magic, where we override the default location for the storages.
  Services.prefs.setCharPref('device.storage.overrideRootDir', directory.path);
}

function checkDebuggerPort() {
  // XXX: To be removed once bug 942756 lands.
  // We are hacking 'unix-domain-socket' pref by setting a tcp port (number).
  // SocketListener.open detects that it isn't a file path (string), and starts
  // listening on the tcp port given here as command line argument.

  // Get the command line arguments that were passed to the b2g client
  let args;
  try {
    let service = Cc["@mozilla.org/commandlinehandler/general-startup;1?type=b2gcmds"].getService(Ci.nsISupports);
    args = service.wrappedJSObject.cmdLine;
  } catch(e) {}

  if (!args) {
    return;
  }

  let dbgport;
  try {
    dbgport = args.handleFlagWithParam('start-debugger-server', false);
  } catch(e) {}

  if (dbgport) {
    dump('Opening debugger server on ' + dbgport + '\n');
    Services.prefs.setCharPref('devtools.debugger.unix-domain-socket', dbgport);
    navigator.mozSettings.createLock().set(
      {'debugger.remote-mode': 'adb-devtools'});
  }
}


function initResponsiveDesign() {
  Cu.import('resource://devtools/client/responsivedesign/responsivedesign.jsm');
  ResponsiveUIManager.on('on', function(event, {tab:tab}) {
    let responsive = ResponsiveUIManager.getResponsiveUIForTab(tab);
    let document = tab.ownerDocument;

    // Only tweak reponsive mode for shell.html tabs.
    if (tab.linkedBrowser.contentWindow != window) {
      return;
    }

    // Disable transition as they mess up with screen size handler
    responsive.transitionsEnabled = false;

    responsive.buildPhoneUI();

    responsive.rotatebutton.addEventListener('command', function (evt) {
      GlobalSimulatorScreen.flipScreen();
      evt.stopImmediatePropagation();
      evt.preventDefault();
    }, true);

    // Enable touch events
    responsive.enableTouch();
  });


  let mgr = browserWindow.ResponsiveUI.ResponsiveUIManager;
  mgr.toggle(browserWindow, browserWindow.gBrowser.selectedTab);

}

function openDevtools() {
  // Open devtool panel while maximizing its size according to screen size
  Services.prefs.setIntPref('devtools.toolbox.sidebar.width',
                            browserWindow.outerWidth - 550);
  Services.prefs.setCharPref('devtools.toolbox.host', 'side');
  let {gDevTools} = Cu.import('resource://devtools/client/framework/gDevTools.jsm', {});
  let {devtools} = Cu.import("resource://devtools/shared/Loader.jsm", {});
  let target = devtools.TargetFactory.forTab(browserWindow.gBrowser.selectedTab);
  gDevTools.showToolbox(target);
}

window.addEventListener('ContentStart', function() {
  // On Firefox Mulet, touch events are enabled within the responsive mode
  if (!isMulet) {
    enableTouch();
  }
  if (Services.prefs.getBoolPref('b2g.software-buttons')) {
    setupButtons();
  }
  checkDebuggerPort();
  setupStorage();
  // On Firefox mulet, we automagically enable the responsive mode
  // and show the devtools
  if (isMulet) {
    initResponsiveDesign(browserWindow);
    openDevtools();
  }
});
