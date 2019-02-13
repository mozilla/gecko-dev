/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/SharedPromptUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");

const LoginInfo =
      Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                             "nsILoginInfo", "init");

/**
 * Constants for password prompt telemetry.
 * Mirrored in mobile/android/components/LoginManagerPrompter.js */
const PROMPT_DISPLAYED = 0;

const PROMPT_ADD_OR_UPDATE = 1;
const PROMPT_NOTNOW = 2;
const PROMPT_NEVER = 3;

/*
 * LoginManagerPromptFactory
 *
 * Implements nsIPromptFactory
 *
 * Invoked by [toolkit/components/prompts/src/nsPrompter.js]
 */
function LoginManagerPromptFactory() {
  Services.obs.addObserver(this, "quit-application-granted", true);
  Services.obs.addObserver(this, "passwordmgr-crypto-login", true);
  Services.obs.addObserver(this, "passwordmgr-crypto-loginCanceled", true);
}

LoginManagerPromptFactory.prototype = {

  classID : Components.ID("{749e62f4-60ae-4569-a8a2-de78b649660e}"),
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIPromptFactory, Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  _asyncPrompts : {},
  _asyncPromptInProgress : false,

  observe : function (subject, topic, data) {
    this.log("Observed: " + topic);
    if (topic == "quit-application-granted") {
      this._cancelPendingPrompts();
    } else if (topic == "passwordmgr-crypto-login") {
      // Start processing the deferred prompters.
      this._doAsyncPrompt();
    } else if (topic == "passwordmgr-crypto-loginCanceled") {
      // User canceled a Master Password prompt, so go ahead and cancel
      // all pending auth prompts to avoid nagging over and over.
      this._cancelPendingPrompts();
    }
  },

  getPrompt : function (aWindow, aIID) {
    var prompt = new LoginManagerPrompter().QueryInterface(aIID);
    prompt.init(aWindow, this);
    return prompt;
  },

  _doAsyncPrompt : function() {
    if (this._asyncPromptInProgress) {
      this.log("_doAsyncPrompt bypassed, already in progress");
      return;
    }

    // Find the first prompt key we have in the queue
    var hashKey = null;
    for (hashKey in this._asyncPrompts)
      break;

    if (!hashKey) {
      this.log("_doAsyncPrompt:run bypassed, no prompts in the queue");
      return;
    }

    // If login manger has logins for this host, defer prompting if we're
    // already waiting on a master password entry.
    var prompt = this._asyncPrompts[hashKey];
    var prompter = prompt.prompter;
    var [hostname, httpRealm] = prompter._getAuthTarget(prompt.channel, prompt.authInfo);
    var hasLogins = (prompter._pwmgr.countLogins(hostname, null, httpRealm) > 0);
    if (hasLogins && prompter._pwmgr.uiBusy) {
      this.log("_doAsyncPrompt:run bypassed, master password UI busy");
      return;
    }

    this._asyncPromptInProgress = true;
    prompt.inProgress = true;

    var self = this;

    var runnable = {
      run : function() {
        var ok = false;
        try {
          self.log("_doAsyncPrompt:run - performing the prompt for '" + hashKey + "'");
          ok = prompter.promptAuth(prompt.channel,
                                   prompt.level,
                                   prompt.authInfo);
        } catch (e if (e instanceof Components.Exception) &&
                       e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
          self.log("_doAsyncPrompt:run bypassed, UI is not available in this context");
        } catch (e) {
          Components.utils.reportError("LoginManagerPrompter: " +
              "_doAsyncPrompt:run: " + e + "\n");
        }

        delete self._asyncPrompts[hashKey];
        prompt.inProgress = false;
        self._asyncPromptInProgress = false;

        for each (var consumer in prompt.consumers) {
          if (!consumer.callback)
            // Not having a callback means that consumer didn't provide it
            // or canceled the notification
            continue;

          self.log("Calling back to " + consumer.callback + " ok=" + ok);
          try {
            if (ok)
              consumer.callback.onAuthAvailable(consumer.context, prompt.authInfo);
            else
              consumer.callback.onAuthCancelled(consumer.context, true);
          } catch (e) { /* Throw away exceptions caused by callback */ }
        }
        self._doAsyncPrompt();
      }
    };

    Services.tm.mainThread.dispatch(runnable, Ci.nsIThread.DISPATCH_NORMAL);
    this.log("_doAsyncPrompt:run dispatched");
  },


  _cancelPendingPrompts : function() {
    this.log("Canceling all pending prompts...");
    var asyncPrompts = this._asyncPrompts;
    this.__proto__._asyncPrompts = {};

    for each (var prompt in asyncPrompts) {
      // Watch out! If this prompt is currently prompting, let it handle
      // notifying the callbacks of success/failure, since it's already
      // asking the user for input. Reusing a callback can be crashy.
      if (prompt.inProgress) {
        this.log("skipping a prompt in progress");
        continue;
      }

      for each (var consumer in prompt.consumers) {
        if (!consumer.callback)
          continue;

        this.log("Canceling async auth prompt callback " + consumer.callback);
        try {
          consumer.callback.onAuthCancelled(consumer.context, true);
        } catch (e) { /* Just ignore exceptions from the callback */ }
      }
    }
  },
}; // end of LoginManagerPromptFactory implementation

XPCOMUtils.defineLazyGetter(this.LoginManagerPromptFactory.prototype, "log", () => {
  let logger = LoginHelper.createLogger("Login PromptFactory");
  return logger.log.bind(logger);
});




/* ==================== LoginManagerPrompter ==================== */




/*
 * LoginManagerPrompter
 *
 * Implements interfaces for prompting the user to enter/save/change auth info.
 *
 * nsIAuthPrompt: Used by SeaMonkey, Thunderbird, but not Firefox.
 *
 * nsIAuthPrompt2: Is invoked by a channel for protocol-based authentication
 * (eg HTTP Authenticate, FTP login).
 *
 * nsILoginManagerPrompter: Used by Login Manager for saving/changing logins
 * found in HTML forms.
 */
function LoginManagerPrompter() {}

LoginManagerPrompter.prototype = {

  classID : Components.ID("{8aa66d77-1bbb-45a6-991e-b8f47751c291}"),
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIAuthPrompt,
                                          Ci.nsIAuthPrompt2,
                                          Ci.nsILoginManagerPrompter]),

  _factory       : null,
  _window        : null,
  _browser       : null,
  _opener        : null,

  __pwmgr : null, // Password Manager service
  get _pwmgr() {
    if (!this.__pwmgr)
      this.__pwmgr = Cc["@mozilla.org/login-manager;1"].
                     getService(Ci.nsILoginManager);
    return this.__pwmgr;
  },

  __promptService : null, // Prompt service for user interaction
  get _promptService() {
    if (!this.__promptService)
      this.__promptService =
          Cc["@mozilla.org/embedcomp/prompt-service;1"].
          getService(Ci.nsIPromptService2);
    return this.__promptService;
  },


  __strBundle : null, // String bundle for L10N
  get _strBundle() {
    if (!this.__strBundle) {
      var bunService = Cc["@mozilla.org/intl/stringbundle;1"].
                       getService(Ci.nsIStringBundleService);
      this.__strBundle = bunService.createBundle(
                  "chrome://passwordmgr/locale/passwordmgr.properties");
      if (!this.__strBundle)
        throw new Error("String bundle for Login Manager not present!");
    }

    return this.__strBundle;
  },


  __ellipsis : null,
  get _ellipsis() {
    if (!this.__ellipsis) {
      this.__ellipsis = "\u2026";
      try {
        this.__ellipsis = Services.prefs.getComplexValue(
                            "intl.ellipsis", Ci.nsIPrefLocalizedString).data;
      } catch (e) { }
    }
    return this.__ellipsis;
  },


  // Whether we are in private browsing mode
  get _inPrivateBrowsing() {
    if (this._window) {
      return PrivateBrowsingUtils.isContentWindowPrivate(this._window);
    } else {
      // If we don't that we're in private browsing mode if the caller did
      // not provide a window.  The callers which really care about this
      // will indeed pass down a window to us, and for those who don't,
      // we can just assume that we don't want to save the entered login
      // information.
      return true;
    }
  },




  /* ---------- nsIAuthPrompt prompts ---------- */


  /*
   * prompt
   *
   * Wrapper around the prompt service prompt. Saving random fields here
   * doesn't really make sense and therefore isn't implemented.
   */
  prompt : function (aDialogTitle, aText, aPasswordRealm,
                     aSavePassword, aDefaultText, aResult) {
    if (aSavePassword != Ci.nsIAuthPrompt.SAVE_PASSWORD_NEVER)
      throw new Components.Exception("prompt only supports SAVE_PASSWORD_NEVER",
                                     Cr.NS_ERROR_NOT_IMPLEMENTED);

    this.log("===== prompt() called =====");

    if (aDefaultText) {
      aResult.value = aDefaultText;
    }

    return this._promptService.prompt(this._window,
           aDialogTitle, aText, aResult, null, {});
  },


  /*
   * promptUsernameAndPassword
   *
   * Looks up a username and password in the database. Will prompt the user
   * with a dialog, even if a username and password are found.
   */
  promptUsernameAndPassword : function (aDialogTitle, aText, aPasswordRealm,
                                       aSavePassword, aUsername, aPassword) {
    this.log("===== promptUsernameAndPassword() called =====");

    if (aSavePassword == Ci.nsIAuthPrompt.SAVE_PASSWORD_FOR_SESSION)
      throw new Components.Exception("promptUsernameAndPassword doesn't support SAVE_PASSWORD_FOR_SESSION",
                                     Cr.NS_ERROR_NOT_IMPLEMENTED);

    var selectedLogin = null;
    var checkBox = { value : false };
    var checkBoxLabel = null;
    var [hostname, realm, unused] = this._getRealmInfo(aPasswordRealm);

    // If hostname is null, we can't save this login.
    if (hostname) {
      var canRememberLogin;
      if (this._inPrivateBrowsing)
        canRememberLogin = false;
      else
        canRememberLogin = (aSavePassword ==
                            Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY) &&
                           this._pwmgr.getLoginSavingEnabled(hostname);

      // if checkBoxLabel is null, the checkbox won't be shown at all.
      if (canRememberLogin)
        checkBoxLabel = this._getLocalizedString("rememberPassword");

      // Look for existing logins.
      var foundLogins = this._pwmgr.findLogins({}, hostname, null,
                                               realm);

      // XXX Like the original code, we can't deal with multiple
      // account selection. (bug 227632)
      if (foundLogins.length > 0) {
        selectedLogin = foundLogins[0];

        // If the caller provided a username, try to use it. If they
        // provided only a password, this will try to find a password-only
        // login (or return null if none exists).
        if (aUsername.value)
          selectedLogin = this._repickSelectedLogin(foundLogins,
                                                    aUsername.value);

        if (selectedLogin) {
          checkBox.value = true;
          aUsername.value = selectedLogin.username;
          // If the caller provided a password, prefer it.
          if (!aPassword.value)
            aPassword.value = selectedLogin.password;
        }
      }
    }

    var ok = this._promptService.promptUsernameAndPassword(this._window,
                aDialogTitle, aText, aUsername, aPassword,
                checkBoxLabel, checkBox);

    if (!ok || !checkBox.value || !hostname)
      return ok;

    if (!aPassword.value) {
      this.log("No password entered, so won't offer to save.");
      return ok;
    }

    // XXX We can't prompt with multiple logins yet (bug 227632), so
    // the entered login might correspond to an existing login
    // other than the one we originally selected.
    selectedLogin = this._repickSelectedLogin(foundLogins, aUsername.value);

    // If we didn't find an existing login, or if the username
    // changed, save as a new login.
    if (!selectedLogin) {
      // add as new
      this.log("New login seen for " + realm);
      var newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                     createInstance(Ci.nsILoginInfo);
      newLogin.init(hostname, null, realm,
                    aUsername.value, aPassword.value, "", "");
      this._pwmgr.addLogin(newLogin);
    } else if (aPassword.value != selectedLogin.password) {
      // update password
      this.log("Updating password for  " + realm);
      this._updateLogin(selectedLogin, aPassword.value);
    } else {
      this.log("Login unchanged, no further action needed.");
      this._updateLogin(selectedLogin);
    }

    return ok;
  },


  /*
   * promptPassword
   *
   * If a password is found in the database for the password realm, it is
   * returned straight away without displaying a dialog.
   *
   * If a password is not found in the database, the user will be prompted
   * with a dialog with a text field and ok/cancel buttons. If the user
   * allows it, then the password will be saved in the database.
   */
  promptPassword : function (aDialogTitle, aText, aPasswordRealm,
                             aSavePassword, aPassword) {
    this.log("===== promptPassword called() =====");

    if (aSavePassword == Ci.nsIAuthPrompt.SAVE_PASSWORD_FOR_SESSION)
      throw new Components.Exception("promptPassword doesn't support SAVE_PASSWORD_FOR_SESSION",
                                     Cr.NS_ERROR_NOT_IMPLEMENTED);

    var checkBox = { value : false };
    var checkBoxLabel = null;
    var [hostname, realm, username] = this._getRealmInfo(aPasswordRealm);

    username = decodeURIComponent(username);

    // If hostname is null, we can't save this login.
    if (hostname && !this._inPrivateBrowsing) {
      var canRememberLogin = (aSavePassword ==
                              Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY) &&
                             this._pwmgr.getLoginSavingEnabled(hostname);

      // if checkBoxLabel is null, the checkbox won't be shown at all.
      if (canRememberLogin)
        checkBoxLabel = this._getLocalizedString("rememberPassword");

      if (!aPassword.value) {
        // Look for existing logins.
        var foundLogins = this._pwmgr.findLogins({}, hostname, null,
                                                 realm);

        // XXX Like the original code, we can't deal with multiple
        // account selection (bug 227632). We can deal with finding the
        // account based on the supplied username - but in this case we'll
        // just return the first match.
        for (var i = 0; i < foundLogins.length; ++i) {
          if (foundLogins[i].username == username) {
            aPassword.value = foundLogins[i].password;
            // wallet returned straight away, so this mimics that code
            return true;
          }
        }
      }
    }

    var ok = this._promptService.promptPassword(this._window, aDialogTitle,
                                                aText, aPassword,
                                                checkBoxLabel, checkBox);

    if (ok && checkBox.value && hostname && aPassword.value) {
      var newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                     createInstance(Ci.nsILoginInfo);
      newLogin.init(hostname, null, realm, username,
                    aPassword.value, "", "");

      this.log("New login seen for " + realm);

      this._pwmgr.addLogin(newLogin);
    }

    return ok;
  },

  /* ---------- nsIAuthPrompt helpers ---------- */


  /**
   * Given aRealmString, such as "http://user@example.com/foo", returns an
   * array of:
   *   - the formatted hostname
   *   - the realm (hostname + path)
   *   - the username, if present
   *
   * If aRealmString is in the format produced by NS_GetAuthKey for HTTP[S]
   * channels, e.g. "example.com:80 (httprealm)", null is returned for all
   * arguments to let callers know the login can't be saved because we don't
   * know whether it's http or https.
   */
  _getRealmInfo : function (aRealmString) {
    var httpRealm = /^.+ \(.+\)$/;
    if (httpRealm.test(aRealmString))
      return [null, null, null];

    var uri = Services.io.newURI(aRealmString, null, null);
    var pathname = "";

    if (uri.path != "/")
      pathname = uri.path;

    var formattedHostname = this._getFormattedHostname(uri);

    return [formattedHostname, formattedHostname + pathname, uri.username];
  },

  /* ---------- nsIAuthPrompt2 prompts ---------- */




  /*
   * promptAuth
   *
   * Implementation of nsIAuthPrompt2.
   *
   * nsIChannel aChannel
   * int        aLevel
   * nsIAuthInformation aAuthInfo
   */
  promptAuth : function (aChannel, aLevel, aAuthInfo) {
    var selectedLogin = null;
    var checkbox = { value : false };
    var checkboxLabel = null;
    var epicfail = false;
    var canAutologin = false;

    try {

      this.log("===== promptAuth called =====");

      // If the user submits a login but it fails, we need to remove the
      // notification bar that was displayed. Conveniently, the user will
      // be prompted for authentication again, which brings us here.
      this._removeLoginNotifications();

      var [hostname, httpRealm] = this._getAuthTarget(aChannel, aAuthInfo);


      // Looks for existing logins to prefill the prompt with.
      var foundLogins = this._pwmgr.findLogins({},
                                  hostname, null, httpRealm);
      this.log("found " + foundLogins.length + " matching logins.");

      // XXX Can't select from multiple accounts yet. (bug 227632)
      if (foundLogins.length > 0) {
        selectedLogin = foundLogins[0];
        this._SetAuthInfo(aAuthInfo, selectedLogin.username,
                                     selectedLogin.password);

        // Allow automatic proxy login
        if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY &&
            !(aAuthInfo.flags & Ci.nsIAuthInformation.PREVIOUS_FAILED) &&
            Services.prefs.getBoolPref("signon.autologin.proxy") &&
            !this._inPrivateBrowsing) {

          this.log("Autologin enabled, skipping auth prompt.");
          canAutologin = true;
        }

        checkbox.value = true;
      }

      var canRememberLogin = this._pwmgr.getLoginSavingEnabled(hostname);
      if (this._inPrivateBrowsing)
        canRememberLogin = false;

      // if checkboxLabel is null, the checkbox won't be shown at all.
      var notifyBox = this._getNotifyBox();
      if (canRememberLogin && !notifyBox)
        checkboxLabel = this._getLocalizedString("rememberPassword");
    } catch (e) {
      // Ignore any errors and display the prompt anyway.
      epicfail = true;
      Components.utils.reportError("LoginManagerPrompter: " +
          "Epic fail in promptAuth: " + e + "\n");
    }

    var ok = canAutologin;
    if (!ok) {
      if (this._window)
        PromptUtils.fireDialogEvent(this._window, "DOMWillOpenModalDialog", this._browser);
      ok = this._promptService.promptAuth(this._window,
                                          aChannel, aLevel, aAuthInfo,
                                          checkboxLabel, checkbox);
    }

    // If there's a notification box, use it to allow the user to
    // determine if the login should be saved. If there isn't a
    // notification box, only save the login if the user set the
    // checkbox to do so.
    var rememberLogin = notifyBox ? canRememberLogin : checkbox.value;
    if (!ok || !rememberLogin || epicfail)
      return ok;

    try {
      var [username, password] = this._GetAuthInfo(aAuthInfo);

      if (!password) {
        this.log("No password entered, so won't offer to save.");
        return ok;
      }

      // XXX We can't prompt with multiple logins yet (bug 227632), so
      // the entered login might correspond to an existing login
      // other than the one we originally selected.
      selectedLogin = this._repickSelectedLogin(foundLogins, username);

      // If we didn't find an existing login, or if the username
      // changed, save as a new login.
      if (!selectedLogin) {
        this.log("New login seen for " + username +
                 " @ " + hostname + " (" + httpRealm + ")");

        var newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                       createInstance(Ci.nsILoginInfo);
        newLogin.init(hostname, null, httpRealm,
                      username, password, "", "");
        var notifyObj = this._getPopupNote() || notifyBox;
        if (notifyObj)
          this._showSaveLoginNotification(notifyObj, newLogin);
        else
          this._pwmgr.addLogin(newLogin);

      } else if (password != selectedLogin.password) {

        this.log("Updating password for " + username +
                 " @ " + hostname + " (" + httpRealm + ")");
        var notifyObj = this._getPopupNote() || notifyBox;
        if (notifyObj)
          this._showChangeLoginNotification(notifyObj,
                                            selectedLogin, password);
        else
          this._updateLogin(selectedLogin, password);

      } else {
        this.log("Login unchanged, no further action needed.");
        this._updateLogin(selectedLogin);
      }
    } catch (e) {
      Components.utils.reportError("LoginManagerPrompter: " +
          "Fail2 in promptAuth: " + e + "\n");
    }

    return ok;
  },

  asyncPromptAuth : function (aChannel, aCallback, aContext, aLevel, aAuthInfo) {
    var cancelable = null;

    try {
      this.log("===== asyncPromptAuth called =====");

      // If the user submits a login but it fails, we need to remove the
      // notification bar that was displayed. Conveniently, the user will
      // be prompted for authentication again, which brings us here.
      this._removeLoginNotifications();

      cancelable = this._newAsyncPromptConsumer(aCallback, aContext);

      var [hostname, httpRealm] = this._getAuthTarget(aChannel, aAuthInfo);

      var hashKey = aLevel + "|" + hostname + "|" + httpRealm;
      this.log("Async prompt key = " + hashKey);
      var asyncPrompt = this._factory._asyncPrompts[hashKey];
      if (asyncPrompt) {
        this.log("Prompt bound to an existing one in the queue, callback = " + aCallback);
        asyncPrompt.consumers.push(cancelable);
        return cancelable;
      }

      this.log("Adding new prompt to the queue, callback = " + aCallback);
      asyncPrompt = {
        consumers: [cancelable],
        channel: aChannel,
        authInfo: aAuthInfo,
        level: aLevel,
        inProgress : false,
        prompter: this
      };

      this._factory._asyncPrompts[hashKey] = asyncPrompt;
      this._factory._doAsyncPrompt();
    }
    catch (e) {
      Components.utils.reportError("LoginManagerPrompter: " +
          "asyncPromptAuth: " + e + "\nFalling back to promptAuth\n");
      // Fail the prompt operation to let the consumer fall back
      // to synchronous promptAuth method
      throw e;
    }

    return cancelable;
  },




  /* ---------- nsILoginManagerPrompter prompts ---------- */




  /*
   * init
   *
   */
  init : function (aWindow, aFactory) {
    this._window = aWindow;
    this._factory = aFactory || null;
    this._browser = null;
    this._opener = null;

    this.log("===== initialized =====");
  },

  setE10sData : function (aBrowser, aOpener) {
    if (!(this._window instanceof Ci.nsIDOMChromeWindow))
      throw new Error("Unexpected call");
    this._browser = aBrowser;
    this._opener = aOpener;
  },


  /*
   * promptToSavePassword
   *
   */
  promptToSavePassword : function (aLogin) {
    var notifyObj = this._getPopupNote() || this._getNotifyBox();
    if (notifyObj)
      this._showSaveLoginNotification(notifyObj, aLogin);
    else
      this._showSaveLoginDialog(aLogin);
  },


  /*
   * _showLoginNotification
   *
   * Displays a notification bar.
   *
   */
  _showLoginNotification : function (aNotifyBox, aName, aText, aButtons) {
    var oldBar = aNotifyBox.getNotificationWithValue(aName);
    const priority = aNotifyBox.PRIORITY_INFO_MEDIUM;

    this.log("Adding new " + aName + " notification bar");
    var newBar = aNotifyBox.appendNotification(
                            aText, aName,
                            "chrome://mozapps/skin/passwordmgr/key.png",
                            priority, aButtons);

    // The page we're going to hasn't loaded yet, so we want to persist
    // across the first location change.
    newBar.persistence++;

    // Sites like Gmail perform a funky redirect dance before you end up
    // at the post-authentication page. I don't see a good way to
    // heuristically determine when to ignore such location changes, so
    // we'll try ignoring location changes based on a time interval.
    newBar.timeout = Date.now() + 20000; // 20 seconds

    if (oldBar) {
      this.log("(...and removing old " + aName + " notification bar)");
      aNotifyBox.removeNotification(oldBar);
    }
  },

  /**
   * Displays the PopupNotifications.jsm doorhanger for password save or change.
   *
   * @param {nsILoginInfo} login
   *        Login to save or change. For changes, this login should contain the
   *        new password.
   * @param {string} type
   *        This is "password-save" or "password-change" depending on the
   *        original notification type. This is used for telemetry and tests.
   */
  _showLoginCaptureDoorhanger(login, type) {
    let { browser } = this._getNotifyWindow();

    let saveMsgNames = {
      prompt: "rememberPasswordMsgNoUsername",
      buttonLabel: "notifyBarRememberPasswordButtonText",
      buttonAccessKey: "notifyBarRememberPasswordButtonAccessKey",
    };

    let changeMsgNames = {
      // We reuse the existing message, even if it expects a username, until we
      // switch to the final terminology in bug 1144856.
      prompt: "updatePasswordMsg",
      buttonLabel: "notifyBarUpdateButtonText",
      buttonAccessKey: "notifyBarUpdateButtonAccessKey",
    };

    let initialMsgNames = type == "password-save" ? saveMsgNames
                                                  : changeMsgNames;

    let histogramName = type == "password-save" ? "PWMGR_PROMPT_REMEMBER_ACTION"
                                                : "PWMGR_PROMPT_UPDATE_ACTION";
    let histogram = Services.telemetry.getHistogramById(histogramName);
    histogram.add(PROMPT_DISPLAYED);

    let chromeDoc = browser.ownerDocument;

    let currentNotification;

    let updateButtonStatus = (element) => {
      let mainActionButton = chromeDoc.getAnonymousElementByAttribute(element.button, "anonid", "button");
      // Disable the main button inside the menu-button if the password field is empty.
      if (login.password.length == 0) {
        mainActionButton.setAttribute("disabled", true);
        chromeDoc.getElementById("password-notification-password")
                 .classList.add("popup-notification-invalid-input");
      } else {
        mainActionButton.removeAttribute("disabled");
        chromeDoc.getElementById("password-notification-password")
                 .classList.remove("popup-notification-invalid-input");
      }
    };

    let updateButtonLabel = () => {
      let foundLogins = Services.logins.findLogins({}, login.hostname,
                                                   login.formSubmitURL,
                                                   login.httpRealm);
      let logins = foundLogins.filter(l => l.username == login.username);
      let msgNames = (logins.length == 0) ? saveMsgNames : changeMsgNames;

      // Update the label based on whether this will be a new login or not.
      let label = this._getLocalizedString(msgNames.buttonLabel);
      let accessKey = this._getLocalizedString(msgNames.buttonAccessKey);

      // Update the labels for the next time the panel is opened.
      currentNotification.mainAction.label = label;
      currentNotification.mainAction.accessKey = accessKey;

      // Update the labels in real time if the notification is displayed.
      let element = [...currentNotification.owner.panel.childNodes]
                    .find(n => n.notification == currentNotification);
      if (element) {
        element.setAttribute("buttonlabel", label);
        element.setAttribute("buttonaccesskey", accessKey);
        updateButtonStatus(element);
      }
    };

    let writeDataToUI = () => {
      // setAttribute is used since the <textbox> binding may not be attached yet.
      chromeDoc.getElementById("password-notification-username")
               .setAttribute("placeholder", usernamePlaceholder);
      chromeDoc.getElementById("password-notification-username")
               .setAttribute("value", login.username);
      let passwordField = chromeDoc.getElementById("password-notification-password");
      // Ensure the type is reset so the field is masked.
      passwordField.setAttribute("type", "password");
      passwordField.setAttribute("value", login.password);
      if (Services.prefs.getBoolPref("signon.rememberSignons.visibilityToggle")) {
        passwordField.setAttribute("show-content", showPasswordPlaceholder);
      } else {
        passwordField.setAttribute("show-content", "");
      }
      updateButtonLabel();
    };

    let readDataFromUI = () => {
      login.username =
        chromeDoc.getElementById("password-notification-username").value;
      login.password =
        chromeDoc.getElementById("password-notification-password").value;
    };

    let onInput = () => {
      readDataFromUI();
      updateButtonLabel();
    };

    let onPasswordFocus = (focusEvent) => {
      let passwordField = chromeDoc.getElementById("password-notification-password");
      // Gets the caret position before changing the type of the textbox
      let selectionStart = passwordField.selectionStart;
      let selectionEnd = passwordField.selectionEnd;
      if (focusEvent.rangeParent != null) {
        // Check for a click over the SHOW placeholder
        selectionStart = passwordField.value.length;
        selectionEnd = passwordField.value.length;
      }
      passwordField.setAttribute("type", "");
      passwordField.selectionStart = selectionStart;
      passwordField.selectionEnd = selectionEnd;
    };

    let onPasswordBlur = () => {
      // Use setAttribute in case the <textbox> binding isn't applied.
      chromeDoc.getElementById("password-notification-password").setAttribute("type", "password");
    };

    let onNotificationClick = (clickEvent) => {
      // Removes focus from textboxes when we click elsewhere on the doorhanger.
      let focusedElement = Services.focus.focusedElement;
      if (!focusedElement || focusedElement.nodeName != "html:input") {
        // No input is focused so we don't need to blur
        return;
      }

      let focusedBindingParent = chromeDoc.getBindingParent(focusedElement);
      if (!focusedBindingParent || focusedBindingParent.nodeName != "textbox" ||
          clickEvent.explicitOriginalTarget == focusedBindingParent) {
        // The focus wasn't in a textbox or the click was in the focused textbox.
        return;
      }
      focusedBindingParent.blur();
    };

    let persistData = () => {
      let foundLogins = Services.logins.findLogins({}, login.hostname,
                                                   login.formSubmitURL,
                                                   login.httpRealm);
      let logins = foundLogins.filter(l => l.username == login.username);
      if (logins.length == 0) {
        // The original login we have been provided with might have its own
        // metadata, but we don't want it propagated to the newly created one.
        Services.logins.addLogin(new LoginInfo(login.hostname,
                                               login.formSubmitURL,
                                               login.httpRealm,
                                               login.username,
                                               login.password,
                                               login.usernameField,
                                               login.passwordField));
      } else if (logins.length == 1) {
        if (logins[0].password == login.password) {
          // We only want to touch the login's use count and last used time.
          this._updateLogin(logins[0], null);
        } else {
          this._updateLogin(logins[0], login.password);
        }
      } else {
        Cu.reportError("Unexpected match of multiple logins.");
      }
    };

    // The main action is the "Remember" or "Update" button.
    let mainAction = {
      label: this._getLocalizedString(initialMsgNames.buttonLabel),
      accessKey: this._getLocalizedString(initialMsgNames.buttonAccessKey),
      callback: () => {
        histogram.add(PROMPT_ADD_OR_UPDATE);
        if(histogramName == "PWMGR_PROMPT_REMEMBER_ACTION")
        {
          Services.obs.notifyObservers(null, 'LoginStats:NewSavedPassword', null);
        }
        readDataFromUI();
        persistData();
        browser.focus();
      }
    };

    // Include a "Never for this site" button when saving a new password.
    let secondaryActions = type == "password-save" ? [{
      label: this._getLocalizedString("notifyBarNeverRememberButtonText"),
      accessKey: this._getLocalizedString("notifyBarNeverRememberButtonAccessKey"),
      callback: () => {
        histogram.add(PROMPT_NEVER);
        Services.logins.setLoginSavingEnabled(login.hostname, false);
        browser.focus();
      }
    }] : null;

    let usernamePlaceholder = this._getLocalizedString("noUsernamePlaceholder");
    let showPasswordPlaceholder = this._getLocalizedString("showPasswordPlaceholder");
    let displayHost = this._getShortDisplayHost(login.hostname);

    this._getPopupNote().show(
      browser,
      "password",
      this._getLocalizedString(initialMsgNames.prompt, [displayHost]),
      "password-notification-icon",
      mainAction,
      secondaryActions,
      {
        timeout: Date.now() + 10000,
        origin: login.hostname,
        persistWhileVisible: true,
        passwordNotificationType: type,
        eventCallback: function (topic) {
          switch (topic) {
            case "showing":
              currentNotification = this;
              chromeDoc.getElementById("password-notification-username")
                       .addEventListener("input", onInput);
              chromeDoc.getElementById("password-notification-password")
                       .addEventListener("input", onInput);
              if (Services.prefs.getBoolPref("signon.rememberSignons.visibilityToggle")) {
                chromeDoc.getElementById("password-notification-password")
                         .addEventListener("focus", onPasswordFocus);
              }
              chromeDoc.getElementById("password-notification-password")
                       .addEventListener("blur", onPasswordBlur);
              break;
            case "shown":
              chromeDoc.getElementById("notification-popup")
                         .addEventListener("click", onNotificationClick);
              writeDataToUI();
              break;
            case "dismissed":
              readDataFromUI();
              // Fall through.
            case "removed":
              currentNotification = null;
              chromeDoc.getElementById("notification-popup")
                       .removeEventListener("click", onNotificationClick);
              chromeDoc.getElementById("password-notification-username")
                       .removeEventListener("input", onInput);
              chromeDoc.getElementById("password-notification-password")
                       .removeEventListener("input", onInput);
              chromeDoc.getElementById("password-notification-password")
                       .removeEventListener("focus", onPasswordFocus);
              chromeDoc.getElementById("password-notification-password")
                       .removeEventListener("blur", onPasswordBlur);
              break;
          }
          return false;
        },
      }
    );
  },

  /*
   * _showSaveLoginNotification
   *
   * Displays a notification bar or a popup notification, to allow the user
   * to save the specified login. This allows the user to see the results of
   * their login, and only save a login which they know worked.
   *
   * @param aNotifyObj
   *        A notification box or a popup notification.
   */
  _showSaveLoginNotification : function (aNotifyObj, aLogin) {
    // Ugh. We can't use the strings from the popup window, because they
    // have the access key marked in the string (eg "Mo&zilla"), along
    // with some weird rules for handling access keys that do not occur
    // in the string, for L10N. See commonDialog.js's setLabelForNode().
    var neverButtonText =
          this._getLocalizedString("notifyBarNeverRememberButtonText");
    var neverButtonAccessKey =
          this._getLocalizedString("notifyBarNeverRememberButtonAccessKey");
    var rememberButtonText =
          this._getLocalizedString("notifyBarRememberPasswordButtonText");
    var rememberButtonAccessKey =
          this._getLocalizedString("notifyBarRememberPasswordButtonAccessKey");

    var displayHost = this._getShortDisplayHost(aLogin.hostname);
    var notificationText = this._getLocalizedString(
                                  "rememberPasswordMsgNoUsername",
                                  [displayHost]);

    // The callbacks in |buttons| have a closure to access the variables
    // in scope here; set one to |this._pwmgr| so we can get back to pwmgr
    // without a getService() call.
    var pwmgr = this._pwmgr;

    // Notification is a PopupNotification
    if (aNotifyObj == this._getPopupNote()) {
      this._showLoginCaptureDoorhanger(aLogin, "password-save");
    } else {
      var notNowButtonText =
            this._getLocalizedString("notifyBarNotNowButtonText");
      var notNowButtonAccessKey =
            this._getLocalizedString("notifyBarNotNowButtonAccessKey");
      var buttons = [
        // "Remember" button
        {
          label:     rememberButtonText,
          accessKey: rememberButtonAccessKey,
          popup:     null,
          callback: function(aNotifyObj, aButton) {
            pwmgr.addLogin(aLogin);
          }
        },

        // "Never for this site" button
        {
          label:     neverButtonText,
          accessKey: neverButtonAccessKey,
          popup:     null,
          callback: function(aNotifyObj, aButton) {
            pwmgr.setLoginSavingEnabled(aLogin.hostname, false);
          }
        },

        // "Not now" button
        {
          label:     notNowButtonText,
          accessKey: notNowButtonAccessKey,
          popup:     null,
          callback:  function() { /* NOP */ }
        }
      ];

      this._showLoginNotification(aNotifyObj, "password-save",
                                  notificationText, buttons);
    }
  },


  /*
   * _removeLoginNotifications
   *
   */
  _removeLoginNotifications : function () {
    var popupNote = this._getPopupNote();
    if (popupNote)
      popupNote = popupNote.getNotification("password");
    if (popupNote)
      popupNote.remove();

    var notifyBox = this._getNotifyBox();
    if (notifyBox) {
      var oldBar = notifyBox.getNotificationWithValue("password-save");
      if (oldBar) {
        this.log("Removing save-password notification bar.");
        notifyBox.removeNotification(oldBar);
      }

      oldBar = notifyBox.getNotificationWithValue("password-change");
      if (oldBar) {
        this.log("Removing change-password notification bar.");
        notifyBox.removeNotification(oldBar);
      }
    }
  },


  /*
   * _showSaveLoginDialog
   *
   * Called when we detect a new login in a form submission,
   * asks the user what to do.
   *
   */
  _showSaveLoginDialog : function (aLogin) {
    const buttonFlags = Ci.nsIPrompt.BUTTON_POS_1_DEFAULT +
        (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
        (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1) +
        (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_2);

    var displayHost = this._getShortDisplayHost(aLogin.hostname);

    var dialogText;
    if (aLogin.username) {
      var displayUser = this._sanitizeUsername(aLogin.username);
      dialogText = this._getLocalizedString(
                           "rememberPasswordMsg",
                           [displayUser, displayHost]);
    } else {
      dialogText = this._getLocalizedString(
                           "rememberPasswordMsgNoUsername",
                           [displayHost]);

    }
    var dialogTitle        = this._getLocalizedString(
                                    "savePasswordTitle");
    var neverButtonText    = this._getLocalizedString(
                                    "neverForSiteButtonText");
    var rememberButtonText = this._getLocalizedString(
                                    "rememberButtonText");
    var notNowButtonText   = this._getLocalizedString(
                                    "notNowButtonText");

    this.log("Prompting user to save/ignore login");
    var userChoice = this._promptService.confirmEx(this._window,
                                        dialogTitle, dialogText,
                                        buttonFlags, rememberButtonText,
                                        notNowButtonText, neverButtonText,
                                        null, {});
    //  Returns:
    //   0 - Save the login
    //   1 - Ignore the login this time
    //   2 - Never save logins for this site
    if (userChoice == 2) {
      this.log("Disabling " + aLogin.hostname + " logins by request.");
      this._pwmgr.setLoginSavingEnabled(aLogin.hostname, false);
    } else if (userChoice == 0) {
      this.log("Saving login for " + aLogin.hostname);
      this._pwmgr.addLogin(aLogin);
    } else {
      // userChoice == 1 --> just ignore the login.
      this.log("Ignoring login.");
    }
  },


  /*
   * promptToChangePassword
   *
   * Called when we think we detect a password change for an existing
   * login, when the form being submitted contains multiple password
   * fields.
   *
   */
  promptToChangePassword : function (aOldLogin, aNewLogin) {
    var notifyObj = this._getPopupNote() || this._getNotifyBox();

    if (notifyObj)
      this._showChangeLoginNotification(notifyObj, aOldLogin,
                                        aNewLogin.password);
    else
      this._showChangeLoginDialog(aOldLogin, aNewLogin.password);
  },


  /*
   * _showChangeLoginNotification
   *
   * Shows the Change Password notification bar or popup notification.
   *
   * @param aNotifyObj
   *        A notification box or a popup notification.
   */
  _showChangeLoginNotification : function (aNotifyObj, aOldLogin, aNewPassword) {
    var changeButtonText =
          this._getLocalizedString("notifyBarUpdateButtonText");
    var changeButtonAccessKey =
          this._getLocalizedString("notifyBarUpdateButtonAccessKey");

    // We reuse the existing message, even if it expects a username, until we
    // switch to the final terminology in bug 1144856.
    var displayHost = this._getShortDisplayHost(aOldLogin.hostname);
    var notificationText = this._getLocalizedString("updatePasswordMsg",
                                                    [displayHost]);

    // The callbacks in |buttons| have a closure to access the variables
    // in scope here; set one to |this._pwmgr| so we can get back to pwmgr
    // without a getService() call.
    var self = this;

    // Notification is a PopupNotification
    if (aNotifyObj == this._getPopupNote()) {
      aOldLogin.password = aNewPassword;
      this._showLoginCaptureDoorhanger(aOldLogin, "password-change");
    } else {
      var dontChangeButtonText =
            this._getLocalizedString("notifyBarDontChangeButtonText");
      var dontChangeButtonAccessKey =
            this._getLocalizedString("notifyBarDontChangeButtonAccessKey");
      var buttons = [
        // "Yes" button
        {
          label:     changeButtonText,
          accessKey: changeButtonAccessKey,
          popup:     null,
          callback:  function(aNotifyObj, aButton) {
            self._updateLogin(aOldLogin, aNewPassword);
          }
        },

        // "No" button
        {
          label:     dontChangeButtonText,
          accessKey: dontChangeButtonAccessKey,
          popup:     null,
          callback:  function(aNotifyObj, aButton) {
            // do nothing
          }
        }
      ];

      this._showLoginNotification(aNotifyObj, "password-change",
                                  notificationText, buttons);
    }
  },


  /*
   * _showChangeLoginDialog
   *
   * Shows the Change Password dialog.
   *
   */
  _showChangeLoginDialog : function (aOldLogin, aNewPassword) {
    const buttonFlags = Ci.nsIPrompt.STD_YES_NO_BUTTONS;

    var dialogText;
    if (aOldLogin.username)
      dialogText  = this._getLocalizedString(
                              "updatePasswordMsg",
                              [aOldLogin.username]);
    else
      dialogText  = this._getLocalizedString(
                              "updatePasswordMsgNoUser");

    var dialogTitle = this._getLocalizedString(
                                "passwordChangeTitle");

    // returns 0 for yes, 1 for no.
    var ok = !this._promptService.confirmEx(this._window,
                            dialogTitle, dialogText, buttonFlags,
                            null, null, null,
                            null, {});
    if (ok) {
      this.log("Updating password for user " + aOldLogin.username);
      this._updateLogin(aOldLogin, aNewPassword);
    }
  },


  /*
   * promptToChangePasswordWithUsernames
   *
   * Called when we detect a password change in a form submission, but we
   * don't know which existing login (username) it's for. Asks the user
   * to select a username and confirm the password change.
   *
   * Note: The caller doesn't know the username for aNewLogin, so this
   *       function fills in .username and .usernameField with the values
   *       from the login selected by the user.
   *
   * Note; XPCOM stupidity: |count| is just |logins.length|.
   */
  promptToChangePasswordWithUsernames : function (logins, count, aNewLogin) {
    const buttonFlags = Ci.nsIPrompt.STD_YES_NO_BUTTONS;

    var usernames = logins.map(function (l) l.username);
    var dialogText  = this._getLocalizedString("userSelectText");
    var dialogTitle = this._getLocalizedString("passwordChangeTitle");
    var selectedIndex = { value: null };

    // If user selects ok, outparam.value is set to the index
    // of the selected username.
    var ok = this._promptService.select(this._window,
                            dialogTitle, dialogText,
                            usernames.length, usernames,
                            selectedIndex);
    if (ok) {
      // Now that we know which login to use, modify its password.
      var selectedLogin = logins[selectedIndex.value];
      this.log("Updating password for user " + selectedLogin.username);
      this._updateLogin(selectedLogin, aNewLogin.password);
    }
  },




  /* ---------- Internal Methods ---------- */




  /*
   * _updateLogin
   */
  _updateLogin : function (login, newPassword) {
    var now = Date.now();
    var propBag = Cc["@mozilla.org/hash-property-bag;1"].
                  createInstance(Ci.nsIWritablePropertyBag);
    if (newPassword) {
      propBag.setProperty("password", newPassword);
      // Explicitly set the password change time here (even though it would
      // be changed automatically), to ensure that it's exactly the same
      // value as timeLastUsed.
      propBag.setProperty("timePasswordChanged", now);
    }
    propBag.setProperty("timeLastUsed", now);
    propBag.setProperty("timesUsedIncrement", 1);
    this._pwmgr.modifyLogin(login, propBag);
  },


  /*
   * _getChromeWindow
   *
   * Given a content DOM window, returns the chrome window it's in.
   */
  _getChromeWindow: function (aWindow) {
    // In e10s, aWindow may already be a chrome window.
    if (aWindow instanceof Ci.nsIDOMChromeWindow)
      return aWindow;
    var chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell)
                           .chromeEventHandler.ownerDocument.defaultView;
    return chromeWin;
  },


  /*
   * _getNotifyWindow
   */
  _getNotifyWindow: function () {

    try {
      // Get topmost window, in case we're in a frame.
      var notifyWin = this._window.top;
      var isE10s = (notifyWin instanceof Ci.nsIDOMChromeWindow);
      var useOpener = false;

      // Some sites pop up a temporary login window, which disappears
      // upon submission of credentials. We want to put the notification
      // bar in the opener window if this seems to be happening.
      if (notifyWin.opener) {
        var chromeDoc = this._getChromeWindow(notifyWin).
                             document.documentElement;

        var hasHistory;
        if (isE10s) {
          if (!this._browser)
            throw new Error("Expected a browser in e10s");
          hasHistory = this._browser.canGoBack;
        } else {
          var webnav = notifyWin.
                       QueryInterface(Ci.nsIInterfaceRequestor).
                       getInterface(Ci.nsIWebNavigation);
          hasHistory = webnav.sessionHistory.count > 1;
        }

        // Check to see if the current window was opened with chrome
        // disabled, and if so use the opener window. But if the window
        // has been used to visit other pages (ie, has a history),
        // assume it'll stick around and *don't* use the opener.
        if (chromeDoc.getAttribute("chromehidden") && !hasHistory) {
          this.log("Using opener window for notification bar.");
          notifyWin = notifyWin.opener;
          useOpener = true;
        }
      }

      let browser;
      if (useOpener && this._opener && isE10s) {
        // In e10s, we have to reconstruct the opener browser from
        // the CPOW passed in the message (and then passed to us in
        // setE10sData).
        // NB: notifyWin is now the chrome window for the opening
        // window.

        browser = notifyWin.gBrowser.getBrowserForContentWindow(this._opener);
      } else if (isE10s) {
        browser = this._browser;
      } else {
        var chromeWin = this._getChromeWindow(notifyWin).wrappedJSObject;
        browser = chromeWin.gBrowser
                           .getBrowserForDocument(notifyWin.top.document);
      }

      return { notifyWin: notifyWin, browser: browser };

    } catch (e) {
      // If any errors happen, just assume no notification box.
      this.log("Unable to get notify window: " + e.fileName + ":" + e.lineNumber + ": " + e.message);
      return null;
    }
  },


  /*
   * _getPopupNote
   *
   * Returns the popup notification to this prompter,
   * or null if there isn't one available.
   */
  _getPopupNote : function () {
    let popupNote = null;

    try {
      let { notifyWin } = this._getNotifyWindow();

      // Get the chrome window for the content window we're using.
      // .wrappedJSObject needed here -- see bug 422974 comment 5.
      let chromeWin = this._getChromeWindow(notifyWin).wrappedJSObject;

      popupNote = chromeWin.PopupNotifications;
    } catch (e) {
      this.log("Popup notifications not available on window");
    }

    return popupNote;
  },


  /*
   * _getNotifyBox
   *
   * Returns the notification box to this prompter, or null if there isn't
   * a notification box available.
   */
  _getNotifyBox : function () {
    let notifyBox = null;

    try {
      let { notifyWin } = this._getNotifyWindow();

      // Get the chrome window for the content window we're using.
      // .wrappedJSObject needed here -- see bug 422974 comment 5.
      let chromeWin = this._getChromeWindow(notifyWin).wrappedJSObject;

      notifyBox = chromeWin.getNotificationBox(notifyWin);
    } catch (e) {
      this.log("Notification bars not available on window");
    }

    return notifyBox;
  },


  /*
   * _repickSelectedLogin
   *
   * The user might enter a login that isn't the one we prefilled, but
   * is the same as some other existing login. So, pick a login with a
   * matching username, or return null.
   */
  _repickSelectedLogin : function (foundLogins, username) {
    for (var i = 0; i < foundLogins.length; i++)
      if (foundLogins[i].username == username)
        return foundLogins[i];
    return null;
  },


  /*
   * _getLocalizedString
   *
   * Can be called as:
   *   _getLocalizedString("key1");
   *   _getLocalizedString("key2", ["arg1"]);
   *   _getLocalizedString("key3", ["arg1", "arg2"]);
   *   (etc)
   *
   * Returns the localized string for the specified key,
   * formatted if required.
   *
   */
  _getLocalizedString : function (key, formatArgs) {
    if (formatArgs)
      return this._strBundle.formatStringFromName(
                                  key, formatArgs, formatArgs.length);
    else
      return this._strBundle.GetStringFromName(key);
  },


  /*
   * _sanitizeUsername
   *
   * Sanitizes the specified username, by stripping quotes and truncating if
   * it's too long. This helps prevent an evil site from messing with the
   * "save password?" prompt too much.
   */
  _sanitizeUsername : function (username) {
    if (username.length > 30) {
      username = username.substring(0, 30);
      username += this._ellipsis;
    }
    return username.replace(/['"]/g, "");
  },


  /*
   * _getFormattedHostname
   *
   * The aURI parameter may either be a string uri, or an nsIURI instance.
   *
   * Returns the hostname to use in a nsILoginInfo object (for example,
   * "http://example.com").
   */
  _getFormattedHostname : function (aURI) {
    var uri;
    if (aURI instanceof Ci.nsIURI) {
      uri = aURI;
    } else {
      uri = Services.io.newURI(aURI, null, null);
    }
    var scheme = uri.scheme;

    var hostname = scheme + "://" + uri.host;

    // If the URI explicitly specified a port, only include it when
    // it's not the default. (We never want "http://foo.com:80")
    var port = uri.port;
    if (port != -1) {
      var handler = Services.io.getProtocolHandler(scheme);
      if (port != handler.defaultPort)
        hostname += ":" + port;
    }

    return hostname;
  },


  /*
   * _getShortDisplayHost
   *
   * Converts a login's hostname field (a URL) to a short string for
   * prompting purposes. Eg, "http://foo.com" --> "foo.com", or
   * "ftp://www.site.co.uk" --> "site.co.uk".
   */
  _getShortDisplayHost: function (aURIString) {
    var displayHost;

    var eTLDService = Cc["@mozilla.org/network/effective-tld-service;1"].
                      getService(Ci.nsIEffectiveTLDService);
    var idnService = Cc["@mozilla.org/network/idn-service;1"].
                     getService(Ci.nsIIDNService);
    try {
      var uri = Services.io.newURI(aURIString, null, null);
      var baseDomain = eTLDService.getBaseDomain(uri);
      displayHost = idnService.convertToDisplayIDN(baseDomain, {});
    } catch (e) {
      this.log("_getShortDisplayHost couldn't process " + aURIString);
    }

    if (!displayHost)
      displayHost = aURIString;

    return displayHost;
  },


  /*
   * _getAuthTarget
   *
   * Returns the hostname and realm for which authentication is being
   * requested, in the format expected to be used with nsILoginInfo.
   */
  _getAuthTarget : function (aChannel, aAuthInfo) {
    var hostname, realm;

    // If our proxy is demanding authentication, don't use the
    // channel's actual destination.
    if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
      this.log("getAuthTarget is for proxy auth");
      if (!(aChannel instanceof Ci.nsIProxiedChannel))
        throw new Error("proxy auth needs nsIProxiedChannel");

      var info = aChannel.proxyInfo;
      if (!info)
        throw new Error("proxy auth needs nsIProxyInfo");

      // Proxies don't have a scheme, but we'll use "moz-proxy://"
      // so that it's more obvious what the login is for.
      var idnService = Cc["@mozilla.org/network/idn-service;1"].
                       getService(Ci.nsIIDNService);
      hostname = "moz-proxy://" +
                  idnService.convertUTF8toACE(info.host) +
                  ":" + info.port;
      realm = aAuthInfo.realm;
      if (!realm)
        realm = hostname;

      return [hostname, realm];
    }

    hostname = this._getFormattedHostname(aChannel.URI);

    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted hostname instead.
    realm = aAuthInfo.realm;
    if (!realm)
      realm = hostname;

    return [hostname, realm];
  },


  /**
   * Returns [username, password] as extracted from aAuthInfo (which
   * holds this info after having prompted the user).
   *
   * If the authentication was for a Windows domain, we'll prepend the
   * return username with the domain. (eg, "domain\user")
   */
  _GetAuthInfo : function (aAuthInfo) {
    var username, password;

    var flags = aAuthInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN && aAuthInfo.domain)
      username = aAuthInfo.domain + "\\" + aAuthInfo.username;
    else
      username = aAuthInfo.username;

    password = aAuthInfo.password;

    return [username, password];
  },


  /**
   * Given a username (possibly in DOMAIN\user form) and password, parses the
   * domain out of the username if necessary and sets domain, username and
   * password on the auth information object.
   */
  _SetAuthInfo : function (aAuthInfo, username, password) {
    var flags = aAuthInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
      // Domain is separated from username by a backslash
      var idx = username.indexOf("\\");
      if (idx == -1) {
        aAuthInfo.username = username;
      } else {
        aAuthInfo.domain   =  username.substring(0, idx);
        aAuthInfo.username =  username.substring(idx+1);
      }
    } else {
      aAuthInfo.username = username;
    }
    aAuthInfo.password = password;
  },

  _newAsyncPromptConsumer : function(aCallback, aContext) {
    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
      callback: aCallback,
      context: aContext,
      cancel: function() {
        this.callback.onAuthCancelled(this.context, false);
        this.callback = null;
        this.context = null;
      }
    };
  }

}; // end of LoginManagerPrompter implementation

XPCOMUtils.defineLazyGetter(this.LoginManagerPrompter.prototype, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerPrompter");
  return logger.log.bind(logger);
});

var component = [LoginManagerPromptFactory, LoginManagerPrompter];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
