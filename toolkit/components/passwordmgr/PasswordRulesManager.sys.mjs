/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Logic: "resource://gre/modules/LoginManager.shared.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  PasswordGenerator: "resource://gre/modules/shared/PasswordGenerator.sys.mjs",
  PasswordRulesParser:
    "resource://gre/modules/shared/PasswordRulesParser.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let logger = lazy.LoginHelper.createLogger("PasswordRulesManager");
  return logger.log.bind(logger);
});

const IMPROVED_PASSWORD_GENERATION_HISTOGRAM =
  "PWMGR_NUM_IMPROVED_GENERATED_PASSWORDS";

/**
 * Handles interactions between PasswordRulesParser and the "password-rules" Remote Settings collection
 *
 * @class PasswordRulesManagerParent
 * @extends {JSWindowActorParent}
 */
export class PasswordRulesManagerParent extends JSWindowActorParent {
  /**
   * @type RemoteSettingsClient
   *
   * @memberof PasswordRulesManagerParent
   */
  _passwordRulesClient = null;

  async initPasswordRulesCollection() {
    if (!this._passwordRulesClient) {
      this._passwordRulesClient = lazy.RemoteSettings(
        lazy.LoginHelper.improvedPasswordRulesCollection
      );
    }
  }

  /**
   * Generates a password based on rules from the origin parameters.
   * @param {nsIURI} uri
   * @return {string} password
   * @memberof PasswordRulesManagerParent
   */
  async generatePassword(uri, { inputMaxLength } = {}) {
    await this.initPasswordRulesCollection();
    let originDisplayHost = uri.displayHost;
    let records = await this._passwordRulesClient.get();
    let currentRecord;
    for (let record of records) {
      if (Services.eTLD.hasRootDomain(originDisplayHost, record.Domain)) {
        currentRecord = record;
        break;
      }
    }
    let isCustomRule = false;
    // If we found a matching result, use that to generate a stronger password.
    // Otherwise, generate a password using the default rules set.
    if (currentRecord?.Domain) {
      isCustomRule = true;
      lazy.log(
        `Password rules for ${currentRecord.Domain}:  ${currentRecord["password-rules"]}.`
      );
      let currentRules = lazy.PasswordRulesParser.parsePasswordRules(
        currentRecord["password-rules"]
      );
      let mapOfRules = lazy.Logic.transformRulesToMap(currentRules);
      Services.telemetry
        .getHistogramById(IMPROVED_PASSWORD_GENERATION_HISTOGRAM)
        .add(isCustomRule);
      return lazy.PasswordGenerator.generatePassword({
        rules: mapOfRules,
        inputMaxLength,
      });
    }
    lazy.log(
      `No password rules for specified origin, generating standard password.`
    );
    Services.telemetry
      .getHistogramById(IMPROVED_PASSWORD_GENERATION_HISTOGRAM)
      .add(isCustomRule);
    return lazy.PasswordGenerator.generatePassword({ inputMaxLength });
  }
}
