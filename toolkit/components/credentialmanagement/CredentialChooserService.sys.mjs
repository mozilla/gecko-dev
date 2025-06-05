/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * Class implementing the nsICredentialChooserService.
 *
 * This class shows UI to the user for the Credential Chooser for the
 * Credential Management API.
 *
 * @class CredentialChooserService
 */
export class CredentialChooserService {
  classID = Components.ID("{673ddc19-03e2-4b30-a868-06297e8fed89}");
  QueryInterface = ChromeUtils.generateQI(["nsICredentialChooserService"]);

  /**
   * A service function to help any UI. Fetches and serializes images to
   * data urls, which can be used in chrome UI.
   *
   * @param {Window} window - Window which should perform the fetch
   * @param {nsIURI} uri - Icon location to be fetched from
   * @returns {Promise<string, Error>} The data URI encoded as a string representing the icon that was loaded
   */
  async fetchImageToDataURI(window, uri) {
    if (uri.protocol === "data:") {
      return uri.href;
    }
    let request = new window.Request(uri.spec, { mode: "cors" });
    request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_IMAGE);
    let blob;
    let response = await window.fetch(request);
    if (!response.ok) {
      return Promise.reject(new Error("HTTP failure on Fetch"));
    }
    blob = await response.blob();
    return new Promise((resolve, reject) => {
      let reader = new FileReader();
      reader.onloadend = () => resolve(reader.result);
      reader.onerror = reject;
      reader.readAsDataURL(blob);
    });
  }

  /**
   * A helper function that performs precisely the right Fetch for the well-known resource for FedCM.
   *
   * @param {nsIURI} uri - Well known resource location
   * @param {nsIPrincipal} triggeringPrincipal - Principal of the IDP triggering this request
   * @returns {Promise} A promise that will be the result of fetching the resource and parsing the body as JSON,
   *          or reject along the way.
   */
  async fetchWellKnown(uri, triggeringPrincipal) {
    let request = new Request(uri.spec, {
      mode: "no-cors",
      referrerPolicy: "no-referrer",
      // We use a Null Principal here because we don't want to send any
      // cookies or an Origin header here before we get user permission.
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        triggeringPrincipal.originAttributes
      ),
      // and we want to be able to read the response, so we don't let CORS hide it
      // because we are no-cors
      neverTaint: true,
      credentials: "omit",
      headers: [["Accept", "application/json"]],
    });
    // This overrides the Sec-Fetch-Dest to `webidentity` rather than `empty`
    request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_WEB_IDENTITY);
    let response = await fetch(request);
    return response.json();
  }

  /**
   * A helper function that performs precisely the right Fetch for the IDP configuration resource for FedCM.
   *
   * @param {nsIURI} uri - Well known resource location
   * @param {nsIPrincipal} triggeringPrincipal - Principal of the IDP triggering this request
   * @returns {Promise} A promise that will be the result of fetching the resource and parsing the body as JSON,
   *          or reject along the way.
   */
  async fetchConfig(uri, triggeringPrincipal) {
    let request = new Request(uri.spec, {
      mode: "no-cors",
      referrerPolicy: "no-referrer",
      redirect: "error",
      // We use a Null Principal here because we don't want to send any
      // cookies or an Origin header here before we get user permission.
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        triggeringPrincipal.originAttributes
      ),
      // and we want to be able to read the response, so we don't let CORS hide it
      // because we are no-cors
      neverTaint: true,
      credentials: "omit",
      headers: [["Accept", "application/json"]],
    });
    // This overrides the Sec-Fetch-Dest to `webidentity` rather than `empty`
    request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_WEB_IDENTITY);
    let response = await fetch(request);
    return response.json();
  }

  /**
   * A helper function that performs precisely the right Fetch for the account list for FedCM.
   *
   * @param {nsIURI} uri - Well known resource location
   * @param {nsIPrincipal} triggeringPrincipal - Principal of the IDP triggering this request
   * @returns {Promise} A promise that will be the result of fetching the resource and parsing the body as JSON,
   *          or reject along the way.
   */
  async fetchAccounts(uri, triggeringPrincipal) {
    let request = new Request(uri.spec, {
      mode: "no-cors",
      redirect: "error",
      referrerPolicy: "no-referrer",
      triggeringPrincipal,
      // and we want to be able to read the response, so we don't let CORS hide it
      // because we are no-cors
      neverTaint: true,
      credentials: "include",
      headers: [["Accept", "application/json"]],
    });
    // This overrides the Sec-Fetch-Dest to `webidentity` rather than `empty`
    request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_WEB_IDENTITY);
    let response = await fetch(request);
    return response.json();
  }

  /**
   * A helper function that performs precisely the right Fetch for the token request for FedCM.
   *
   * @param {nsIURI} uri - Well known resource location
   * @param {string} body - Body to be sent with the fetch, pre-serialized.
   * @param {nsIPrincipal} triggeringPrincipal - Principal of the IDP triggering this request
   * @returns {Promise} A promise that will be the result of fetching the resource and parsing the body as JSON,
   *          or reject along the way.
   */
  async fetchToken(uri, body, triggeringPrincipal) {
    let request = new Request(uri.spec, {
      mode: "cors",
      method: "POST",
      redirect: "error",
      triggeringPrincipal,
      body,
      credentials: "include",
      headers: [["Content-type", "application/x-www-form-urlencoded"]],
    });
    // This overrides the Sec-Fetch-Dest to `webidentity` rather than `empty`
    request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_WEB_IDENTITY);
    let response = await fetch(request);
    return response.json();
  }

  /**
   * A helper function that performs precisely the right Fetch for the disconnect request for FedCM.
   *
   * @param {nsIURI} uri - Well known resource location
   * @param {string} body - Body to be sent with the fetch, pre-serialized.
   * @param {nsIPrincipal} triggeringPrincipal - Principal of the IDP triggering this request
   * @returns {Promise} A promise that will be the result of fetching the resource and parsing the body as JSON,
   *          or reject along the way.
   */
  async fetchDisconnect(uri, body, triggeringPrincipal) {
    let request = new Request(uri.spec, {
      mode: "cors",
      method: "POST",
      redirect: "error",
      triggeringPrincipal,
      body,
      credentials: "include",
      headers: [["Content-type", "application/x-www-form-urlencoded"]],
    });
    // This overrides the Sec-Fetch-Dest to `webidentity` rather than `empty`
    request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_WEB_IDENTITY);
    let response = await fetch(request);
    return response.json();
  }
}
