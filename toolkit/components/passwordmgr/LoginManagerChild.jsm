/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Module doing most of the content process work for the password manager.
 */

// Disable use-ownerGlobal since LoginForm doesn't have it.
/* eslint-disable mozilla/use-ownerGlobal */

"use strict";

const EXPORTED_SYMBOLS = ["LoginManagerChild"];

const PASSWORD_INPUT_ADDED_COALESCING_THRESHOLD_MS = 1;
const AUTOCOMPLETE_AFTER_RIGHT_CLICK_THRESHOLD_MS = 400;
const AUTOFILL_STATE = "-moz-autofill";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { CreditCard } = ChromeUtils.import(
  "resource://gre/modules/CreditCard.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormLikeFactory",
  "resource://gre/modules/FormLikeFactory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginFormFactory",
  "resource://gre/modules/LoginFormFactory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginRecipesContent",
  "resource://gre/modules/LoginRecipes.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "InsecurePasswordUtils",
  "resource://gre/modules/InsecurePasswordUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ContentDOMReference",
  "resource://gre/modules/ContentDOMReference.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AutoCompleteChild",
  "resource://gre/actors/AutoCompleteChild.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gFormFillService",
  "@mozilla.org/satchel/form-fill-controller;1",
  "nsIFormFillController"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerChild");
  return logger.log.bind(logger);
});

Services.cpmm.addMessageListener("clearRecipeCache", () => {
  LoginRecipesContent._clearRecipeCache();
});

let gLastRightClickTimeStamp = Number.NEGATIVE_INFINITY;

// Input element on which enter keydown event was fired.
let gKeyDownEnterForInput = null;

const observer = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
  ]),

  // nsIWebProgressListener
  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    // Only handle pushState/replaceState here.
    if (
      !(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) ||
      !(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_PUSHSTATE)
    ) {
      return;
    }

    log(
      "onLocationChange handled:",
      aLocation.displaySpec,
      aWebProgress.DOMWindow.document
    );

    LoginManagerChild.forWindow(aWebProgress.DOMWindow)._onNavigation(
      aWebProgress.DOMWindow.document
    );
  },

  onStateChange(aWebProgress, aRequest, aState, aStatus) {
    if (
      aState & Ci.nsIWebProgressListener.STATE_RESTORING &&
      aState & Ci.nsIWebProgressListener.STATE_STOP
    ) {
      // Re-fill a document restored from bfcache since password field values
      // aren't persisted there.
      LoginManagerChild.forWindow(aWebProgress.DOMWindow)._onDocumentRestored(
        aWebProgress.DOMWindow.document
      );
      return;
    }

    if (!(aState & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    // We only care about when a page triggered a load, not the user. For example:
    // clicking refresh/back/forward, typing a URL and hitting enter, and loading a bookmark aren't
    // likely to be when a user wants to save a login.
    let channel = aRequest.QueryInterface(Ci.nsIChannel);
    let triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
    if (
      triggeringPrincipal.isNullPrincipal ||
      triggeringPrincipal.equals(
        Services.scriptSecurityManager.getSystemPrincipal()
      )
    ) {
      return;
    }

    // Don't handle history navigation, reload, or pushState not triggered via chrome UI.
    // e.g. history.go(-1), location.reload(), history.replaceState()
    if (!(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_NORMAL)) {
      log(
        "onStateChange: loadType isn't LOAD_CMD_NORMAL:",
        aWebProgress.loadType
      );
      return;
    }

    log("onStateChange handled:", channel);
    LoginManagerChild.forWindow(aWebProgress.DOMWindow)._onNavigation(
      aWebProgress.DOMWindow.document
    );
  },

  // nsIObserver
  observe(subject, topic, data) {
    switch (topic) {
      case "autocomplete-did-enter-text": {
        let input = subject.QueryInterface(Ci.nsIAutoCompleteInput);
        let { selectedIndex } = input.popup;
        if (selectedIndex < 0) {
          break;
        }

        let { focusedInput } = gFormFillService;
        if (focusedInput.nodePrincipal.isNullPrincipal) {
          // If we have a null principal then prevent any more password manager code from running and
          // incorrectly using the document `location`.
          return;
        }

        let window = focusedInput.ownerGlobal;
        let loginManagerChild = LoginManagerChild.forWindow(window);

        let style = input.controller.getStyleAt(selectedIndex);
        if (style == "login" || style == "loginWithOrigin") {
          let details = JSON.parse(
            input.controller.getCommentAt(selectedIndex)
          );
          loginManagerChild.onFieldAutoComplete(focusedInput, details.guid);
        } else if (style == "generatedPassword") {
          loginManagerChild._highlightFilledField(focusedInput);
          loginManagerChild._generatedPasswordFilledOrEdited(focusedInput);
        }
        break;
      }
    }
  },

  // nsIDOMEventListener
  handleEvent(aEvent) {
    if (!aEvent.isTrusted) {
      return;
    }

    if (!LoginHelper.enabled) {
      return;
    }

    let window = aEvent.target.ownerDocument.defaultView;

    switch (aEvent.type) {
      // Used to mask fields with filled generated passwords when blurred.
      case "blur": {
        let unmask = false;
        LoginManagerChild.forWindow(window)._togglePasswordFieldMasking(
          aEvent.target,
          unmask
        );
        break;
      }

      // Used to watch for changes to fields filled with generated passwords.
      case "change": {
        LoginManagerChild.forWindow(window)._generatedPasswordFilledOrEdited(
          aEvent.target
        );
        break;
      }

      // Used to watch for changes to fields filled with generated passwords.
      case "input": {
        LoginManagerChild.forWindow(
          window
        )._maybeStopTreatingAsGeneratedPasswordField(aEvent);
        break;
      }

      case "keydown": {
        if (
          aEvent.keyCode == aEvent.DOM_VK_TAB ||
          aEvent.keyCode == aEvent.DOM_VK_RETURN
        ) {
          LoginManagerChild.forWindow(window).onUsernameAutocompleted(
            aEvent.target
          );
        }
        break;
      }

      case "focus": {
        if (aEvent.target.type == "password") {
          // Used to unmask fields with filled generated passwords when focused.
          let unmask = true;
          LoginManagerChild.forWindow(window)._togglePasswordFieldMasking(
            aEvent.target,
            unmask
          );
          break;
        }

        // Only used for username fields.
        LoginManagerChild.forWindow(window)._onUsernameFocus(aEvent);
        break;
      }

      case "mousedown": {
        if (aEvent.button == 2) {
          // Date.now() is used instead of event.timeStamp since
          // dom.event.highrestimestamp.enabled isn't true on all channels yet.
          gLastRightClickTimeStamp = Date.now();
        }

        break;
      }

      default: {
        throw new Error("Unexpected event");
      }
    }
  },
};

// Add this observer once for the process.
Services.obs.addObserver(observer, "autocomplete-did-enter-text");

let gAutoCompleteListener = {
  // Input element on which enter keydown event was fired.
  keyDownEnterForInput: null,

  added: false,

  init() {
    if (!this.added) {
      AutoCompleteChild.addPopupStateListener((...args) => {
        this.popupStateListener(...args);
      });
      this.added = true;
    }
  },

  popupStateListener(messageName, data, target) {
    switch (messageName) {
      case "FormAutoComplete:PopupOpened": {
        let { chromeEventHandler } = target.docShell;
        chromeEventHandler.addEventListener("keydown", this, true);
        break;
      }

      case "FormAutoComplete:PopupClosed": {
        this.onPopupClosed(data.selectedRowStyle, target);
        let { chromeEventHandler } = target.docShell;
        chromeEventHandler.removeEventListener("keydown", this, true);
        break;
      }
    }
  },

  handleEvent(event) {
    if (event.type != "keydown") {
      return;
    }

    let focusedElement = gFormFillService.focusedInput;
    if (
      event.keyCode != event.DOM_VK_RETURN ||
      focusedElement != event.target
    ) {
      this.keyDownEnterForInput = null;
      return;
    }
    this.keyDownEnterForInput = focusedElement;
  },

  onPopupClosed(selectedRowStyle, window) {
    let focusedElement = gFormFillService.focusedInput;
    let eventTarget = this.keyDownEnterForInput;
    if (
      !eventTarget ||
      eventTarget !== focusedElement ||
      selectedRowStyle != "loginsFooter"
    ) {
      this.keyDownEnterForInput = null;
      return;
    }

    let loginManager = window.getWindowGlobalChild().getActor("LoginManager");
    let hostname = eventTarget.ownerDocument.documentURIObject.host;
    loginManager.sendAsyncMessage("PasswordManager:OpenPreferences", {
      hostname,
      entryPoint: "autocomplete",
    });
  },
};

this.LoginManagerChild = class LoginManagerChild extends JSWindowActorChild {
  constructor() {
    super();

    /**
     * WeakMap of the root element of a LoginForm to the DeferredTask to fill its fields.
     *
     * This is used to be able to throttle fills for a LoginForm since onDOMInputPasswordAdded gets
     * dispatched for each password field added to a document but we only want to fill once per
     * LoginForm when multiple fields are added at once.
     *
     * @type {WeakMap}
     */
    this._deferredPasswordAddedTasksByRootElement = new WeakMap();

    /**
     * WeakMap of a document to the array of callbacks to execute when it becomes visible
     *
     * This is used to defer handling DOMFormHasPassword and onDOMInputPasswordAdded events when the
     * containing document is hidden.
     * When the document first becomes visible, any queued events will be handled as normal.
     *
     * @type {WeakMap}
     */
    this._visibleTasksByDocument = new WeakMap();

    /**
     * Maps all DOM content documents in this content process, including those in
     * frames, to the current state used by the Login Manager.
     */
    this._loginFormStateByDocument = new WeakMap();
  }

  static forWindow(window) {
    let windowGlobal = window.getWindowGlobalChild();
    if (windowGlobal) {
      return windowGlobal.getActor("LoginManager");
    }

    return null;
  }

  _compareAndUpdatePreviouslySentValues(
    formLikeRoot,
    usernameValue,
    passwordValue,
    dismissed = false
  ) {
    let state = this.stateForDocument(formLikeRoot.ownerDocument);
    const lastSentValues = state.lastSubmittedValuesByRootElement.get(
      formLikeRoot
    );
    if (lastSentValues) {
      if (dismissed && !lastSentValues.dismissed) {
        // preserve previous dismissed value if it was false (i.e. shown/open)
        dismissed = false;
      }
      if (
        lastSentValues.username == usernameValue &&
        lastSentValues.password == passwordValue &&
        lastSentValues.dismissed == dismissed
      ) {
        log(
          "_compareAndUpdatePreviouslySentValues: values are equivalent, returning true"
        );
        return true;
      }
    }

    // Save the last submitted values so we don't prompt twice for the same values using
    // different capture methods e.g. a form submit event and upon navigation.
    state.lastSubmittedValuesByRootElement.set(formLikeRoot, {
      username: usernameValue,
      password: passwordValue,
      dismissed,
    });
    log(
      "_compareAndUpdatePreviouslySentValues: values not equivalent, returning false"
    );
    return false;
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "PasswordManager:fillForm": {
        this.fillForm({
          loginFormOrigin: msg.data.loginFormOrigin,
          loginsFound: LoginHelper.vanillaObjectsToLogins(msg.data.logins),
          recipes: msg.data.recipes,
          inputElementIdentifier: msg.data.inputElementIdentifier,
          originMatches: msg.data.originMatches,
        });
        break;
      }

      case "PasswordManager:fillGeneratedPassword": {
        // make a login for the password
        let generatedLogin = Cc[
          "@mozilla.org/login-manager/loginInfo;1"
        ].createInstance(Ci.nsILoginInfo);
        generatedLogin.init(
          msg.data.origin,
          "", // empty formActionOrigin
          null, // no realm
          "", // empty username
          msg.data.password
        );
        this.fillForm({
          loginFormOrigin: msg.data.origin,
          loginsFound: [generatedLogin],
          recipes: msg.data.recipes,
          inputElementIdentifier: msg.data.inputElementIdentifier,
          originMatches: msg.data.originMatches,
        });
        let inputElement = ContentDOMReference.resolve(
          msg.data.inputElementIdentifier
        );
        if (inputElement) {
          this._generatedPasswordFilledOrEdited(inputElement);
        } else {
          log("Could not resolve inputElementIdentifier to a living element.");
        }
        break;
      }

      case "PasswordManager:formIsPending": {
        return this._visibleTasksByDocument.has(this.document);
      }

      case "PasswordManager:formProcessed": {
        this.notifyObserversOfFormProcessed(msg.data.formid);
        break;
      }
    }

    return undefined;
  }

  shouldIgnoreLoginManagerEvent(event) {
    let nodePrincipal = event.target.nodePrincipal;
    // If we have a system or null principal then prevent any more password manager code from running and
    // incorrectly using the document `location`. Also skip password manager for about: pages.
    return (
      nodePrincipal.isSystemPrincipal ||
      nodePrincipal.isNullPrincipal ||
      nodePrincipal.schemeIs("about")
    );
  }

  handleEvent(event) {
    if (
      AppConstants.platform == "android" &&
      Services.prefs.getBoolPref("reftest.remote", false)
    ) {
      // XXX known incompatibility between reftest harness and form-fill. Is this still needed?
      return;
    }

    switch (event.type) {
      case "DOMFormBeforeSubmit": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }

        this.onDOMFormBeforeSubmit(event);
        break;
      }
      case "DOMFormHasPassword": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }

        this.onDOMFormHasPassword(event);
        let formLike = LoginFormFactory.createFromForm(event.originalTarget);
        InsecurePasswordUtils.reportInsecurePasswords(formLike);
        break;
      }
      case "DOMInputPasswordAdded": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }

        this.onDOMInputPasswordAdded(event, this.document.defaultView);
        let formLike = LoginFormFactory.createFromField(event.originalTarget);
        InsecurePasswordUtils.reportInsecurePasswords(formLike);
        break;
      }
    }
  }

  notifyObserversOfFormProcessed(formid) {
    Services.obs.notifyObservers(this, "passwordmgr-processed-form", formid);
  }

  /**
   * Get relevant logins and recipes from the parent
   *
   * @param {HTMLFormElement} form - form to get login data for
   * @param {Object} options
   * @param {boolean} options.guid - guid of a login to retrieve
   * @param {boolean} options.showMasterPassword - whether to show a master password prompt
   */
  _getLoginDataFromParent(form, options) {
    let doc = form.ownerDocument;

    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    if (!formOrigin) {
      throw new Error("_getLoginDataFromParent: A form origin is required");
    }
    let actionOrigin = LoginHelper.getFormActionOrigin(form);

    let messageData = { formOrigin, actionOrigin, options };

    let resultPromise = this.sendQuery(
      "PasswordManager:findLogins",
      messageData
    );
    return resultPromise.then(result => {
      return {
        form,
        loginsFound: LoginHelper.vanillaObjectsToLogins(result.logins),
        recipes: result.recipes,
      };
    });
  }

  _autoCompleteSearchAsync(aSearchString, aPreviousResult, aElement) {
    let doc = aElement.ownerDocument;
    let form = LoginFormFactory.createFromField(aElement);

    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    let actionOrigin = LoginHelper.getFormActionOrigin(form);
    let autocompleteInfo = aElement.getAutocompleteInfo();

    let previousResult = aPreviousResult
      ? {
          searchString: aPreviousResult.searchString,
          logins: LoginHelper.loginsToVanillaObjects(aPreviousResult.logins),
        }
      : null;

    let messageData = {
      autocompleteInfo,
      formOrigin,
      actionOrigin,
      searchString: aSearchString,
      previousResult,
      isSecure: InsecurePasswordUtils.isFormSecure(form),
      isPasswordField: aElement.type == "password",
    };

    if (LoginHelper.showAutoCompleteFooter) {
      gAutoCompleteListener.init();
    }

    let resultPromise = this.sendQuery(
      "PasswordManager:autoCompleteLogins",
      messageData
    );

    return resultPromise.then(result => {
      return {
        generatedPassword: result.generatedPassword,
        logins: LoginHelper.vanillaObjectsToLogins(result.logins),
      };
    });
  }

  setupProgressListener(window) {
    if (!LoginHelper.formlessCaptureEnabled) {
      return;
    }

    // Get the highest accessible docshell and attach the progress listener to that.
    let browsingContext = window.getWindowGlobalChild().browsingContext;
    let docShell;
    while (browsingContext && browsingContext.docShell) {
      docShell = browsingContext.docShell;
      browsingContext = browsingContext.parent;
    }

    try {
      let webProgress = docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebProgress);
      webProgress.addProgressListener(
        observer,
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT |
          Ci.nsIWebProgress.NOTIFY_LOCATION
      );
    } catch (ex) {
      // Ignore NS_ERROR_FAILURE if the progress listener was already added
    }
  }

  onDOMFormBeforeSubmit(event) {
    if (!event.isTrusted) {
      return;
    }

    // We're invoked before the content's |submit| event handlers, so we
    // can grab form data before it might be modified (see bug 257781).
    log("notified before form submission");
    let formLike = LoginFormFactory.createFromForm(event.target);
    this._onFormSubmit(formLike);
  }

  onDocumentVisibilityChange(event) {
    if (!event.isTrusted) {
      return;
    }
    let document = event.target;
    let onVisibleTasks = this._visibleTasksByDocument.get(document);
    if (!onVisibleTasks) {
      return;
    }
    for (let task of onVisibleTasks) {
      log("onDocumentVisibilityChange, executing queued task");
      task();
    }
    this._visibleTasksByDocument.delete(document);
  }

  _deferHandlingEventUntilDocumentVisible(event, document, fn) {
    log(
      `document.visibilityState: ${document.visibilityState}, defer handling ${
        event.type
      }`
    );
    let onVisibleTasks = this._visibleTasksByDocument.get(document);
    if (!onVisibleTasks) {
      log(
        `deferHandling, first queued event, register the visibilitychange handler`
      );
      onVisibleTasks = [];
      this._visibleTasksByDocument.set(document, onVisibleTasks);
      document.addEventListener(
        "visibilitychange",
        event => {
          this.onDocumentVisibilityChange(event);
        },
        { once: true }
      );
    }
    onVisibleTasks.push(fn);
  }

  onDOMFormHasPassword(event) {
    if (!event.isTrusted) {
      return;
    }
    let isMasterPasswordSet = Services.cpmm.sharedData.get(
      "isMasterPasswordSet"
    );
    let document = event.target.ownerDocument;

    // don't attempt to defer handling when a master password is set
    // Showing the MP modal as soon as possible minimizes its interference with tab interactions
    // See bug 1539091 and bug 1538460.
    log(
      "onDOMFormHasPassword, visibilityState:",
      document.visibilityState,
      "isMasterPasswordSet:",
      isMasterPasswordSet
    );
    if (document.visibilityState == "visible" || isMasterPasswordSet) {
      this._processDOMFormHasPasswordEvent(event);
    } else {
      // wait until the document becomes visible before handling this event
      this._deferHandlingEventUntilDocumentVisible(event, document, () => {
        this._processDOMFormHasPasswordEvent(event);
      });
    }
  }

  _processDOMFormHasPasswordEvent(event) {
    let form = event.target;
    let formLike = LoginFormFactory.createFromForm(form);
    log("_processDOMFormHasPasswordEvent:", form, formLike);
    this._fetchLoginsFromParentAndFillForm(formLike);
  }

  onDOMInputPasswordAdded(event, window) {
    if (!event.isTrusted) {
      return;
    }

    this.setupProgressListener(window);

    let pwField = event.originalTarget;
    if (pwField.form) {
      // Fill is handled by onDOMFormHasPassword which is already throttled.
      return;
    }

    let document = pwField.ownerDocument;
    let isMasterPasswordSet = Services.cpmm.sharedData.get(
      "isMasterPasswordSet"
    );
    log(
      "onDOMInputPasswordAdded, visibilityState:",
      document.visibilityState,
      "isMasterPasswordSet:",
      isMasterPasswordSet
    );

    // don't attempt to defer handling when a master password is set
    // Showing the MP modal as soon as possible minimizes its interference with tab interactions
    // See bug 1539091 and bug 1538460.
    if (document.visibilityState == "visible" || isMasterPasswordSet) {
      this._processDOMInputPasswordAddedEvent(event);
    } else {
      // wait until the document becomes visible before handling this event
      this._deferHandlingEventUntilDocumentVisible(event, document, () => {
        this._processDOMInputPasswordAddedEvent(event);
      });
    }
  }

  _processDOMInputPasswordAddedEvent(event) {
    let pwField = event.originalTarget;

    let formLike = LoginFormFactory.createFromField(pwField);
    log(" _processDOMInputPasswordAddedEvent:", pwField, formLike);

    let deferredTask = this._deferredPasswordAddedTasksByRootElement.get(
      formLike.rootElement
    );
    if (!deferredTask) {
      log(
        "Creating a DeferredTask to call _fetchLoginsFromParentAndFillForm soon"
      );
      LoginFormFactory.setForRootElement(formLike.rootElement, formLike);

      deferredTask = new DeferredTask(
        () => {
          // Get the updated LoginForm instead of the one at the time of creating the DeferredTask via
          // a closure since it could be stale since LoginForm.elements isn't live.
          let formLike2 = LoginFormFactory.getForRootElement(
            formLike.rootElement
          );
          log(
            "Running deferred processing of onDOMInputPasswordAdded",
            formLike2
          );
          this._deferredPasswordAddedTasksByRootElement.delete(
            formLike2.rootElement
          );
          this._fetchLoginsFromParentAndFillForm(formLike2);
        },
        PASSWORD_INPUT_ADDED_COALESCING_THRESHOLD_MS,
        0
      );

      this._deferredPasswordAddedTasksByRootElement.set(
        formLike.rootElement,
        deferredTask
      );
    }

    let window = pwField.ownerGlobal;
    if (deferredTask.isArmed) {
      log("DeferredTask is already armed so just updating the LoginForm");
      // We update the LoginForm so it (most important .elements) is fresh when the task eventually
      // runs since changes to the elements could affect our field heuristics.
      LoginFormFactory.setForRootElement(formLike.rootElement, formLike);
    } else if (window.document.readyState == "complete") {
      log(
        "Arming the DeferredTask we just created since document.readyState == 'complete'"
      );
      deferredTask.arm();
    } else {
      window.addEventListener(
        "DOMContentLoaded",
        function() {
          log(
            "Arming the onDOMInputPasswordAdded DeferredTask due to DOMContentLoaded"
          );
          deferredTask.arm();
        },
        { once: true }
      );
    }
  }

  /**
   * Fetch logins from the parent for a given form and then attempt to fill it.
   *
   * @param {LoginForm} form to fetch the logins for then try autofill.
   */
  _fetchLoginsFromParentAndFillForm(form) {
    if (!LoginHelper.enabled) {
      return;
    }

    this._getLoginDataFromParent(form, { showMasterPassword: true })
      .then(this.loginsFound.bind(this))
      .catch(Cu.reportError);
  }

  /**
   * Retrieves a reference to the state object associated with the given
   * document. This is initialized to an object with default values.
   */
  stateForDocument(document) {
    let loginFormState = this._loginFormStateByDocument.get(document);
    if (!loginFormState) {
      loginFormState = {
        /**
         * Keeps track of filled fields and values.
         */
        fillsByRootElement: new WeakMap(),
        /**
         * Keeps track of logins that were last submitted.
         */
        lastSubmittedValuesByRootElement: new WeakMap(),
        loginFormRootElements: new WeakSet(),
      };
      this._loginFormStateByDocument.set(document, loginFormState);
    }
    return loginFormState;
  }

  /**
   * Perform a password fill upon user request coming from the parent process.
   * The fill will be in the form previously identified during page navigation.
   *
   * @param An object with the following properties:
   *        {
   *          loginFormOrigin:
   *            String with the origin for which the login UI was displayed.
   *            This must match the origin of the form used for the fill.
   *          loginsFound:
   *            Array containing the login to fill. While other messages may
   *            have more logins, for this use case this is expected to have
   *            exactly one element. The origin of the login may be different
   *            from the origin of the form used for the fill.
   *          recipes:
   *            Fill recipes transmitted together with the original message.
   *          inputElementIdentifier:
   *            An identifier generated for the input element via ContentDOMReference.
   *          originMatches:
   *            True if the origin of the form matches the page URI.
   *        }
   */
  fillForm({
    loginFormOrigin,
    loginsFound,
    recipes,
    inputElementIdentifier,
    originMatches,
  }) {
    if (!inputElementIdentifier) {
      log("fillForm: No input element specified");
      return;
    }

    let inputElement = ContentDOMReference.resolve(inputElementIdentifier);
    if (!inputElement) {
      log(
        "fillForm: Could not resolve inputElementIdentifier to a living element."
      );
      return;
    }

    if (!originMatches) {
      if (
        !inputElement ||
        LoginHelper.getLoginOrigin(inputElement.ownerDocument.documentURI) !=
          loginFormOrigin
      ) {
        log(
          "fillForm: The requested origin doesn't match the one from the",
          "document. This may mean we navigated to a document from a different",
          "site before we had a chance to indicate this change in the user",
          "interface."
        );
        return;
      }
    }

    let clobberUsername = true;
    let form = LoginFormFactory.createFromField(inputElement);
    if (inputElement.type == "password") {
      clobberUsername = false;
    }

    this._fillForm(form, loginsFound, recipes, {
      inputElement,
      autofillForm: true,
      clobberUsername,
      clobberPassword: true,
      userTriggered: true,
    });
  }

  loginsFound({ form, loginsFound, recipes }) {
    let doc = form.ownerDocument;
    let autofillForm =
      LoginHelper.autofillForms &&
      !PrivateBrowsingUtils.isContentWindowPrivate(doc.defaultView);

    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    LoginRecipesContent.cacheRecipes(formOrigin, doc.defaultView, recipes);

    this._fillForm(form, loginsFound, recipes, { autofillForm });
  }

  /**
   * Focus event handler for username fields to decide whether to show autocomplete.
   * @param {FocusEvent} event
   */
  _onUsernameFocus(event) {
    let focusedField = event.target;
    if (!focusedField.mozIsTextField(true) || focusedField.readOnly) {
      return;
    }

    if (this._isLoginAlreadyFilled(focusedField)) {
      log("_onUsernameFocus: Already filled");
      return;
    }

    /*
     * A `mousedown` event is fired before the `focus` event if the user right clicks into an
     * unfocused field. In that case we don't want to show both autocomplete and a context menu
     * overlapping so we check against the timestamp that was set by the `mousedown` event if the
     * button code indicated a right click.
     * We use a timestamp instead of a bool to avoid complexity when dealing with multiple input
     * forms and the fact that a mousedown into an already focused field does not trigger another focus.
     * Date.now() is used instead of event.timeStamp since dom.event.highrestimestamp.enabled isn't
     * true on all channels yet.
     */
    let timeDiff = Date.now() - gLastRightClickTimeStamp;
    if (timeDiff < AUTOCOMPLETE_AFTER_RIGHT_CLICK_THRESHOLD_MS) {
      log(
        "Not opening autocomplete after focus since a context menu was opened within",
        timeDiff,
        "ms"
      );
      return;
    }

    log("maybeOpenAutocompleteAfterFocus: Opening the autocomplete popup");
    gFormFillService.showPopup();
  }

  /**
   * A username or password was autocompleted into a field.
   */
  onFieldAutoComplete(acInputField, loginGUID) {
    if (!LoginHelper.enabled) {
      return;
    }

    // This is probably a bit over-conservatative.
    if (
      ChromeUtils.getClassName(acInputField.ownerDocument) != "HTMLDocument"
    ) {
      return;
    }

    if (!LoginFormFactory.createFromField(acInputField)) {
      return;
    }

    if (LoginHelper.isUsernameFieldType(acInputField)) {
      this.onUsernameAutocompleted(acInputField, loginGUID);
    } else if (acInputField.hasBeenTypePassword) {
      // Ensure the field gets re-masked and edits don't overwrite the generated
      // password in case a generated password was filled into it previously.
      this._stopTreatingAsGeneratedPasswordField(acInputField);
      this._highlightFilledField(acInputField);
    }
  }

  /**
   * A username field was filled or tabbed away from so try fill in the
   * associated password in the password field.
   */
  onUsernameAutocompleted(acInputField, loginGUID = null) {
    log("onUsernameAutocompleted:", acInputField);

    let acForm = LoginFormFactory.createFromField(acInputField);
    let doc = acForm.ownerDocument;
    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    let recipes = LoginRecipesContent.getRecipes(
      this,
      formOrigin,
      doc.defaultView
    );

    // Make sure the username field fillForm will use is the
    // same field as the autocomplete was activated on.
    let [usernameField, passwordField, ignored] = this._getFormFields(
      acForm,
      false,
      recipes
    );
    if (usernameField == acInputField && passwordField) {
      this._getLoginDataFromParent(acForm, {
        guid: loginGUID,
        showMasterPassword: false,
      })
        .then(({ form, loginsFound, recipes }) => {
          if (!loginGUID) {
            // not an explicit autocomplete menu selection, filter for exact matches only
            loginsFound = this._filterForExactFormOriginLogins(
              loginsFound,
              acForm
            );
            // filter the list for exact matches with the username
            // NOTE: this could be an empty string which is a valid username
            let searchString = usernameField.value.toLowerCase();
            loginsFound = loginsFound.filter(
              l => l.username.toLowerCase() == searchString
            );
          }

          this._fillForm(form, loginsFound, recipes, {
            autofillForm: true,
            clobberPassword: true,
            userTriggered: true,
          });
        })
        .catch(Cu.reportError);
    } else {
      // Ignore the event, it's for some input we don't care about.
    }
  }

  /**
   * @param {LoginForm} form - the LoginForm to look for password fields in.
   * @param {Object} options
   * @param {bool} [options.skipEmptyFields=false] - Whether to ignore password fields with no value.
   *                                                 Used at capture time since saving empty values isn't
   *                                                 useful.
   * @param {Object} [options.fieldOverrideRecipe=null] - A relevant field override recipe to use.
   * @return {Array|null} Array of password field elements for the specified form.
   *                      If no pw fields are found, or if more than 3 are found, then null
   *                      is returned.
   */
  _getPasswordFields(
    form,
    { fieldOverrideRecipe = null, minPasswordLength = 0 } = {}
  ) {
    // Locate the password fields in the form.
    let pwFields = [];
    for (let i = 0; i < form.elements.length; i++) {
      let element = form.elements[i];
      if (
        ChromeUtils.getClassName(element) !== "HTMLInputElement" ||
        element.type != "password" ||
        !element.isConnected
      ) {
        continue;
      }

      // Exclude ones matching a `notPasswordSelector`, if specified.
      if (
        fieldOverrideRecipe &&
        fieldOverrideRecipe.notPasswordSelector &&
        element.matches(fieldOverrideRecipe.notPasswordSelector)
      ) {
        log(
          "skipping password field (id/name is",
          element.id,
          " / ",
          element.name + ") due to recipe:",
          fieldOverrideRecipe
        );
        continue;
      }

      // XXX: Bug 780449 tracks our handling of emoji and multi-code-point characters in
      // password fields. To avoid surprises, we should be consistent with the visual
      // representation of the masked password
      if (
        minPasswordLength &&
        element.value.trim().length < minPasswordLength
      ) {
        log(
          "skipping password field (id/name is",
          element.id,
          " / ",
          element.name + ") as value is too short:",
          element.value.trim().length
        );
        continue; // Ignore empty or too-short passwords fields
      }

      pwFields[pwFields.length] = {
        index: i,
        element,
      };
    }

    // If too few or too many fields, bail out.
    if (!pwFields.length) {
      log("(form ignored -- no password fields.)");
      return null;
    } else if (pwFields.length > 3) {
      log(
        "(form ignored -- too many password fields. [ got ",
        pwFields.length,
        "])"
      );
      return null;
    }

    return pwFields;
  }

  /**
   * Returns the username and password fields found in the form.
   * Can handle complex forms by trying to figure out what the
   * relevant fields are.
   *
   * @param {LoginForm} form
   * @param {bool} isSubmission
   * @param {Set} recipes
   * @return {Array} [usernameField, newPasswordField, oldPasswordField]
   *
   * usernameField may be null.
   * newPasswordField will always be non-null.
   * oldPasswordField may be null. If null, newPasswordField is just
   * "theLoginField". If not null, the form is apparently a
   * change-password field, with oldPasswordField containing the password
   * that is being changed.
   *
   * Note that even though we can create a LoginForm from a text field,
   * this method will only return a non-null usernameField if the
   * LoginForm has a password field.
   */
  _getFormFields(form, isSubmission, recipes) {
    let usernameField = null;
    let pwFields = null;
    let fieldOverrideRecipe = LoginRecipesContent.getFieldOverrides(
      recipes,
      form
    );
    if (fieldOverrideRecipe) {
      let pwOverrideField = LoginRecipesContent.queryLoginField(
        form,
        fieldOverrideRecipe.passwordSelector
      );
      if (pwOverrideField) {
        // The field from the password override may be in a different LoginForm.
        let formLike = LoginFormFactory.createFromField(pwOverrideField);
        pwFields = [
          {
            index: [...formLike.elements].indexOf(pwOverrideField),
            element: pwOverrideField,
          },
        ];
      }

      let usernameOverrideField = LoginRecipesContent.queryLoginField(
        form,
        fieldOverrideRecipe.usernameSelector
      );
      if (usernameOverrideField) {
        usernameField = usernameOverrideField;
      }
    }

    if (!pwFields) {
      // Locate the password field(s) in the form. Up to 3 supported.
      // If there's no password field, there's nothing for us to do.
      const minSubmitPasswordLength = 2;
      pwFields = this._getPasswordFields(form, {
        fieldOverrideRecipe,
        minPasswordLength: isSubmission ? minSubmitPasswordLength : 0,
      });
    }

    if (!pwFields) {
      return [null, null, null];
    }

    if (!usernameField) {
      // Locate the username field in the form by searching backwards
      // from the first password field, assume the first text field is the
      // username. We might not find a username field if the user is
      // already logged in to the site.

      for (let i = pwFields[0].index - 1; i >= 0; i--) {
        let element = form.elements[i];
        if (!LoginHelper.isUsernameFieldType(element)) {
          continue;
        }

        if (
          fieldOverrideRecipe &&
          fieldOverrideRecipe.notUsernameSelector &&
          element.matches(fieldOverrideRecipe.notUsernameSelector)
        ) {
          continue;
        }

        usernameField = element;
        break;
      }
    }

    if (!usernameField) {
      log("(form -- no username field found)");
    } else {
      let acFieldName = usernameField.getAutocompleteInfo().fieldName;
      log(
        "Username field ",
        usernameField,
        "has name/value/autocomplete:",
        usernameField.name,
        "/",
        usernameField.value,
        "/",
        acFieldName
      );
    }
    // If we're not submitting a form (it's a page load), there are no
    // password field values for us to use for identifying fields. So,
    // just assume the first password field is the one to be filled in.
    if (!isSubmission || pwFields.length == 1) {
      let passwordField = pwFields[0].element;
      log("Password field", passwordField, "has name: ", passwordField.name);
      return [usernameField, passwordField, null];
    }

    // Try to figure out what is in the form based on the password values.
    let oldPasswordField, newPasswordField;
    let pw1 = pwFields[0].element.value;
    let pw2 = pwFields[1].element.value;
    let pw3 = pwFields[2] ? pwFields[2].element.value : null;

    if (pwFields.length == 3) {
      // Look for two identical passwords, that's the new password

      if (pw1 == pw2 && pw2 == pw3) {
        // All 3 passwords the same? Weird! Treat as if 1 pw field.
        newPasswordField = pwFields[0].element;
        oldPasswordField = null;
      } else if (pw1 == pw2) {
        newPasswordField = pwFields[0].element;
        oldPasswordField = pwFields[2].element;
      } else if (pw2 == pw3) {
        oldPasswordField = pwFields[0].element;
        newPasswordField = pwFields[2].element;
      } else if (pw1 == pw3) {
        // A bit odd, but could make sense with the right page layout.
        newPasswordField = pwFields[0].element;
        oldPasswordField = pwFields[1].element;
      } else {
        // We can't tell which of the 3 passwords should be saved.
        log("(form ignored -- all 3 pw fields differ)");
        return [null, null, null];
      }
    } else if (pw1 == pw2) {
      // pwFields.length == 2
      // Treat as if 1 pw field
      newPasswordField = pwFields[0].element;
      oldPasswordField = null;
    } else {
      // Just assume that the 2nd password is the new password
      oldPasswordField = pwFields[0].element;
      newPasswordField = pwFields[1].element;
    }

    log(
      "Password field (new) id/name is: ",
      newPasswordField.id,
      " / ",
      newPasswordField.name
    );
    if (oldPasswordField) {
      log(
        "Password field (old) id/name is: ",
        oldPasswordField.id,
        " / ",
        oldPasswordField.name
      );
    } else {
      log("Password field (old):", oldPasswordField);
    }
    return [usernameField, newPasswordField, oldPasswordField];
  }

  /**
   * @return true if the page requests autocomplete be disabled for the
   *              specified element.
   */
  _isAutocompleteDisabled(element) {
    return element && element.autocomplete == "off";
  }

  /**
   * Fill a page that was restored from bfcache since we wouldn't receive
   * DOMInputPasswordAdded or DOMFormHasPassword events for it.
   * @param {Document} aDocument that was restored from bfcache.
   */
  _onDocumentRestored(aDocument) {
    let rootElsWeakSet = LoginFormFactory.getRootElementsWeakSetForDocument(
      aDocument
    );
    let weakLoginFormRootElements = ChromeUtils.nondeterministicGetWeakSetKeys(
      rootElsWeakSet
    );

    log(
      "_onDocumentRestored: loginFormRootElements approx size:",
      weakLoginFormRootElements.length,
      "document:",
      aDocument
    );

    for (let formRoot of weakLoginFormRootElements) {
      if (!formRoot.isConnected) {
        continue;
      }

      let formLike = LoginFormFactory.getForRootElement(formRoot);
      this._fetchLoginsFromParentAndFillForm(formLike);
    }
  }

  /**
   * Trigger capture on any relevant FormLikes due to a navigation alone (not
   * necessarily due to an actual form submission). This method is used to
   * capture logins for cases where form submit events are not used.
   *
   * To avoid multiple notifications for the same LoginForm, this currently
   * avoids capturing when dealing with a real <form> which are ideally already
   * using a submit event.
   *
   * @param {Document} document being navigated
   */
  _onNavigation(aDocument) {
    let rootElsWeakSet = LoginFormFactory.getRootElementsWeakSetForDocument(
      aDocument
    );
    let weakLoginFormRootElements = ChromeUtils.nondeterministicGetWeakSetKeys(
      rootElsWeakSet
    );

    log(
      "_onNavigation: root elements approx size:",
      weakLoginFormRootElements.length,
      "document:",
      aDocument
    );

    for (let formRoot of weakLoginFormRootElements) {
      if (!formRoot.isConnected) {
        continue;
      }

      let formLike = LoginFormFactory.getForRootElement(formRoot);
      this._onFormSubmit(formLike);
    }
  }

  /**
   * Called by our observer when notified of a form submission.
   * [Note that this happens before any DOM onsubmit handlers are invoked.]
   * Looks for a password change in the submitted form, so we can update
   * our stored password.
   *
   * @param {LoginForm} form
   */
  _onFormSubmit(form) {
    log("_onFormSubmit", form);
    let doc = form.ownerDocument;
    let win = doc.defaultView;

    if (
      PrivateBrowsingUtils.isContentWindowPrivate(win) &&
      !LoginHelper.privateBrowsingCaptureEnabled
    ) {
      // We won't do anything in private browsing mode anyway,
      // so there's no need to perform further checks.
      log("(form submission ignored in private browsing mode)");
      return;
    }

    // If password saving is disabled globally, bail out now.
    if (!LoginHelper.enabled) {
      return;
    }

    let origin = LoginHelper.getLoginOrigin(doc.documentURI);
    if (!origin) {
      log("(form submission ignored -- invalid origin)");
      return;
    }

    let formActionOrigin = LoginHelper.getFormActionOrigin(form);

    let recipes = LoginRecipesContent.getRecipes(this, origin, win);

    // Get the appropriate fields from the form.
    let [
      usernameField,
      newPasswordField,
      oldPasswordField,
    ] = this._getFormFields(form, true, recipes);

    // Need at least 1 valid password field to do anything.
    if (newPasswordField == null) {
      return;
    }

    if (usernameField && usernameField.value.match(/[•\*]{3,}/)) {
      log(
        `usernameField.value "${
          usernameField.value
        }" looks munged, setting to null`
      );
      usernameField = null;
    }

    // Check for autocomplete=off attribute. We don't use it to prevent
    // autofilling (for existing logins), but won't save logins when it's
    // present and the storeWhenAutocompleteOff pref is false.
    // XXX spin out a bug that we don't update timeLastUsed in this case?
    if (
      (this._isAutocompleteDisabled(form) ||
        this._isAutocompleteDisabled(usernameField) ||
        this._isAutocompleteDisabled(newPasswordField) ||
        this._isAutocompleteDisabled(oldPasswordField)) &&
      !LoginHelper.storeWhenAutocompleteOff
    ) {
      log("(form submission ignored -- autocomplete=off found)");
      return;
    }

    // Don't try to send DOM nodes over IPC.
    let mockUsername = usernameField
      ? { name: usernameField.name, value: usernameField.value }
      : null;
    let mockPassword = {
      name: newPasswordField.name,
      value: newPasswordField.value,
    };
    let mockOldPassword = oldPasswordField
      ? { name: oldPasswordField.name, value: oldPasswordField.value }
      : null;

    let usernameValue = usernameField ? usernameField.value : null;
    let formLikeRoot = FormLikeFactory.findRootForField(newPasswordField);
    // Dismiss prompt if the username field is a credit card number AND
    // if the password field is a three digit number. Also dismiss prompt if
    // the password is a credit card number and the password field has attribute
    // autocomplete="cc-number".
    let dismissedPrompt = false;
    let newPasswordFieldValue = newPasswordField.value;
    if (
      (CreditCard.isValidNumber(usernameValue) &&
        newPasswordFieldValue.trim().match(/^[0-9]{3}$/)) ||
      (CreditCard.isValidNumber(newPasswordFieldValue) &&
        newPasswordField.getAutocompleteInfo().fieldName == "cc-number")
    ) {
      dismissedPrompt = true;
    }

    if (
      this._compareAndUpdatePreviouslySentValues(
        formLikeRoot,
        usernameValue,
        newPasswordField.value,
        dismissedPrompt
      )
    ) {
      log(
        "(form submission ignored -- already submitted with the same username and password)"
      );
      return;
    }

    let autoFilledLogin = this.stateForDocument(doc).fillsByRootElement.get(
      form.rootElement
    );
    this.sendAsyncMessage("PasswordManager:onFormSubmit", {
      origin,
      formActionOrigin,
      autoFilledLoginGuid: autoFilledLogin && autoFilledLogin.guid,
      usernameField: mockUsername,
      newPasswordField: mockPassword,
      oldPasswordField: mockOldPassword,
      dismissedPrompt,
    });
  }

  _maybeStopTreatingAsGeneratedPasswordField(event) {
    let passwordField = event.target;
    let { value } = passwordField;

    // If the field is now empty or the inserted text replaced the whole value
    // then stop treating it as a generated password field.
    if (!value || (event.data && event.data == value)) {
      this._stopTreatingAsGeneratedPasswordField(passwordField);
    }
  }

  _stopTreatingAsGeneratedPasswordField(passwordField) {
    log("_stopTreatingAsGeneratedPasswordField");

    // Remove all the event listeners added in _generatedPasswordFilledOrEdited
    for (let eventType of ["blur", "change", "focus", "input"]) {
      passwordField.removeEventListener(eventType, observer, {
        capture: true,
        mozSystemGroup: true,
      });
    }

    // Mask the password field
    this._togglePasswordFieldMasking(passwordField, false);
  }

  /**
   * Notify the parent that a generated password was filled into a field or
   * edited so that it can potentially be saved.
   * @param {HTMLInputElement} passwordField
   */
  _generatedPasswordFilledOrEdited(passwordField) {
    log("_generatedPasswordFilledOrEdited", passwordField);

    if (!LoginHelper.enabled) {
      throw new Error(
        "A generated password was filled while the password manager was disabled."
      );
    }

    let win = passwordField.ownerGlobal;
    let formLikeRoot = FormLikeFactory.findRootForField(passwordField);

    this._highlightFilledField(passwordField);

    // change: Listen for changes to the field filled with the generated password so we can preserve edits.
    // input: Listen for the field getting blanked (without blurring) or a paste
    for (let eventType of ["blur", "change", "focus", "input"]) {
      passwordField.addEventListener(eventType, observer, {
        capture: true,
        mozSystemGroup: true,
      });
    }
    // Unmask the password field
    this._togglePasswordFieldMasking(passwordField, true);

    if (PrivateBrowsingUtils.isContentWindowPrivate(win)) {
      log(
        "_generatedPasswordFilledOrEdited: not automatically saving the password in private browsing mode"
      );
      return;
    }

    let loginForm = LoginFormFactory.createFromField(passwordField);
    let formActionOrigin = LoginHelper.getFormActionOrigin(loginForm);
    let origin = LoginHelper.getLoginOrigin(
      passwordField.ownerDocument.documentURI
    );
    let recipes = LoginRecipesContent.getRecipes(this, origin, win);
    let [usernameField] = this._getFormFields(loginForm, false, recipes);
    let username = (usernameField && usernameField.value) || "";
    // Avoid prompting twice for the same value,
    // e.g. context menu fill followed by change (blur) event
    if (
      this._compareAndUpdatePreviouslySentValues(
        formLikeRoot,
        username,
        passwordField.value,
        true // dismissed
      )
    ) {
      log(
        "(generatedPasswordFilledOrEdited ignored -- already messaged with the same password value)"
      );
      return;
    }

    this.sendAsyncMessage("PasswordManager:onGeneratedPasswordFilledOrEdited", {
      formActionOrigin,
      password: passwordField.value,
      username: (usernameField && usernameField.value) || "",
    });
  }

  _togglePasswordFieldMasking(passwordField, unmask) {
    let { editor } = passwordField;

    if (passwordField.type != "password") {
      // The type may have been changed by the website.
      log("_togglePasswordFieldMasking: Field isn't type=password");
      return;
    }

    if (!unmask && !editor) {
      // It hasn't been created yet but the default is to be masked anyways.
      return;
    }

    if (unmask) {
      editor.unmask(0);
      return;
    }

    if (editor.autoMaskingEnabled) {
      return;
    }
    editor.mask();
  }

  /** Remove login field highlight when its value is cleared or overwritten.
   */
  _removeFillFieldHighlight(event) {
    let winUtils = event.target.ownerGlobal.windowUtils;
    winUtils.removeManuallyManagedState(event.target, AUTOFILL_STATE);
  }

  /**
   * Highlight login fields on autocomplete or autofill on page load.
   * @param {Node} element that needs highlighting.
   */
  _highlightFilledField(element) {
    let winUtils = element.ownerGlobal.windowUtils;

    winUtils.addManuallyManagedState(element, AUTOFILL_STATE);
    // Remove highlighting when the field is changed.
    element.addEventListener("input", this._removeFillFieldHighlight, {
      mozSystemGroup: true,
      once: true,
    });
  }

  /**
   * Filter logins for exact origin/formActionOrigin and dedupe on usernamematche
   * @param {nsILoginInfo[]} logins an array of nsILoginInfo that could be
   *        used for the form, including ones with a different form action origin
   *        which are only used when the fill is userTriggered
   * @param {LoginForm} form
   */
  _filterForExactFormOriginLogins(logins, form) {
    let loginOrigin = LoginHelper.getLoginOrigin(
      form.ownerDocument.documentURI
    );
    let formActionOrigin = LoginHelper.getFormActionOrigin(form);

    logins = logins.filter(l => {
      let formActionMatches = LoginHelper.isOriginMatching(
        l.formActionOrigin,
        formActionOrigin,
        {
          schemeUpgrades: LoginHelper.schemeUpgrades,
          acceptWildcardMatch: true,
          acceptDifferentSubdomains: false,
        }
      );
      let formOriginMatches = LoginHelper.isOriginMatching(
        l.origin,
        loginOrigin,
        {
          schemeUpgrades: LoginHelper.schemeUpgrades,
          acceptWildcardMatch: true,
          acceptDifferentSubdomains: false,
        }
      );
      return formActionMatches && formOriginMatches;
    });

    // Since the logins are already filtered now to only match the origin and formAction,
    // dedupe to just the username since remaining logins may have different schemes.
    logins = LoginHelper.dedupeLogins(
      logins,
      ["username"],
      ["scheme", "timePasswordChanged"],
      loginOrigin,
      formActionOrigin
    );
    return logins;
  }

  /**
   * Attempt to find the username and password fields in a form, and fill them
   * in using the provided logins and recipes.
   *
   * @param {LoginForm} form
   * @param {nsILoginInfo[]} foundLogins an array of nsILoginInfo that could be
   *        used for the form, including ones with a different form action origin
   *        which are only used when the fill is userTriggered
   * @param {Set} recipes a set of recipes that could be used to affect how the
   *        form is filled
   * @param {Object} [options = {}] a list of options for this method
   * @param {HTMLInputElement} [options.inputElement = null] an optional target
   *        input element we want to fill
   * @param {bool} [options.autofillForm = false] denotes if we should fill the
   *        form in automatically
   * @param {bool} [options.clobberUsername = false] controls if an existing
   *        username can be overwritten. If this is false and an inputElement
   *        of type password is also passed, the username field will be ignored.
   *        If this is false and no inputElement is passed, if the username
   *        field value is not found in foundLogins, it will not fill the
   *        password.
   * @param {bool} [options.clobberPassword = false] controls if an existing
   *        password value can be overwritten
   * @param {bool} [options.userTriggered = false] an indication of whether
   *        this filling was triggered by the user
   */
  // eslint-disable-next-line complexity
  _fillForm(
    form,
    foundLogins,
    recipes,
    {
      inputElement = null,
      autofillForm = false,
      clobberUsername = false,
      clobberPassword = false,
      userTriggered = false,
    } = {}
  ) {
    if (ChromeUtils.getClassName(form) === "HTMLFormElement") {
      throw new Error("_fillForm should only be called with LoginForm objects");
    }

    log("_fillForm", form.elements);
    let usernameField;
    // Will be set to one of AUTOFILL_RESULT in the `try` block.
    let autofillResult = -1;
    const AUTOFILL_RESULT = {
      FILLED: 0,
      NO_PASSWORD_FIELD: 1,
      PASSWORD_DISABLED_READONLY: 2,
      NO_LOGINS_FIT: 3,
      NO_SAVED_LOGINS: 4,
      EXISTING_PASSWORD: 5,
      EXISTING_USERNAME: 6,
      MULTIPLE_LOGINS: 7,
      NO_AUTOFILL_FORMS: 8,
      AUTOCOMPLETE_OFF: 9,
      INSECURE: 10,
      PASSWORD_AUTOCOMPLETE_NEW_PASSWORD: 11,
    };

    try {
      // Nothing to do if we have no matching (excluding form action
      // checks) logins available, and there isn't a need to show
      // the insecure form warning.
      if (
        !foundLogins.length &&
        (InsecurePasswordUtils.isFormSecure(form) ||
          !LoginHelper.showInsecureFieldWarning)
      ) {
        // We don't log() here since this is a very common case.
        autofillResult = AUTOFILL_RESULT.NO_SAVED_LOGINS;
        return;
      }

      // Heuristically determine what the user/pass fields are
      // We do this before checking to see if logins are stored,
      // so that the user isn't prompted for a master password
      // without need.
      let passwordField;
      [usernameField, passwordField] = this._getFormFields(
        form,
        false,
        recipes
      );

      // If we have a password inputElement parameter and it's not
      // the same as the one heuristically found, use the parameter
      // one instead.
      if (inputElement) {
        if (inputElement.type == "password") {
          passwordField = inputElement;
          if (!clobberUsername) {
            usernameField = null;
          }
        } else if (LoginHelper.isUsernameFieldType(inputElement)) {
          usernameField = inputElement;
        } else {
          throw new Error("Unexpected input element type.");
        }
      }

      // Need a valid password field to do anything.
      if (passwordField == null) {
        log("not filling form, no password field found");
        autofillResult = AUTOFILL_RESULT.NO_PASSWORD_FIELD;
        return;
      }

      // If the password field is disabled or read-only, there's nothing to do.
      if (passwordField.disabled || passwordField.readOnly) {
        log("not filling form, password field disabled or read-only");
        autofillResult = AUTOFILL_RESULT.PASSWORD_DISABLED_READONLY;
        return;
      }

      // Attach autocomplete stuff to the username field, if we have
      // one. This is normally used to select from multiple accounts,
      // but even with one account we should refill if the user edits.
      // We would also need this attached to show the insecure login
      // warning, regardless of saved login.
      if (usernameField) {
        gFormFillService.markAsLoginManagerField(usernameField);
        usernameField.addEventListener("keydown", observer);
      }

      if (!userTriggered) {
        // Only autofill logins that match the form's action and origin. In the above code
        // we have attached autocomplete for logins that don't match the form action.
        foundLogins = this._filterForExactFormOriginLogins(foundLogins, form);
      }

      // Nothing to do if we have no matching logins available.
      // Only insecure pages reach this block and logs the same
      // telemetry flag.
      if (!foundLogins.length) {
        // We don't log() here since this is a very common case.
        autofillResult = AUTOFILL_RESULT.NO_SAVED_LOGINS;
        return;
      }

      // Prevent autofilling insecure forms.
      if (
        !userTriggered &&
        !LoginHelper.insecureAutofill &&
        !InsecurePasswordUtils.isFormSecure(form)
      ) {
        log("not filling form since it's insecure");
        autofillResult = AUTOFILL_RESULT.INSECURE;
        return;
      }

      // Discard logins which have username/password values that don't
      // fit into the fields (as specified by the maxlength attribute).
      // The user couldn't enter these values anyway, and it helps
      // with sites that have an extra PIN to be entered (bug 391514)
      let maxUsernameLen = Number.MAX_VALUE;
      let maxPasswordLen = Number.MAX_VALUE;

      // If attribute wasn't set, default is -1.
      if (usernameField && usernameField.maxLength >= 0) {
        maxUsernameLen = usernameField.maxLength;
      }
      if (passwordField.maxLength >= 0) {
        maxPasswordLen = passwordField.maxLength;
      }

      let logins = foundLogins.filter(function(l) {
        let fit =
          l.username.length <= maxUsernameLen &&
          l.password.length <= maxPasswordLen;
        if (!fit) {
          log("Ignored", l.username, "login: won't fit");
        }

        return fit;
      }, this);

      if (!logins.length) {
        log("form not filled, none of the logins fit in the field");
        autofillResult = AUTOFILL_RESULT.NO_LOGINS_FIT;
        return;
      }

      const passwordACFieldName = passwordField.getAutocompleteInfo().fieldName;

      // If the password field has the autocomplete value of "new-password"
      // and we're autofilling without user interaction, there's nothing to do.
      if (!userTriggered && passwordACFieldName == "new-password") {
        log(
          "not filling form, password field has the autocomplete new-password value"
        );
        autofillResult = AUTOFILL_RESULT.PASSWORD_AUTOCOMPLETE_NEW_PASSWORD;
        return;
      }

      // Don't clobber an existing password.
      if (passwordField.value && !clobberPassword) {
        log("form not filled, the password field was already filled");
        autofillResult = AUTOFILL_RESULT.EXISTING_PASSWORD;
        return;
      }

      // Select a login to use for filling in the form.
      let selectedLogin;
      if (
        !clobberUsername &&
        usernameField &&
        (usernameField.value ||
          usernameField.disabled ||
          usernameField.readOnly)
      ) {
        // If username was specified in the field, it's disabled or it's readOnly, only fill in the
        // password if we find a matching login.
        let username = usernameField.value.toLowerCase();

        let matchingLogins = logins.filter(
          l => l.username.toLowerCase() == username
        );
        if (!matchingLogins.length) {
          log(
            "Password not filled. None of the stored logins match the username already present."
          );
          autofillResult = AUTOFILL_RESULT.EXISTING_USERNAME;
          return;
        }

        // If there are multiple, and one matches case, use it
        for (let l of matchingLogins) {
          if (l.username == usernameField.value) {
            selectedLogin = l;
          }
        }
        // Otherwise just use the first
        if (!selectedLogin) {
          selectedLogin = matchingLogins[0];
        }
      } else if (logins.length == 1) {
        selectedLogin = logins[0];
      } else {
        // We have multiple logins. Handle a special case here, for sites
        // which have a normal user+pass login *and* a password-only login
        // (eg, a PIN). Prefer the login that matches the type of the form
        // (user+pass or pass-only) when there's exactly one that matches.
        let matchingLogins;
        if (usernameField) {
          matchingLogins = logins.filter(l => l.username);
        } else {
          matchingLogins = logins.filter(l => !l.username);
        }

        if (matchingLogins.length != 1) {
          log("Multiple logins for form, so not filling any.");
          autofillResult = AUTOFILL_RESULT.MULTIPLE_LOGINS;
          return;
        }

        selectedLogin = matchingLogins[0];
      }

      // We will always have a selectedLogin at this point.

      if (!autofillForm) {
        log("autofillForms=false but form can be filled");
        autofillResult = AUTOFILL_RESULT.NO_AUTOFILL_FORMS;
        return;
      }

      if (
        !userTriggered &&
        passwordACFieldName == "off" &&
        !LoginHelper.autofillAutocompleteOff
      ) {
        log(
          "Not autofilling the login because we're respecting autocomplete=off"
        );
        autofillResult = AUTOFILL_RESULT.AUTOCOMPLETE_OFF;
        return;
      }

      // Fill the form

      if (usernameField) {
        // Don't modify the username field if it's disabled or readOnly so we preserve its case.
        let disabledOrReadOnly =
          usernameField.disabled || usernameField.readOnly;

        let userNameDiffers = selectedLogin.username != usernameField.value;
        // Don't replace the username if it differs only in case, and the user triggered
        // this autocomplete. We assume that if it was user-triggered the entered text
        // is desired.
        let userEnteredDifferentCase =
          userTriggered &&
          userNameDiffers &&
          usernameField.value.toLowerCase() ==
            selectedLogin.username.toLowerCase();

        if (!disabledOrReadOnly) {
          if (!userEnteredDifferentCase && userNameDiffers) {
            usernameField.setUserInput(selectedLogin.username);
          }

          this._highlightFilledField(usernameField);
        }
      }

      let doc = form.ownerDocument;
      if (passwordField.value != selectedLogin.password) {
        // Ensure the field gets re-masked in case a generated password was
        // filled into it previously.
        this._stopTreatingAsGeneratedPasswordField(passwordField);
        passwordField.setUserInput(selectedLogin.password);
        let autoFilledLogin = {
          guid: selectedLogin.QueryInterface(Ci.nsILoginMetaInfo).guid,
          username: selectedLogin.username,
          usernameField: usernameField
            ? Cu.getWeakReference(usernameField)
            : null,
          password: selectedLogin.password,
          passwordField: Cu.getWeakReference(passwordField),
        };
        log(
          "Saving autoFilledLogin",
          autoFilledLogin.guid,
          "for",
          form.rootElement
        );
        this.stateForDocument(doc).fillsByRootElement.set(
          form.rootElement,
          autoFilledLogin
        );
      }

      this._highlightFilledField(passwordField);

      log("_fillForm succeeded");
      autofillResult = AUTOFILL_RESULT.FILLED;
    } catch (ex) {
      Cu.reportError(ex);
      throw ex;
    } finally {
      if (autofillResult == -1) {
        // eslint-disable-next-line no-unsafe-finally
        throw new Error("_fillForm: autofillResult must be specified");
      }

      if (!userTriggered) {
        // Ignore fills as a result of user action for this probe.
        Services.telemetry
          .getHistogramById("PWMGR_FORM_AUTOFILL_RESULT")
          .add(autofillResult);

        if (usernameField) {
          let focusedElement = gFormFillService.focusedInput;
          if (
            usernameField == focusedElement &&
            autofillResult !== AUTOFILL_RESULT.FILLED
          ) {
            log(
              "_fillForm: Opening username autocomplete popup since the form wasn't autofilled"
            );
            gFormFillService.showPopup();
          }
        }
      }

      if (usernameField) {
        log("_fillForm: Attaching event listeners to usernameField");
        usernameField.addEventListener("focus", observer);
        usernameField.addEventListener("mousedown", observer);
      }

      this.sendAsyncMessage("PasswordManager:formProcessed", {
        formid: form.rootElement.id,
      });
    }
  }

  /**
   * Given a field, determine whether that field was last filled as a username
   * field AND whether the username is still filled in with the username AND
   * whether the associated password field has the matching password.
   *
   * @note This could possibly be unified with getFieldContext but they have
   * slightly different use cases. getFieldContext looks up recipes whereas this
   * method doesn't need to since it's only returning a boolean based upon the
   * recipes used for the last fill (in _fillForm).
   *
   * @param {HTMLInputElement} aUsernameField element contained in a LoginForm
   *                                          cached in LoginFormFactory.
   * @returns {Boolean} whether the username and password fields still have the
   *                    last-filled values, if previously filled.
   */
  _isLoginAlreadyFilled(aUsernameField) {
    let formLikeRoot = FormLikeFactory.findRootForField(aUsernameField);
    // Look for the existing LoginForm.
    let existingLoginForm = LoginFormFactory.getForRootElement(formLikeRoot);
    if (!existingLoginForm) {
      throw new Error(
        "_isLoginAlreadyFilled called with a username field with " +
          "no rootElement LoginForm"
      );
    }

    log("_isLoginAlreadyFilled: existingLoginForm", existingLoginForm);
    let filledLogin = this.stateForDocument(
      aUsernameField.ownerDocument
    ).fillsByRootElement.get(formLikeRoot);
    if (!filledLogin) {
      return false;
    }

    // Unpack the weak references.
    let autoFilledUsernameField = filledLogin.usernameField
      ? filledLogin.usernameField.get()
      : null;
    let autoFilledPasswordField = filledLogin.passwordField.get();

    // Check username and password values match what was filled.
    if (
      !autoFilledUsernameField ||
      autoFilledUsernameField != aUsernameField ||
      autoFilledUsernameField.value != filledLogin.username ||
      !autoFilledPasswordField ||
      autoFilledPasswordField.value != filledLogin.password
    ) {
      return false;
    }

    return true;
  }

  /**
   * Returns the username and password fields found in the form by input
   * element into form.
   *
   * @param {HTMLInputElement} aField
   *                           A form field into form.
   * @return {Array} [usernameField, newPasswordField, oldPasswordField]
   *
   * More detail of these values is same as _getFormFields.
   */
  getUserNameAndPasswordFields(aField) {
    // If the element is not a proper form field, return null.
    if (
      ChromeUtils.getClassName(aField) !== "HTMLInputElement" ||
      (aField.type != "password" && !LoginHelper.isUsernameFieldType(aField)) ||
      aField.nodePrincipal.isNullPrincipal ||
      !aField.ownerDocument
    ) {
      return [null, null, null];
    }
    let form = LoginFormFactory.createFromField(aField);

    let doc = aField.ownerDocument;
    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    let recipes = LoginRecipesContent.getRecipes(
      this,
      formOrigin,
      doc.defaultView
    );

    return this._getFormFields(form, false, recipes);
  }

  /**
   * Verify if a field is a valid login form field and
   * returns some information about it's LoginForm.
   *
   * @param {Element} aField
   *                  A form field we want to verify.
   *
   * @returns {Object} an object with information about the
   *                   LoginForm username and password field
   *                   or null if the passed field is invalid.
   */
  getFieldContext(aField) {
    // If the element is not a proper form field, return null.
    if (
      ChromeUtils.getClassName(aField) !== "HTMLInputElement" ||
      (aField.type != "password" && !LoginHelper.isUsernameFieldType(aField)) ||
      aField.nodePrincipal.isNullPrincipal ||
      aField.nodePrincipal.schemeIs("about") ||
      !aField.ownerDocument
    ) {
      return null;
    }

    let [usernameField, newPasswordField] = this.getUserNameAndPasswordFields(
      aField
    );

    // If we are not verifying a password field, we want
    // to use aField as the username field.
    if (aField.type != "password") {
      usernameField = aField;
    }

    return {
      usernameField: {
        found: !!usernameField,
        disabled:
          usernameField && (usernameField.disabled || usernameField.readOnly),
      },
      passwordField: {
        found: !!newPasswordField,
        disabled:
          newPasswordField &&
          (newPasswordField.disabled || newPasswordField.readOnly),
      },
    };
  }
};
