/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The selectable profile
 */
export class SelectableProfile {
  // DB internal autoincremented integer ID.
  // eslint-disable-next-line no-unused-private-class-members
  #id;

  // Path to profile on disk.
  #path;

  // The user-editable name
  #name;

  // Name of the user's chosen avatar, which corresponds to a list of built-in
  // SVG avatars.
  #avatar;

  // Cached theme properties, used to allow displaying a SelectableProfile
  // without loading the AddonManager to get theme info.
  #themeL10nId;
  #themeFg;
  #themeBg;

  constructor(row) {
    this.#id = row.getResultByName("id");
    this.#path = row.getResultByName("path");
    this.#name = row.getResultByName("name");
    this.#avatar = row.getResultByName("avatar");
    this.#themeL10nId = row.getResultByName("themeL10nId");
    this.#themeFg = row.getResultByName("themeFg");
    this.#themeBg = row.getResultByName("themeBg");
  }

  /**
   * Get the id of the profile.
   *
   * @returns {number} Id of profile
   */
  get id() {
    return this.#id;
  }

  // Note: setters update the object, then ask the SelectableProfileService to save it.

  /**
   * Get the user-editable name for the profile.
   *
   * @returns {string} Name of profile
   */
  get name() {
    return this.#name;
  }

  /**
   * Update the user-editable name for the profile, then trigger saving the profile,
   * which will notify() other running instances.
   *
   * @param {string} aName The new name of the profile
   */
  set name(aName) {
    this.#name = aName;

    this.saveUpdatesToDB();
  }

  /**
   * Get the full path to the profile as a string.
   *
   * @returns {string} Path of profile
   */
  get path() {
    return PathUtils.joinRelative(
      Services.dirsvc.get("UAppData", Ci.nsIFile).path,
      this.#path
    );
  }

  /**
   * Get the profile directory as an  nsIFile.
   *
   * @returns {Promise<nsIFile>} A promsie that resolves to an nsIFIle for
   * the profile directory
   */
  get rootDir() {
    return IOUtils.getDirectory(this.path);
  }

  /**
   * Get the name of the avatar for the profile.
   *
   * @returns {string} Name of the avatar
   */
  get avatar() {
    return this.#avatar;
  }

  /**
   * Update the avatar, then trigger saving the profile, which will notify()
   * other running instances.
   *
   * @param {string} aAvatar Name of the avatar
   */
  set avatar(aAvatar) {
    this.avatar = aAvatar;

    this.saveUpdatesToDB();
  }

  // Note, theme properties are set and returned as a group.

  /**
   * Get the theme l10n-id as a string, like "theme-foo-name".
   *     the theme foreground color as CSS style string, like "rgb(1,1,1)",
   *     the theme background color as CSS style string, like "rgb(0,0,0)".
   *
   * @returns {object} an object of the form { themeL10nId, themeFg, themeBg }.
   */
  get theme() {
    return {
      themeL10nId: this.#themeL10nId,
      themeFg: this.#themeFg,
      themeBg: this.#themeBg,
    };
  }

  /**
   * Update the theme (all three properties are required), then trigger saving
   * the profile, which will notify() other running instances.
   *
   * @param {object} param0 The theme object
   * @param {string} param0.themeL10nId L10n id of the theme
   * @param {string} param0.themeFg Foreground color of theme as CSS style string, like "rgb(1,1,1)",
   * @param {string} param0.themeBg Background color of theme as CSS style string, like "rgb(0,0,0)".
   */
  set theme({ themeL10nId, themeFg, themeBg }) {
    this.#themeL10nId = themeL10nId;
    this.#themeFg = themeFg;
    this.#themeBg = themeBg;

    this.saveUpdatesToDB();
  }

  saveUpdatesToDB() {}

  toObject() {
    return {
      id: this.id,
      path: this.#path,
      name: this.name,
      avatar: this.avatar,
      ...this.theme,
    };
  }
}
