/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "CaptchaDetectionParent",
    maxLogLevelPref: "captchadetection.loglevel",
  });
});

ChromeUtils.defineESModuleGetters(lazy, {
  CaptchaDetectionPingUtils:
    "resource://gre/modules/CaptchaDetectionPingUtils.sys.mjs",
  CaptchaResponseObserver:
    "resource://gre/modules/CaptchaResponseObserver.sys.mjs",
});

/**
 * Holds the state of each tab.
 * The state is an object with the following structure:
 * [key: tabId]: typeof ReturnType<TabState.#defaultValue()>
 */
class TabState {
  #state;

  constructor() {
    this.#state = new Map();
  }

  get(tabId) {
    return this.#state.get(tabId);
  }

  static #defaultValue() {
    return new Map();
  }

  update(tabId, updateFunction) {
    if (!this.#state.has(tabId)) {
      this.#state.set(tabId, TabState.#defaultValue());
    }
    updateFunction(this.#state.get(tabId));
  }

  clear(tabId) {
    this.#state.delete(tabId);
  }
}

const tabState = new TabState();

/**
 * This actor parent is responsible for recording the state of captchas
 * or communicating with parent browsing context.
 */
class CaptchaDetectionParent extends JSWindowActorParent {
  #responseObserver;

  actorCreated() {
    lazy.console.debug("actorCreated");
  }

  actorDestroy() {
    lazy.console.debug("actorDestroy()");

    if (this.#responseObserver) {
      this.#responseObserver.unregister();
    }
  }

  #updateGRecaptchaV2State({ tabId, isPBM, state: { type, changes } }) {
    lazy.console.debug("updateGRecaptchaV2State", changes);

    if (changes === "ImagesShown") {
      tabState.update(tabId, state => {
        state.set(type + changes, true);
      });

      // We don't call maybeSubmitPing here because we might end up
      // submitting the ping without the "GotCheckmark" event.
      // maybeSubmitPing will be called when "GotCheckmark" event is
      // received, or when the daily maybeSubmitPing is called.
      const shownMetric = "googleRecaptchaV2Ps" + (isPBM ? "Pbm" : "");
      Glean.captchaDetection[shownMetric].add(1);
    } else if (changes === "GotCheckmark") {
      const autoCompleted = !tabState.get(tabId)?.has(type + "ImagesShown");
      lazy.console.debug(
        "GotCheckmark" +
          (autoCompleted ? " (auto-completed)" : " (manually-completed)")
      );
      const resultMetric =
        "googleRecaptchaV2" +
        (autoCompleted ? "Ac" : "Pc") +
        (isPBM ? "Pbm" : "");
      Glean.captchaDetection[resultMetric].add(1);
      lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
    }
  }

  #recordCFTurnstileResult({ isPBM, state: { result } }) {
    lazy.console.debug("recordCFTurnstileResult", result);
    const resultMetric =
      "cloudflareTurnstile" +
      (result === "Succeeded" ? "Cc" : "Cf") +
      (isPBM ? "Pbm" : "");
    Glean.captchaDetection[resultMetric].add(1);
    lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
  }

  async #datadomeInit() {
    const parent = this.browsingContext.parentWindowContext;
    if (!parent) {
      lazy.console.error("Datadome captcha loaded in a top-level window?");
      return;
    }

    let actor = null;
    try {
      actor = parent.getActor("CaptchaDetectionCommunication");
      if (!actor) {
        lazy.console.error("CaptchaDetection actor not found in parent window");
        return;
      }
    } catch (e) {
      lazy.console.error("Error getting actor", e);
      return;
    }

    await actor.sendQuery("Datadome:AddMessageListener");
  }

  #recordDatadomeEvent({ isPBM, state: { event, ...payload } }) {
    lazy.console.debug("recordDatadomeEvent", event, payload);
    const suffix = isPBM ? "Pbm" : "";
    if (event === "load") {
      if (payload.captchaShown) {
        Glean.captchaDetection["datadomePs" + suffix].add(1);
      } else if (payload.blocked) {
        Glean.captchaDetection["datadomeBl" + suffix].add(1);
      }
    } else if (event === "passed") {
      Glean.captchaDetection["datadomePc" + suffix].add(1);
    }

    lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
  }

  #recordHCaptchaState({ isPBM, tabId, state: { type, changes } }) {
    lazy.console.debug("recordHCaptchaEvent", changes);

    if (changes === "shown") {
      // I don't think HCaptcha supports auto-completion, but we act
      // as if it does just in case.
      tabState.update(tabId, state => {
        state.set(type + changes, true);
      });

      // We don't call maybeSubmitPing here because we might end up
      // submitting the ping without the "passed" event.
      // maybeSubmitPing will be called when "passed" event is
      // received, or when the daily maybeSubmitPing is called.
      const shownMetric = "hcaptchaPs" + (isPBM ? "Pbm" : "");
      Glean.captchaDetection[shownMetric].add(1);
    } else if (changes === "passed") {
      const autoCompleted = !tabState.get(tabId)?.has(type + "shown");
      const resultMetric =
        "hcaptcha" + (autoCompleted ? "Ac" : "Pc") + (isPBM ? "Pbm" : "");
      Glean.captchaDetection[resultMetric].add(1);
      lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
    }
  }

  #recordAWSWafEvent({
    isPBM,
    state: { event, success, numSolutionsRequired },
  }) {
    if (event === "shown") {
      // We don't call maybeSubmitPing here because we might end up
      // submitting the ping without the "completed" event.
      // maybeSubmitPing will be called when "completed" event is
      // received, or when the daily maybeSubmitPing is called.
      const shownMetric = "awswafPs" + (isPBM ? "Pbm" : "");
      Glean.captchaDetection[shownMetric].add(1);
    } else if (event === "completed") {
      const suffix = isPBM ? "Pbm" : "";
      const resultMetric = "awswaf" + (success ? "Pc" : "Pf") + suffix;
      Glean.captchaDetection[resultMetric].add(1);

      const solutionsRequiredMetric =
        Glean.captchaDetection["awswafSolutionsRequired" + suffix];
      solutionsRequiredMetric.accumulateSingleSample(numSolutionsRequired);

      lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
    }
  }

  #recordArkoseLabsEvent({
    isPBM,
    state: { event, solved, solutionsSubmitted },
  }) {
    if (event === "shown") {
      // We don't call maybeSubmitPing here because we might end up
      // submitting the ping without the "completed" event.
      // maybeSubmitPing will be called when "completed" event is
      // received, or when the daily maybeSubmitPing is called.
      const shownMetric = "arkoselabsPs" + (isPBM ? "Pbm" : "");
      Glean.captchaDetection[shownMetric].add(1);
    } else if (event === "completed") {
      const suffix = isPBM ? "Pbm" : "";
      const resultMetric = "arkoselabs" + (solved ? "Pc" : "Pf") + suffix;
      Glean.captchaDetection[resultMetric].add(1);

      const solutionsRequiredMetric =
        Glean.captchaDetection["arkoselabsSolutionsRequired" + suffix];
      solutionsRequiredMetric.accumulateSingleSample(solutionsSubmitted);

      lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
    }
  }

  async #awsWafInit() {
    this.#responseObserver = new lazy.CaptchaResponseObserver(
      channel =>
        channel.loadInfo?.browsingContextID === this.browsingContext.id &&
        channel.URI &&
        channel.URI.scheme === "https" &&
        channel.URI.host.endsWith(".captcha.awswaf.com") &&
        channel.URI.filePath.endsWith("/verify"),
      (_channel, statusCode, responseBody) => {
        if (statusCode !== Cr.NS_OK) {
          return;
        }

        let body;
        try {
          body = JSON.parse(responseBody);
          if (!body) {
            lazy.console.debug("handleResponseBody", "Failed to parse JSON");
            return;
          }
        } catch (e) {
          lazy.console.debug(
            "handleResponseBody",
            "Failed to parse JSON",
            e,
            responseBody
          );
          return;
        }

        // Check for the presence of the expected keys
        if (
          !["success", "num_solutions_required"].every(key =>
            body.hasOwnProperty(key)
          )
        ) {
          lazy.console.debug("handleResponseBody", "Missing keys", body);
          return;
        }

        this.#recordAWSWafEvent({
          isPBM: this.browsingContext.usePrivateBrowsing,
          state: {
            event: "completed",
            success: body.success,
            numSolutionsRequired: body.num_solutions_required,
          },
        });
      }
    );
    this.#responseObserver.register();
  }

  async #arkoseLabsInit() {
    let solutionsSubmitted = 0;
    this.#responseObserver = new lazy.CaptchaResponseObserver(
      channel =>
        channel.loadInfo?.browsingContextID === this.browsingContext.id &&
        channel.URI &&
        channel.URI.spec === "https://client-api.arkoselabs.com/fc/ca/",
      (_channel, statusCode, responseBody) => {
        if (statusCode !== Cr.NS_OK) {
          return;
        }

        let body;
        try {
          body = JSON.parse(responseBody);
          if (!body) {
            lazy.console.debug(
              "ResponseObserver:ResponseBody",
              "Failed to parse JSON"
            );
            return;
          }
        } catch (e) {
          lazy.console.debug(
            "ResponseObserver:ResponseBody",
            "Failed to parse JSON",
            e,
            responseBody
          );
          return;
        }

        // Check for the presence of the expected keys
        if (["response", "solved"].some(key => !body.hasOwnProperty(key))) {
          lazy.console.debug(
            "ResponseObserver:ResponseBody",
            "Missing keys",
            body
          );
          return;
        }

        solutionsSubmitted++;
        if (body.solved === null) {
          return;
        }

        this.#recordArkoseLabsEvent({
          isPBM: this.browsingContext.usePrivateBrowsing,
          state: {
            event: "completed",
            solved: body.solved,
            solutionsSubmitted,
          },
        });

        solutionsSubmitted = 0;
      }
    );
    this.#responseObserver.register();
  }

  #onTabClosed(tabId) {
    tabState.clear(tabId);

    if (this.#responseObserver) {
      this.#responseObserver.unregister();
    }
  }

  async receiveMessage(message) {
    lazy.console.debug("receiveMessage", message);

    switch (message.name) {
      case "CaptchaState:Update":
        // message.name === "CaptchaState:Update"
        // => message.data = {
        //   tabId: number,
        //   isPBM: boolean,
        //   state: {
        //     type: string,
        //     ...<type specific payload>
        //   }
        // }
        switch (message.data.state.type) {
          case "g-recaptcha-v2":
            this.#updateGRecaptchaV2State(message.data);
            break;
          case "cf-turnstile":
            this.#recordCFTurnstileResult(message.data);
            break;
          case "datadome":
            this.#recordDatadomeEvent(message.data);
            break;
          case "hCaptcha":
            this.#recordHCaptchaState(message.data);
            break;
          case "awsWaf":
            this.#recordAWSWafEvent(message.data);
            break;
        }
        break;
      case "TabState:Closed":
        // message.name === "TabState:Closed"
        // => message.data = {
        //   tabId: number,
        // }
        this.#onTabClosed(message.data.tabId);
        break;
      case "CaptchaDetection:Init":
        // message.name === "CaptchaDetection:Init"
        // => message.data = {
        //   type: string,
        // }
        switch (message.data.type) {
          case "datadome":
            return this.#datadomeInit();
          case "awsWaf":
            return this.#awsWafInit();
          case "arkoseLabs":
            return this.#arkoseLabsInit();
        }
        break;
      default:
        lazy.console.error("Unknown message", message);
    }
    return null;
  }
}

export {
  CaptchaDetectionParent,
  CaptchaDetectionParent as CaptchaDetectionCommunicationParent,
};
