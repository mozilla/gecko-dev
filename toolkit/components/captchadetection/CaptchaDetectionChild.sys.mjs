/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/** @type {lazy} */
const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "CaptchaDetectionChild",
    maxLogLevelPref: "captchadetection.loglevel",
  });
});

/**
 * Abstract class for handling captchas.
 */
class CaptchaHandler {
  /**
   * @param {CaptchaDetectionChild} actor - The window actor.
   * @param {Event} _event - The initial event that created the actor.
   */
  constructor(actor, _event) {
    /** @type {CaptchaDetectionChild} */
    this.actor = actor;
    this.tabId = this.actor.docShell.browserChild.tabId;
    this.isPBM = this.actor.browsingContext.usePrivateBrowsing;
  }

  static matches(_document) {
    throw new Error("abstract method");
  }

  updateState(state) {
    this.actor.sendAsyncMessage("CaptchaState:Update", {
      tabId: this.tabId,
      isPBM: this.isPBM,
      state,
    });
  }

  onActorDestroy() {
    lazy.console.debug("CaptchaHandler destroyed");
  }

  /**
   * @param {Event} event - The event to handle.
   */
  handleEvent(event) {
    lazy.console.debug("CaptchaHandler got event:", event);
  }

  /**
   * @param {ReceiveMessageArgument} message - The message to handle.
   */
  receiveMessage(message) {
    lazy.console.debug("CaptchaHandler got message:", message);
  }
}

/**
 * Handles Google Recaptcha v2 captchas.
 *
 * ReCaptcha v2 places two iframes in the page. One for the
 * challenge and one for the checkmark. This handler listens
 * for the checkmark being clicked or the challenge being
 * shown. When either of these events happen, the handler
 * sends a message to the parent actor. The handler then
 * disconnects the mutation observer to avoid further
 * processing.
 */
class GoogleRecaptchaV2Handler extends CaptchaHandler {
  #enabled;
  #mutationObserver;

  constructor(actor, event) {
    super(actor, event);
    this.#enabled = true;
    this.#mutationObserver = new this.actor.contentWindow.MutationObserver(
      this.#mutationHandler.bind(this)
    );
    this.#mutationObserver.observe(this.actor.document, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: ["style"],
    });
  }

  static matches(document) {
    if (Cu.isInAutomation) {
      return (
        document
          .getElementById("captchaType")
          ?.getAttribute("data-captcha-type") === "g-recaptcha-v2"
      );
    }

    return [
      "https://www.google.com/recaptcha/api2/",
      "https://www.google.com/recaptcha/enterprise/",
    ].some(match => document.location.href.startsWith(match));
  }

  #mutationHandler(_mutations, observer) {
    if (!this.#enabled) {
      return;
    }

    const token = this.actor.document.getElementById("recaptcha-token");
    const initialized = token && token.value !== "";
    if (!initialized) {
      return;
    }

    const checkmark = this.actor.document.getElementById("recaptcha-anchor");
    if (checkmark && checkmark.ariaChecked === "true") {
      this.updateState({
        type: "g-recaptcha-v2",
        changes: "GotCheckmark",
      });
      this.#enabled = false;
      observer.disconnect();
      return;
    }

    const images = this.actor.document.getElementById("rc-imageselect");
    if (images) {
      this.updateState({
        type: "g-recaptcha-v2",
        changes: "ImagesShown",
      });
      this.#enabled = false;
      observer.disconnect();
    }
  }

  onActorDestroy() {
    super.onActorDestroy();
    this.#mutationObserver.disconnect();
  }
}

/**
 * Handles Cloudflare Turnstile captchas.
 *
 * Cloudflare Turnstile captchas have a success and fail div
 * that are displayed when the captcha is completed. This
 * handler listens for the success or fail div to be displayed
 * and sends a message to the parent actor when either of
 * these events happen. The handler then disconnects the
 * mutation observer to avoid further processing.
 * We use two mutation observers to detect shadowroot
 * creation and then observe the shadowroot for the success
 * or fail div.
 */
class CFTurnstileHandler extends CaptchaHandler {
  #observingShadowRoot;
  #mutationObserver;

  constructor(actor, event) {
    super(actor, event);
    this.#observingShadowRoot = false;
    if (this.actor.document.body?.openOrClosedShadowRoot) {
      this.#observeShadowRoot(this.actor.document.body.openOrClosedShadowRoot);
      return;
    }
    this.#mutationObserver = new this.actor.contentWindow.MutationObserver(
      this.#mutationHandler.bind(this)
    );
    this.#mutationObserver.observe(this.actor.document.documentElement, {
      attributes: true,
    });
  }

  static matches(document) {
    if (Cu.isInAutomation) {
      return (
        document
          .getElementById("captchaType")
          ?.getAttribute("data-captcha-type") === "cf-turnstile"
      );
    }

    return (
      document.location.href.includes("/cdn-cgi/challenge-platform/") &&
      document.location.href.includes("/turnstile/if/ov2")
    );
  }

  #mutationHandler(_mutations, observer) {
    lazy.console.debug(_mutations);
    if (this.#observingShadowRoot) {
      return;
    }

    const shadowRoot = this.actor.document.body?.openOrClosedShadowRoot;
    if (!shadowRoot) {
      return;
    }
    observer.disconnect();
    lazy.console.debug("Found shadowRoot", shadowRoot);

    this.#observeShadowRoot(shadowRoot);
  }

  #observeShadowRoot(shadowRoot) {
    if (this.#observingShadowRoot) {
      return;
    }
    this.#observingShadowRoot = true;

    this.#mutationObserver = new this.actor.contentWindow.MutationObserver(
      (_mutations, observer) => {
        const fail = shadowRoot.getElementById("fail");
        const success = shadowRoot.getElementById("success");
        if (!fail || !success) {
          return;
        }

        if (fail.style.display !== "none") {
          lazy.console.debug("Captcha failed");
          this.updateState({
            type: "cf-turnstile",
            result: "Failed",
          });
          observer.disconnect();
          return;
        }

        if (success.style.display !== "none") {
          lazy.console.debug("Captcha succeeded");
          this.updateState({
            type: "cf-turnstile",
            result: "Succeeded",
          });
          observer.disconnect();
        }
      }
    ).observe(shadowRoot, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: ["style"],
    });
  }

  onActorDestroy() {
    super.onActorDestroy();
    this.#mutationObserver.disconnect();
  }
}

/**
 * Handles Datadome captchas.
 *
 * Datadome works by placing an iframe that postMessages to
 * the parent window. This actor attaches to the iframe
 * that posts messages to the parent window. Therefore, we
 * ask the parent actor to initialize a new actor in the
 * parent window. That actor will then listen for messages
 * from the iframe and send a message to the parent actor
 * when the captcha is completed.
 */
class DatadomeHandler extends CaptchaHandler {
  constructor(actor, event) {
    super(actor, event);

    event.stopImmediatePropagation();

    this.actor
      .sendQuery("CaptchaDetection:Init", { type: "datadome" })
      .then(() => {
        // re-dispatch the event
        event.target.dispatchEvent(event);
      });
  }

  static matches(document) {
    if (Cu.isInAutomation) {
      return (
        document
          .getElementById("captchaType")
          ?.getAttribute("data-captcha-type") === "datadome"
      );
    }

    return document.location.href.startsWith(
      "https://geo.captcha-delivery.com/captcha/"
    );
  }
}

/**
 * Handles hCaptcha captchas.
 *
 * hCaptcha works by placing two iframes in the page. One for
 * the challenge and one for the checkbox. This handler listens
 * for the challenge being shown or the checkbox being checked.
 * When either of these events happen, the handler sends a
 * message to the parent actor. The handler then disconnects
 * the mutation observer to avoid further processing.
 */
class HCaptchaHandler extends CaptchaHandler {
  #shown;
  #checked;
  #mutationObserver;

  constructor(actor, event) {
    super(actor, event);

    let params = null;
    try {
      params = new URLSearchParams(this.actor.document.location.hash.slice(1));
    } catch {
      // invalid URL
      return;
    }

    const frameType = params.get("frame");
    if (frameType === "challenge") {
      this.#initChallengeHandler();
    } else if (frameType === "checkbox") {
      this.#initCheckboxHandler();
    }
  }

  static matches(document) {
    if (Cu.isInAutomation) {
      return (
        document
          .getElementById("captchaType")
          ?.getAttribute("data-captcha-type") === "hCaptcha"
      );
    }

    return (
      document.location.href.startsWith(
        "https://newassets.hcaptcha.com/captcha/v1/"
      ) && document.location.href.includes("/static/hcaptcha.html")
    );
  }

  #initChallengeHandler() {
    this.#shown = false;
    this.#mutationObserver = new this.actor.contentWindow.MutationObserver(
      this.#challengeMutationHandler.bind(this)
    );
    this.#mutationObserver.observe(this.actor.document.body, {
      attributes: true,
      attributeFilter: ["aria-hidden"],
    });
  }

  #challengeMutationHandler(_mutations, observer) {
    if (this.#shown) {
      return;
    }

    this.#shown = this.actor.document.body.ariaHidden !== "true";
    if (!this.#shown) {
      return;
    }

    this.updateState({
      type: "hCaptcha",
      changes: "shown",
    });
    observer.disconnect();
  }

  #initCheckboxHandler() {
    this.#checked = false;
    this.#mutationObserver = new this.actor.contentWindow.MutationObserver(
      this.#checkboxMutationHandler.bind(this)
    );
    this.#mutationObserver.observe(this.actor.document, {
      subtree: true,
      attributes: true,
      attributeFilter: ["aria-checked"],
    });
  }

  #checkboxMutationHandler(_mutations, observer) {
    if (this.#checked) {
      return;
    }

    const checkbox = this.actor.document.getElementById("checkbox");
    if (checkbox?.ariaChecked === "true") {
      this.#checked = true;
      this.updateState({
        type: "hCaptcha",
        changes: "passed",
      });
      observer.disconnect();
    }
  }

  onActorDestroy() {
    super.onActorDestroy();
    this.#mutationObserver.disconnect();
  }
}

/**
 * Handles AWS WAF captchas.
 *
 * To detect the state of AWS WAF captchas, we listen for
 * network requests to the captcha API. When the response
 * is received, we check for the presence of the expected
 * keys and send a message to the parent actor with the
 * state of the captcha.
 */
class AWSWafHandler extends CaptchaHandler {
  constructor(actor, event) {
    super(actor, event);

    this.actor.sendAsyncMessage("CaptchaDetection:Init", { type: "awsWaf" });
    this.updateState({
      type: "awsWaf",
      changes: "shown",
    });
  }

  static matches(document) {
    if (Cu.isInAutomation) {
      return (
        document
          .getElementById("captchaType")
          ?.getAttribute("data-captcha-type") === "awsWaf"
      );
    }

    if (
      !document.location.pathname == "/latest" ||
      !document.location.hostname.endsWith(".amazonaws.com") ||
      !document.location.hostname.includes(".execute-api.")
    ) {
      return false;
    }

    if (!document.getElementById("captcha-container")) {
      return false;
    }

    // Query selector is only run after checking URL and #captcha-container.
    // It is already highly likely that we are on a AWS WAF page,
    // but we want to be extra sure.
    return (
      document.head?.querySelectorAll(
        "script[src*='.token.awswaf.com'][src*='/challenge.js'], script[src*='.captcha.awswaf.com'][src*='/captcha.js']"
      ).length === 2
    );
  }
}

/**
 * Handles Arkose Labs captchas.
 *
 * Similar to AWS WAF, we listen for network requests to the
 * captcha API. When the response is received, we check for
 * the presence of the expected keys and send a message to
 * the parent actor with the state of the captcha.
 */
class ArkoseLabsHandler extends CaptchaHandler {
  constructor(actor) {
    super(actor);
    this.actor.sendAsyncMessage("CaptchaDetection:Init", {
      type: "arkoseLabs",
    });
  }

  static matches(document) {
    return document.location.href.startsWith(
      "https://client-api.arkoselabs.com/fc/assets/ec-game-core/game-core/"
    );
  }
}

/**
 * This actor runs in the captcha's frame. It provides information
 * about the captcha's state to the parent actor.
 */
export class CaptchaDetectionChild extends JSWindowActorChild {
  actorCreated() {
    lazy.console.debug("actorCreated");
  }

  /**
   * @constant
   * @type {Array<CaptchaHandler>}
   */
  static #handlers = [
    GoogleRecaptchaV2Handler,
    CFTurnstileHandler,
    DatadomeHandler,
    HCaptchaHandler,
    AWSWafHandler,
    ArkoseLabsHandler,
  ];

  /**
   * @param {Event} event - The event that created the actor.
   */
  #initCaptchaHandler(event) {
    for (const handler of CaptchaDetectionChild.#handlers) {
      if (handler.matches(this.document)) {
        this.handler = new handler(this, event);
        return;
      }
    }
  }

  actorDestroy() {
    lazy.console.debug("actorDestroy()");
    this.handler?.onActorDestroy();
  }

  /**
   * @param {Event} event - The event to handle.
   */
  handleEvent(event) {
    if (
      !this.handler &&
      (event.type === "DOMContentLoaded" || event.type === "pageshow")
    ) {
      this.#initCaptchaHandler(event);
      return;
    }

    if (event.type === "pagehide") {
      this.sendAsyncMessage("TabState:Closed", {
        tabId: this.docShell.browserChild.tabId,
      });
    }

    this.handler?.handleEvent(event);
  }

  /**
   * @param {ReceiveMessageArgument} message - The message to handle.
   */
  receiveMessage(message) {
    if (this.handler) {
      this.handler.receiveMessage(message);
    }
  }
}

/**
 * @typedef lazy
 * @type {object}
 * @property {ConsoleInstance} console - console instance.
 */
