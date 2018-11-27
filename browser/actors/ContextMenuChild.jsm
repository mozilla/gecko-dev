/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContextMenuChild"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  findAllCssSelectors: "resource://gre/modules/css-selector.js",
  SpellCheckHelper: "resource://gre/modules/InlineSpellChecker.jsm",
  LoginManagerContent: "resource://gre/modules/LoginManagerContent.jsm",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  InlineSpellCheckerContent: "resource://gre/modules/InlineSpellCheckerContent.jsm",
});

XPCOMUtils.defineLazyGetter(this, "PageMenuChild", () => {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/PageMenu.jsm", tmp);
  return new tmp.PageMenuChild();
});

const messageListeners = {
  "ContextMenu:BookmarkFrame": function(aMessage) {
    let frame = this.getTarget(aMessage).ownerDocument;

    this.mm.sendAsyncMessage("ContextMenu:BookmarkFrame:Result",
                             { title: frame.title });
  },

  "ContextMenu:Canvas:ToBlobURL": function(aMessage) {
    this.getTarget(aMessage).toBlob((blob) => {
      let blobURL = URL.createObjectURL(blob);
      this.mm.sendAsyncMessage("ContextMenu:Canvas:ToBlobURL:Result", { blobURL });
    });
  },

  "ContextMenu:DoCustomCommand": function(aMessage) {
    E10SUtils.wrapHandlingUserInput(
      this.content,
      aMessage.data.handlingUserInput,
      () => PageMenuChild.executeMenu(aMessage.data.generatedItemId)
    );
  },

  "ContextMenu:Hiding": function() {
    this.context = null;
    this.target = null;
  },

  "ContextMenu:MediaCommand": function(aMessage) {
    E10SUtils.wrapHandlingUserInput(
      this.content, aMessage.data.handlingUserInput, () => {
        let media = this.getTarget(aMessage, "element");

        switch (aMessage.data.command) {
          case "play":
            media.play();
            break;
          case "pause":
            media.pause();
            break;
          case "loop":
            media.loop = !media.loop;
            break;
          case "mute":
            media.muted = true;
            break;
          case "unmute":
            media.muted = false;
            break;
          case "playbackRate":
            media.playbackRate = aMessage.data.data;
            break;
          case "hidecontrols":
            media.removeAttribute("controls");
            break;
          case "showcontrols":
            media.setAttribute("controls", "true");
            break;
          case "fullscreen":
            if (this.content.document.fullscreenEnabled) {
              media.requestFullscreen();
            }

            break;
        }
      }
    );
  },

  "ContextMenu:ReloadFrame": function(aMessage) {
    let forceReload = aMessage.objects && aMessage.objects.forceReload;
    this.getTarget(aMessage).ownerDocument.location.reload(forceReload);
  },

  "ContextMenu:ReloadImage": function(aMessage) {
    let image = this.getTarget(aMessage);

    if (image instanceof Ci.nsIImageLoadingContent) {
      image.forceReload();
    }
  },

  "ContextMenu:SearchFieldBookmarkData": function(aMessage) {
    let node = this.getTarget(aMessage);
    let charset = node.ownerDocument.characterSet;
    let formBaseURI = Services.io.newURI(node.form.baseURI, charset);
    let formURI = Services.io.newURI(node.form.getAttribute("action"),
                                     charset, formBaseURI);
    let spec = formURI.spec;
    let isURLEncoded =  (node.form.method.toUpperCase() == "POST" &&
                         (node.form.enctype == "application/x-www-form-urlencoded" ||
                          node.form.enctype == ""));
    let title = node.ownerDocument.title;
    let formData = [];

    function escapeNameValuePair(aName, aValue, aIsFormUrlEncoded) {
      if (aIsFormUrlEncoded) {
        return escape(aName + "=" + aValue);
      }

      return escape(aName) + "=" + escape(aValue);
    }

    for (let el of node.form.elements) {
      if (!el.type) // happens with fieldsets
        continue;

      if (el == node) {
        formData.push((isURLEncoded) ? escapeNameValuePair(el.name, "%s", true) :
                                       // Don't escape "%s", just append
                                       escapeNameValuePair(el.name, "", false) + "%s");
        continue;
      }

      let type = el.type.toLowerCase();

      if (((el instanceof this.content.HTMLInputElement && el.mozIsTextField(true)) ||
          type == "hidden" || type == "textarea") ||
          ((type == "checkbox" || type == "radio") && el.checked)) {
        formData.push(escapeNameValuePair(el.name, el.value, isURLEncoded));
      } else if (el instanceof this.content.HTMLSelectElement && el.selectedIndex >= 0) {
        for (let j = 0; j < el.options.length; j++) {
          if (el.options[j].selected)
            formData.push(escapeNameValuePair(el.name, el.options[j].value,
                                              isURLEncoded));
        }
      }
    }

    let postData;

    if (isURLEncoded) {
      postData = formData.join("&");
    } else {
      let separator = spec.includes("?") ? "&" : "?";
      spec += separator + formData.join("&");
    }

    this.mm.sendAsyncMessage("ContextMenu:SearchFieldBookmarkData:Result",
                             { spec, title, postData, charset });
  },

  "ContextMenu:SaveVideoFrameAsImage": function(aMessage) {
    let video = this.getTarget(aMessage);
    let canvas = this.content.document.createElementNS("http://www.w3.org/1999/xhtml",
                                                       "canvas");
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;

    let ctxDraw = canvas.getContext("2d");
    ctxDraw.drawImage(video, 0, 0);

    this.mm.sendAsyncMessage("ContextMenu:SaveVideoFrameAsImage:Result", {
      dataURL: canvas.toDataURL("image/jpeg", ""),
    });
  },

  "ContextMenu:SetAsDesktopBackground": function(aMessage) {
    let target = this.getTarget(aMessage);

    // Paranoia: check disableSetDesktopBackground again, in case the
    // image changed since the context menu was initiated.
    let disable = this._disableSetDesktopBackground(target);

    if (!disable) {
      try {
        BrowserUtils.urlSecurityCheck(target.currentURI.spec,
                                      target.ownerDocument.nodePrincipal);
        let canvas = this.content.document.createElement("canvas");
        canvas.width = target.naturalWidth;
        canvas.height = target.naturalHeight;
        let ctx = canvas.getContext("2d");
        ctx.drawImage(target, 0, 0);
        let dataUrl = canvas.toDataURL();
        let url = (new URL(target.ownerDocument.location.href)).pathname;
        let imageName = url.substr(url.lastIndexOf("/") + 1);
        this.mm.sendAsyncMessage("ContextMenu:SetAsDesktopBackground:Result",
                                 { dataUrl, imageName });
      } catch (e) {
        Cu.reportError(e);
        disable = true;
      }
    }

    if (disable) {
      this.mm.sendAsyncMessage("ContextMenu:SetAsDesktopBackground:Result",
                               { disable });
    }
  },
};

let contextMenus = new WeakMap();

class ContextMenuChild extends ActorChild {
  // PUBLIC
  constructor(dispatcher) {
    super(dispatcher);

    contextMenus.set(this.mm, this);

    this.target = null;
    this.context = null;
    this.lastMenuTarget = null;

    Object.keys(messageListeners).forEach(key =>
      this.mm.addMessageListener(key, messageListeners[key].bind(this))
    );
  }

  static getTarget(mm, message, key) {
    return contextMenus.get(mm).getTarget(message, key);
  }

  static getLastTarget(mm) {
    let contextMenu = contextMenus.get(mm);
    return contextMenu && contextMenu.lastMenuTarget;
  }

  /**
   * Returns the event target of the context menu, using a locally stored
   * reference if possible. If not, and aMessage.objects is defined,
   * aMessage.objects[aKey] is returned. Otherwise null.
   * @param  {Object} aMessage Message with a objects property
   * @param  {String} aKey     Key for the target on aMessage.objects
   * @return {Object}          Context menu target
   */
  getTarget(aMessage, aKey = "target") {
    return this.target || (aMessage.objects && aMessage.objects[aKey]);
  }

  // PRIVATE
  _isXULTextLinkLabel(aNode) {
    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    return aNode.namespaceURI == XUL_NS &&
           aNode.tagName == "label" &&
           aNode.classList.contains("text-link") &&
           aNode.href;
  }

  // Generate fully qualified URL for clicked-on link.
  _getLinkURL() {
    let href = this.context.link.href;

    if (href) {
      // Handle SVG links:
      if (typeof href == "object" && href.animVal) {
        return this._makeURLAbsolute(this.context.link.baseURI, href.animVal);
      }

      return href;
    }

    href = this.context.link.getAttribute("href") ||
           this.context.link.getAttributeNS("http://www.w3.org/1999/xlink", "href");

    if (!href || !href.match(/\S/)) {
      // Without this we try to save as the current doc,
      // for example, HTML case also throws if empty
      throw "Empty href";
    }

    return this._makeURLAbsolute(this.context.link.baseURI, href);
  }

  _getLinkURI() {
    try {
      return Services.io.newURI(this.context.linkURL);
    } catch (ex) {
     // e.g. empty URL string
    }

    return null;
  }

  // Get text of link.
  _getLinkText() {
    let text = this._gatherTextUnder(this.context.link);

    if (!text || !text.match(/\S/)) {
      text = this.context.link.getAttribute("title");
      if (!text || !text.match(/\S/)) {
        text = this.context.link.getAttribute("alt");
        if (!text || !text.match(/\S/)) {
          text = this.context.linkURL;
        }
      }
    }

    return text;
  }

  _getLinkProtocol() {
    if (this.context.linkURI) {
      return this.context.linkURI.scheme; // can be |undefined|
    }

    return null;
  }

  // Returns true if clicked-on link targets a resource that can be saved.
  _isLinkSaveable(aLink) {
    // We don't do the Right Thing for news/snews yet, so turn them off
    // until we do.
    return this.context.linkProtocol && !(
           this.context.linkProtocol == "mailto" ||
           this.context.linkProtocol == "javascript" ||
           this.context.linkProtocol == "news" ||
           this.context.linkProtocol == "snews");
  }

  // Gather all descendent text under given document node.
  _gatherTextUnder(root) {
    let text = "";
    let node = root.firstChild;
    let depth = 1;
    while (node && depth > 0) {
      // See if this node is text.
      if (node.nodeType == node.TEXT_NODE) {
        // Add this text to our collection.
        text += " " + node.data;
      } else if (node instanceof this.content.HTMLImageElement) {
        // If it has an "alt" attribute, add that.
        let altText = node.getAttribute( "alt" );
        if ( altText && altText != "" ) {
          text += " " + altText;
        }
      }
      // Find next node to test.
      // First, see if this node has children.
      if (node.hasChildNodes()) {
        // Go to first child.
        node = node.firstChild;
        depth++;
      } else {
        // No children, try next sibling (or parent next sibling).
        while (depth > 0 && !node.nextSibling) {
          node = node.parentNode;
          depth--;
        }
        if (node.nextSibling) {
          node = node.nextSibling;
        }
      }
    }

    // Strip leading and tailing whitespace.
    text = text.trim();
    // Compress remaining whitespace.
    text = text.replace(/\s+/g, " ");
    return text;
  }

  // Returns a "url"-type computed style attribute value, with the url() stripped.
  _getComputedURL(aElem, aProp) {
    let urls = aElem.ownerGlobal.getComputedStyle(aElem).getCSSImageURLs(aProp);

    if (!urls.length) {
      return null;
    }

    if (urls.length != 1) {
      throw "found multiple URLs";
    }

    return urls[0];
  }

  _makeURLAbsolute(aBase, aUrl) {
    return Services.io.newURI(aUrl, null, Services.io.newURI(aBase)).spec;
  }

  _isProprietaryDRM() {
    return this.context.target.isEncrypted && this.context.target.mediaKeys &&
           this.context.target.mediaKeys.keySystem != "org.w3.clearkey";
  }

  _isMediaURLReusable(aURL) {
    if (aURL.startsWith("blob:")) {
      return URL.isValidURL(aURL);
    }

    return true;
  }

  _isTargetATextBox(node) {
    if (node instanceof this.content.HTMLInputElement) {
      return node.mozIsTextField(false);
    }

    return (node instanceof this.content.HTMLTextAreaElement);
  }

  _isSpellCheckEnabled(aNode) {
    // We can always force-enable spellchecking on textboxes
    if (this._isTargetATextBox(aNode)) {
      return true;
    }

    // We can never spell check something which is not content editable
    let editable = aNode.isContentEditable;

    if (!editable && aNode.ownerDocument) {
      editable = aNode.ownerDocument.designMode == "on";
    }

    if (!editable) {
      return false;
    }

    // Otherwise make sure that nothing in the parent chain disables spellchecking
    return aNode.spellcheck;
  }

  _disableSetDesktopBackground(aTarget) {
    // Disable the Set as Desktop Background menu item if we're still trying
    // to load the image or the load failed.
    if (!(aTarget instanceof Ci.nsIImageLoadingContent)) {
      return true;
    }

    if (("complete" in aTarget) && !aTarget.complete) {
      return true;
    }

    if (aTarget.currentURI.schemeIs("javascript")) {
      return true;
    }

    let request = aTarget.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);

    if (!request) {
      return true;
    }

    return false;
  }

  handleEvent(aEvent) {
    let defaultPrevented = aEvent.defaultPrevented;

    if (!Services.prefs.getBoolPref("dom.event.contextmenu.enabled")) {
      let plugin = null;

      try {
        plugin = aEvent.composedTarget.QueryInterface(Ci.nsIObjectLoadingContent);
      } catch (e) {}

      if (plugin && plugin.displayedType == Ci.nsIObjectLoadingContent.TYPE_PLUGIN) {
        // Don't open a context menu for plugins.
        return;
      }

      defaultPrevented = false;
    }

    if (defaultPrevented) {
      return;
    }

    let doc = aEvent.composedTarget.ownerDocument;
    let {
      mozDocumentURIIfNotForErrorPages: docLocation,
      characterSet: charSet,
      baseURI,
      referrer,
      referrerPolicy,
    } = doc;
    docLocation = docLocation && docLocation.spec;
    let frameOuterWindowID = WebNavigationFrames.getFrameId(doc.defaultView);
    let loginFillInfo = LoginManagerContent.getFieldContext(aEvent.composedTarget);

    // The same-origin check will be done in nsContextMenu.openLinkInTab.
    let parentAllowsMixedContent = !!this.docShell.mixedContentChannel;

    // Get referrer attribute from clicked link and parse it
    let referrerAttrValue =
      Services.netUtils.parseAttributePolicyString(aEvent.composedTarget.
                                                   getAttribute("referrerpolicy"));

    if (referrerAttrValue !== Ci.nsIHttpChannel.REFERRER_POLICY_UNSET) {
      referrerPolicy = referrerAttrValue;
    }

    let disableSetDesktopBg = null;

    // Media related cache info parent needs for saving
    let contentType = null;
    let contentDisposition = null;
    if (aEvent.composedTarget.nodeType == aEvent.composedTarget.ELEMENT_NODE &&
        aEvent.composedTarget instanceof Ci.nsIImageLoadingContent &&
        aEvent.composedTarget.currentURI) {
      disableSetDesktopBg = this._disableSetDesktopBackground(aEvent.composedTarget);

      try {
        let imageCache = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools)
                                                         .getImgCacheForDocument(doc);
        // The image cache's notion of where this image is located is
        // the currentURI of the image loading content.
        let props = imageCache.findEntryProperties(aEvent.composedTarget.currentURI, doc);

        try {
          contentType = props.get("type", Ci.nsISupportsCString).data;
        } catch (e) {}

        try {
          contentDisposition = props.get("content-disposition", Ci.nsISupportsCString).data;
        } catch (e) {}
      } catch (e) {}
    }

    let selectionInfo = BrowserUtils.getSelectionDetails(this.content);
    let loadContext = this.docShell.QueryInterface(Ci.nsILoadContext);
    let userContextId = loadContext.originAttributes.userContextId;
    let popupNodeSelectors = findAllCssSelectors(aEvent.composedTarget);

    this._setContext(aEvent);
    let context = this.context;
    this.target = context.target;

    let spellInfo = null;
    let editFlags = null;
    let principal = null;
    let customMenuItems = null;

    let targetAsCPOW = context.target;
    if (targetAsCPOW) {
      this._cleanContext();
    }

    let isRemote = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

    if (isRemote) {
      editFlags = SpellCheckHelper.isEditable(aEvent.composedTarget, this.content);

      if (editFlags & SpellCheckHelper.SPELLCHECKABLE) {
        spellInfo = InlineSpellCheckerContent.initContextMenu(aEvent, editFlags, this.mm);
      }

      // Set the event target first as the copy image command needs it to
      // determine what was context-clicked on. Then, update the state of the
      // commands on the context menu.
      this.docShell.contentViewer.QueryInterface(Ci.nsIContentViewerEdit)
                   .setCommandNode(aEvent.composedTarget);
      aEvent.composedTarget.ownerGlobal.updateCommands("contentcontextmenu");

      customMenuItems = PageMenuChild.build(aEvent.composedTarget);
      principal = doc.nodePrincipal;
    }

    let data = {
      context,
      charSet,
      baseURI,
      isRemote,
      referrer,
      editFlags,
      principal,
      spellInfo,
      contentType,
      docLocation,
      loginFillInfo,
      selectionInfo,
      userContextId,
      referrerPolicy,
      customMenuItems,
      contentDisposition,
      frameOuterWindowID,
      popupNodeSelectors,
      disableSetDesktopBg,
      parentAllowsMixedContent,
    };

    Services.obs.notifyObservers({wrappedJSObject: data}, "on-prepare-contextmenu");

    if (isRemote) {
      this.mm.sendAsyncMessage("contextmenu", data, {
        targetAsCPOW,
      });
    } else {
      let browser = this.docShell.chromeEventHandler;
      let mainWin = browser.ownerGlobal;

      data.documentURIObject = doc.documentURIObject;
      data.disableSetDesktopBackground = data.disableSetDesktopBg;
      delete data.disableSetDesktopBg;

      data.context.targetAsCPOW = targetAsCPOW;

      mainWin.setContextMenuContentData(data);
    }
  }

  /**
   * Some things are not serializable, so we either have to only send
   * their needed data or regenerate them in nsContextMenu.js
   * - target and target.ownerDocument
   * - link
   * - linkURI
   */
  _cleanContext(aEvent) {
    const context = this.context;
    const cleanTarget = Object.create(null);

    cleanTarget.ownerDocument = {
      // used for nsContextMenu.initLeaveDOMFullScreenItems and
      // nsContextMenu.initMediaPlayerItems
      fullscreen: context.target.ownerDocument.fullscreen,

      // used for nsContextMenu.initMiscItems
      contentType: context.target.ownerDocument.contentType,

      // used for nsContextMenu.saveLink
      isPrivate: PrivateBrowsingUtils.isContentWindowPrivate(context.target.ownerGlobal),
    };

    // used for nsContextMenu.initMediaPlayerItems
    Object.assign(cleanTarget, {
      ended: context.target.ended,
      muted: context.target.muted,
      paused: context.target.paused,
      controls: context.target.controls,
      duration: context.target.duration,
    });

    const onMedia = context.onVideo || context.onAudio;

    if (onMedia) {
      Object.assign(cleanTarget, {
        loop: context.target.loop,
        error: context.target.error,
        networkState: context.target.networkState,
        playbackRate: context.target.playbackRate,
        NETWORK_NO_SOURCE: context.target.NETWORK_NO_SOURCE,
      });

      if (context.onVideo) {
        Object.assign(cleanTarget, {
          readyState: context.target.readyState,
          HAVE_CURRENT_DATA: context.target.HAVE_CURRENT_DATA,
        });
      }
    }

    context.target = cleanTarget;

    if (context.link) {
      context.link = { href: context.linkURL };
    }

    delete context.linkURI;
  }

  _setContext(aEvent) {
    this.context = Object.create(null);
    const context = this.context;

    context.timeStamp = aEvent.timeStamp;
    context.screenX = aEvent.screenX;
    context.screenY = aEvent.screenY;
    context.mozInputSource = aEvent.mozInputSource;

    let node = aEvent.composedTarget;

    // Set the node to containing <video>/<audio>/<embed>/<object> if the node
    // is in the videocontrols/pluginProblem UA Widget.
    if (this.content.ShadowRoot) {
      let n = node;
      while (n) {
        if (n instanceof this.content.ShadowRoot) {
          if (n.host instanceof this.content.HTMLMediaElement ||
              n.host instanceof this.content.HTMLEmbedElement ||
              n.host instanceof this.content.HTMLObjectElement) {
            node = n.host;
            break;
          }
          break;
        }
        n = n.parentNode;
      }
    }

    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    context.shouldDisplay = true;

    if (node.nodeType == node.DOCUMENT_NODE ||
        // Don't display for XUL element unless <label class="text-link">
        (node.namespaceURI == XUL_NS && !this._isXULTextLinkLabel(node))) {
      context.shouldDisplay = false;
      return;
    }

    // Initialize context to be sent to nsContextMenu
    // Keep this consistent with the similar code in nsContextMenu's setContext
    context.bgImageURL          = "";
    context.imageDescURL        = "";
    context.imageInfo           = null;
    context.mediaURL            = "";
    context.webExtBrowserType   = "";

    context.canSpellCheck       = false;
    context.hasBGImage          = false;
    context.hasMultipleBGImages = false;
    context.isDesignMode        = false;
    context.inFrame             = false;
    context.inSrcdocFrame       = false;
    context.inSyntheticDoc      = false;
    context.inTabBrowser        = true;
    context.inWebExtBrowser     = false;

    context.link                = null;
    context.linkDownload        = "";
    context.linkHasNoReferrer   = false;
    context.linkProtocol        = "";
    context.linkTextStr         = "";
    context.linkURL             = "";
    context.linkURI             = null;

    context.onAudio             = false;
    context.onCanvas            = false;
    context.onCompletedImage    = false;
    context.onCTPPlugin         = false;
    context.onDRMMedia          = false;
    context.onEditable          = false;
    context.onImage             = false;
    context.onKeywordField      = false;
    context.onLink              = false;
    context.onLoadedImage       = false;
    context.onMailtoLink        = false;
    context.onMozExtLink        = false;
    context.onNumeric           = false;
    context.onPassword          = false;
    context.onSaveableLink      = false;
    context.onSpellcheckable    = false;
    context.onTextInput         = false;
    context.onVideo             = false;

    // Remember the node and its owner document that was clicked
    // This may be modifed before sending to nsContextMenu
    context.target = node;

    context.principal = context.target.ownerDocument.nodePrincipal;
    context.frameOuterWindowID = WebNavigationFrames.getFrameId(context.target.ownerGlobal);

    // Check if we are in a synthetic document (stand alone image, video, etc.).
    context.inSyntheticDoc = context.target.ownerDocument.mozSyntheticDocument;

    context.shouldInitInlineSpellCheckerUINoChildren = false;
    context.shouldInitInlineSpellCheckerUIWithChildren = false;

    let editFlags = SpellCheckHelper.isEditable(context.target, this.content);
    this._setContextForNodesNoChildren(editFlags);
    this._setContextForNodesWithChildren(editFlags);

    this.lastMenuTarget = {
      // Remember the node for extensions.
      targetRef: Cu.getWeakReference(node),
      // The timestamp is used to verify that the target wasn't changed since the observed menu event.
      timeStamp: context.timeStamp,
    };
  }

  /**
   * Sets up the parts of the context menu for when when nodes have no children.
   *
   * @param {Integer} editFlags The edit flags for the node. See SpellCheckHelper
   *                            for the details.
   */
  _setContextForNodesNoChildren(editFlags) {
    const context = this.context;

    if (context.target.nodeType == context.target.TEXT_NODE) {
      // For text nodes, look at the parent node to determine the spellcheck attribute.
      context.canSpellCheck = context.target.parentNode &&
                              this._isSpellCheckEnabled(context.target);
      return;
    }

    // We only deal with TEXT_NODE and ELEMENT_NODE in this function, so return
    // early if we don't have one.
    if (context.target.nodeType != context.target.ELEMENT_NODE) {
      return;
    }

    // See if the user clicked on an image. This check mirrors
    // nsDocumentViewer::GetInImage. Make sure to update both if this is
    // changed.
    if (context.target instanceof Ci.nsIImageLoadingContent &&
        (context.target.currentRequestFinalURI || context.target.currentURI)) {
      context.onImage = true;

      context.imageInfo = {
        currentSrc: context.target.currentSrc,
        width: context.target.width,
        height: context.target.height,
        imageText: context.target.title || context.target.alt,
      };

      const request = context.target.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);

      if (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE)) {
        context.onLoadedImage = true;
      }

      if (request &&
          (request.imageStatus & request.STATUS_LOAD_COMPLETE) &&
          !(request.imageStatus & request.STATUS_ERROR)) {
        context.onCompletedImage = true;
      }

      // The actual URL the image was loaded from (after redirects) is the
      // currentRequestFinalURI.  We should use that as the URL for purposes of
      // deciding on the filename, if it is present. It might not be present
      // if images are blocked.
      context.mediaURL = (context.target.currentRequestFinalURI || context.target.currentURI).spec;

      const descURL = context.target.getAttribute("longdesc");

      if (descURL) {
        context.imageDescURL = this._makeURLAbsolute(context.target.ownerDocument.body.baseURI,
                                                    descURL);
      }
    } else if (context.target instanceof this.content.HTMLCanvasElement) {
      context.onCanvas = true;
    } else if (context.target instanceof this.content.HTMLVideoElement) {
      const mediaURL = context.target.currentSrc || context.target.src;

      if (this._isMediaURLReusable(mediaURL)) {
        context.mediaURL = mediaURL;
      }

      if (this._isProprietaryDRM()) {
        context.onDRMMedia = true;
      }

      // Firefox always creates a HTMLVideoElement when loading an ogg file
      // directly. If the media is actually audio, be smarter and provide a
      // context menu with audio operations.
      if (context.target.readyState >= context.target.HAVE_METADATA &&
          (context.target.videoWidth == 0 || context.target.videoHeight == 0)) {
        context.onAudio = true;
      } else {
        context.onVideo = true;
      }
    } else if (context.target instanceof this.content.HTMLAudioElement) {
      context.onAudio = true;
      const mediaURL = context.target.currentSrc || context.target.src;

      if (this._isMediaURLReusable(mediaURL)) {
        context.mediaURL = mediaURL;
      }

      if (this._isProprietaryDRM()) {
        context.onDRMMedia = true;
      }
    } else if (editFlags & (SpellCheckHelper.INPUT | SpellCheckHelper.TEXTAREA)) {
      context.onTextInput = (editFlags & SpellCheckHelper.TEXTINPUT) !== 0;
      context.onNumeric = (editFlags & SpellCheckHelper.NUMERIC) !== 0;
      context.onEditable = (editFlags & SpellCheckHelper.EDITABLE) !== 0;
      context.onPassword = (editFlags & SpellCheckHelper.PASSWORD) !== 0;
      context.onSpellcheckable = (editFlags & SpellCheckHelper.SPELLCHECKABLE) !== 0;

      // This is guaranteed to be an input or textarea because of the condition above,
      // so the no-children flag is always correct. We deal with contenteditable elsewhere.
      if (context.onSpellcheckable) {
        context.shouldInitInlineSpellCheckerUINoChildren = true;
      }

      context.onKeywordField = (editFlags & SpellCheckHelper.KEYWORD);
    } else if (context.target instanceof this.content.HTMLHtmlElement) {
      const bodyElt = context.target.ownerDocument.body;

      if (bodyElt) {
        let computedURL;

        try {
          computedURL = this._getComputedURL(bodyElt, "background-image");
          context.hasMultipleBGImages = false;
        } catch (e) {
          context.hasMultipleBGImages = true;
        }

        if (computedURL) {
          context.hasBGImage = true;
          context.bgImageURL = this._makeURLAbsolute(bodyElt.baseURI,
                                                    computedURL);
        }
      }
    } else if ((context.target instanceof this.content.HTMLEmbedElement ||
               context.target instanceof this.content.HTMLObjectElement) &&
               context.target.displayedType == this.content.HTMLObjectElement.TYPE_NULL &&
               context.target.pluginFallbackType == this.content.HTMLObjectElement.PLUGIN_CLICK_TO_PLAY) {
      context.onCTPPlugin = true;
    }

    context.canSpellCheck = this._isSpellCheckEnabled(context.target);
  }

  /**
   * Sets up the parts of the context menu for when when nodes have children.
   *
   * @param {Integer} editFlags The edit flags for the node. See SpellCheckHelper
   *                            for the details.
   */
  _setContextForNodesWithChildren(editFlags) {
    const context = this.context;

    // Second, bubble out, looking for items of interest that can have childen.
    // Always pick the innermost link, background image, etc.
    let elem = context.target;

    while (elem) {
      if (elem.nodeType == elem.ELEMENT_NODE) {
        // Link?
        const XLINK_NS = "http://www.w3.org/1999/xlink";

        if (!context.onLink &&
            // Be consistent with what hrefAndLinkNodeForClickEvent
            // does in browser.js
            (this._isXULTextLinkLabel(elem) ||
            (elem instanceof this.content.HTMLAnchorElement && elem.href) ||
            (elem instanceof this.content.SVGAElement &&
            (elem.href || elem.hasAttributeNS(XLINK_NS, "href"))) ||
            (elem instanceof this.content.HTMLAreaElement && elem.href) ||
            elem instanceof this.content.HTMLLinkElement ||
            elem.getAttributeNS(XLINK_NS, "type") == "simple")) {

          // Target is a link or a descendant of a link.
          context.onLink = true;

          // Remember corresponding element.
          context.link = elem;
          context.linkURL = this._getLinkURL();
          context.linkURI = this._getLinkURI();
          context.linkTextStr = this._getLinkText();
          context.linkProtocol = this._getLinkProtocol();
          context.onMailtoLink = (context.linkProtocol == "mailto");
          context.onMozExtLink = (context.linkProtocol == "moz-extension");
          context.onSaveableLink = this._isLinkSaveable(context.link);
          context.linkHasNoReferrer = BrowserUtils.linkHasNoReferrer(elem);

          try {
            if (elem.download) {
              // Ignore download attribute on cross-origin links
              context.principal.checkMayLoad(context.linkURI, false, true);
              context.linkDownload = elem.download;
            }
          } catch (ex) {}
        }

        // Background image?  Don't bother if we've already found a
        // background image further down the hierarchy.  Otherwise,
        // we look for the computed background-image style.
        if (!context.hasBGImage &&
            !context.hasMultipleBGImages) {
          let bgImgUrl = null;

          try {
            bgImgUrl = this._getComputedURL(elem, "background-image");
            context.hasMultipleBGImages = false;
          } catch (e) {
            context.hasMultipleBGImages = true;
          }

          if (bgImgUrl) {
            context.hasBGImage = true;
            context.bgImageURL = this._makeURLAbsolute(elem.baseURI,
                                                      bgImgUrl);
          }
        }
      }

      elem = elem.parentNode;
    }

    // See if the user clicked in a frame.
    const docDefaultView = context.target.ownerGlobal;

    if (docDefaultView != docDefaultView.top) {
      context.inFrame = true;

      if (context.target.ownerDocument.isSrcdocDocument) {
          context.inSrcdocFrame = true;
      }
    }

    // if the document is editable, show context menu like in text inputs
    if (!context.onEditable) {
      if (editFlags & SpellCheckHelper.CONTENTEDITABLE) {
        // If this.onEditable is false but editFlags is CONTENTEDITABLE, then
        // the document itself must be editable.
        context.onTextInput       = true;
        context.onKeywordField    = false;
        context.onImage           = false;
        context.onLoadedImage     = false;
        context.onCompletedImage  = false;
        context.inFrame           = false;
        context.inSrcdocFrame     = false;
        context.hasBGImage        = false;
        context.isDesignMode      = true;
        context.onEditable        = true;
        context.onSpellcheckable  = true;
        context.shouldInitInlineSpellCheckerUIWithChildren = true;
      }
    }
  }
}
