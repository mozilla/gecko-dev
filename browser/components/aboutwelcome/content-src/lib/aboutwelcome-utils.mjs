/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// If the container has a "page" data attribute, then this is
// a Spotlight modal or Feature Callout. Otherwise, this is
// about:welcome and we should return the current page.
const page =
  document.querySelector(
    "#multi-stage-message-root.onboardingContainer[data-page]"
  )?.dataset.page || document.location.href;

export const AboutWelcomeUtils = {
  handleUserAction(action) {
    return window.AWSendToParent("SPECIAL_ACTION", action);
  },
  sendImpressionTelemetry(messageId, context) {
    window.AWSendEventTelemetry?.({
      event: "IMPRESSION",
      event_context: {
        ...context,
        page,
      },
      message_id: messageId,
    });
  },
  sendActionTelemetry(messageId, elementId, eventName = "CLICK_BUTTON") {
    const ping = {
      event: eventName,
      event_context: {
        source: elementId,
        page,
      },
      message_id: messageId,
    };
    window.AWSendEventTelemetry?.(ping);
  },
  sendDismissTelemetry(messageId, elementId) {
    // Don't send DISMISS telemetry in spotlight modals since they already send
    // their own equivalent telemetry.
    if (page !== "spotlight") {
      this.sendActionTelemetry(messageId, elementId, "DISMISS");
    }
  },
  async fetchFlowParams(metricsFlowUri) {
    let flowParams;
    try {
      const response = await fetch(metricsFlowUri, {
        credentials: "omit",
      });
      if (response.status === 200) {
        const { deviceId, flowId, flowBeginTime } = await response.json();
        flowParams = { deviceId, flowId, flowBeginTime };
      } else {
        console.error("Non-200 response", response);
      }
    } catch (e) {
      flowParams = null;
    }
    return flowParams;
  },
  sendEvent(type, detail) {
    document.dispatchEvent(
      new CustomEvent(`AWPage:${type}`, {
        bubbles: true,
        detail,
      })
    );
  },
  getLoadingStrategyFor(url) {
    return url?.startsWith("http") ? "lazy" : "eager";
  },
  handleCampaignAction(action, messageId) {
    window.AWSendToParent("HANDLE_CAMPAIGN_ACTION", action).then(handled => {
      if (handled) {
        this.sendActionTelemetry(messageId, "CAMPAIGN_ACTION");
      }
    });
  },
  getValidStyle(style, validStyles, allowVars) {
    if (!style) {
      return null;
    }
    return Object.keys(style)
      .filter(
        key => validStyles.includes(key) || (allowVars && key.startsWith("--"))
      )
      .reduce((obj, key) => {
        obj[key] = style[key];
        return obj;
      }, {});
  },
};
