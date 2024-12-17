/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

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
  constructor(actor) {
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

  handleEvent(event) {
    lazy.console.debug("CaptchaHandler got event:", event);
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

  constructor(actor) {
    super(actor);
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

  constructor(actor) {
    super(actor);
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
 * This actor runs in the captcha's frame. It provides information
 * about the captcha's state to the parent actor.
 */
export class CaptchaDetectionChild extends JSWindowActorChild {
  actorCreated() {
    lazy.console.debug("actorCreated");
  }

  static #handlers = [GoogleRecaptchaV2Handler, CFTurnstileHandler];

  #initCaptchaHandler() {
    for (const handler of CaptchaDetectionChild.#handlers) {
      if (handler.matches(this.document)) {
        this.handler = new handler(this);
        return;
      }
    }
  }

  actorDestroy() {
    lazy.console.debug("actorDestroy()");
    this.handler?.onActorDestroy();
  }

  handleEvent(event) {
    if (
      !this.handler &&
      (event.type === "DOMContentLoaded" || event.type === "pageshow")
    ) {
      this.#initCaptchaHandler();
      return;
    }

    if (event.type === "pagehide") {
      this.sendAsyncMessage("TabState:Closed", {
        tabId: this.docShell.browserChild.tabId,
      });
    }

    this.handler?.handleEvent(event);
  }
}
