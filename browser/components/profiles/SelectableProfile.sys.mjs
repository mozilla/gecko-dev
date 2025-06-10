/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ProfilesDatastoreService } from "moz-src:///toolkit/profile/ProfilesDatastoreService.sys.mjs";
import { SelectableProfileService } from "resource:///modules/profiles/SelectableProfileService.sys.mjs";

const STANDARD_AVATARS = new Set([
  "barbell",
  "bike",
  "book",
  "briefcase",
  "canvas",
  "craft",
  "default-favicon",
  "diamond",
  "flower",
  "folder",
  "hammer",
  "heart",
  "heart-rate",
  "history",
  "leaf",
  "lightbulb",
  "makeup",
  "message",
  "musical-note",
  "palette",
  "paw-print",
  "plane",
  "present",
  "shopping",
  "soccer",
  "sparkle-single",
  "star",
  "video-game-controller",
]);
const STANDARD_AVATAR_SIZES = [16, 20, 24, 48, 60, 80];

function standardAvatarURL(avatar, size = "80") {
  return `chrome://browser/content/profiles/assets/${size}_${avatar}.svg`;
}

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

  // Name of the user's chosen avatar, which corresponds to a list of standard
  // SVG avatars. Or if the avatar is a custom image, the filename of the image
  // stored in the avatars directory.
  #avatar;

  // lastAvatarURL is saved when URL.createObjectURL is invoked so we can
  // revoke the url at a later time.
  #lastAvatarURL;

  // Cached theme properties, used to allow displaying a SelectableProfile
  // without loading the AddonManager to get theme info.
  #themeId;
  #themeFg;
  #themeBg;

  constructor(row) {
    this.#id = row.getResultByName("id");
    this.#path = row.getResultByName("path");
    this.#name = row.getResultByName("name");
    this.#avatar = row.getResultByName("avatar");
    this.#themeId = row.getResultByName("themeId");
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

    Services.prefs.setBoolPref("browser.profiles.profile-name.updated", true);
  }

  /**
   * Get the full path to the profile as a string.
   *
   * @returns {string} Path of profile
   */
  get path() {
    return PathUtils.joinRelative(
      ProfilesDatastoreService.constructor.getDirectory("UAppData").path,
      this.#path
    );
  }

  /**
   * Get the profile directory as an nsIFile.
   *
   * @returns {Promise<nsIFile>} A promise that resolves to an nsIFile for
   * the profile directory
   */
  get rootDir() {
    return IOUtils.getDirectory(this.path);
  }

  /**
   * Get the profile local directory as an nsIFile.
   *
   * @returns {Promise<nsIFile>} A promise that resolves to an nsIFile for
   * the profile local directory
   */
  get localDir() {
    return this.rootDir.then(root => {
      let relative = root.getRelativePath(
        ProfilesDatastoreService.constructor.getDirectory("DefProfRt")
      );
      let local =
        ProfilesDatastoreService.constructor.getDirectory("DefProfLRt");
      local.appendRelativePath(relative);
      return local;
    });
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
   * Get the path of the current avatar.
   * If the avatar is standard, the return value will be of the form
   * 'chrome://browser/content/profiles/assets/{avatar}.svg'.
   * If the avatar is custom, the return value will be the path to the file on
   * disk.
   *
   * @returns {string} Path to the current avatar.
   */
  getAvatarPath(size) {
    if (!this.hasCustomAvatar) {
      return standardAvatarURL(this.avatar, size);
    }

    return PathUtils.join(
      ProfilesDatastoreService.constructor.PROFILE_GROUPS_DIR,
      "avatars",
      this.avatar
    );
  }

  /**
   * Get the URL of the current avatar.
   * If the avatar is standard, the return value will be of the form
   * 'chrome://browser/content/profiles/assets/${size}_${avatar}.svg'.
   * If the avatar is custom, the return value will be a blob URL.
   *
   * @param {string|number} size optional Must be one of the sizes in
   * STANDARD_AVATAR_SIZES. Will be converted to a string.
   *
   * @returns {Promise<string>} Resolves to the URL of the current avatar
   */
  async getAvatarURL(size) {
    if (!this.hasCustomAvatar) {
      return standardAvatarURL(this.avatar, size);
    }

    if (this.#lastAvatarURL) {
      URL.revokeObjectURL(this.#lastAvatarURL);
    }

    const fileExists = await IOUtils.exists(this.getAvatarPath());
    if (!fileExists) {
      throw new Error("Custom avatar file doesn't exist.");
    }
    const file = await File.createFromFileName(this.getAvatarPath());
    this.#lastAvatarURL = URL.createObjectURL(file);

    return this.#lastAvatarURL;
  }

  /**
   * Get the avatar file. This is only used for custom avatars to generate an
   * object url. Standard avatars should use getAvatarURL or getAvatarPath.
   *
   * @returns {Promise<File>} Resolves to a file of the avatar
   */
  async getAvatarFile() {
    if (!this.hasCustomAvatar) {
      throw new Error(
        "Profile does not have custom avatar. Custom avatar file doesn't exist."
      );
    }

    return File.createFromFileName(this.getAvatarPath());
  }

  get hasCustomAvatar() {
    return !STANDARD_AVATARS.has(this.avatar);
  }

  /**
   * Update the avatar, then trigger saving the profile, which will notify()
   * other running instances.
   *
   * @param {string|File} aAvatarOrFile Name of the avatar or File os custom avatar
   */
  async setAvatar(aAvatarOrFile) {
    if (STANDARD_AVATARS.has(aAvatarOrFile)) {
      this.#avatar = aAvatarOrFile;
    } else {
      await this.#uploadCustomAvatar(aAvatarOrFile);
    }

    await this.saveUpdatesToDB();
  }

  async #uploadCustomAvatar(file) {
    const avatarsDir = PathUtils.join(
      ProfilesDatastoreService.constructor.PROFILE_GROUPS_DIR,
      "avatars"
    );

    // Create avatars directory if it does not exist
    await IOUtils.makeDirectory(avatarsDir, { ignoreExisting: true });

    let uuid = Services.uuid.generateUUID().toString().slice(1, -1);

    const filePath = PathUtils.join(avatarsDir, uuid);

    const arrayBuffer = await file.arrayBuffer();
    const uint8Array = new Uint8Array(arrayBuffer);

    await IOUtils.write(filePath, uint8Array, { tmpPath: `${filePath}.tmp` });

    this.#avatar = uuid;
  }

  /**
   * Get the l10n id for the current avatar.
   *
   * @returns {string} L10n id for the current avatar
   */
  get avatarL10nId() {
    switch (this.avatar) {
      case "book":
        return "book-avatar-alt";
      case "briefcase":
        return "briefcase-avatar-alt";
      case "flower":
        return "flower-avatar-alt";
      case "heart":
        return "heart-avatar-alt";
      case "shopping":
        return "shopping-avatar-alt";
      case "star":
        return "star-avatar-alt";
    }

    return "custom-avatar-alt";
  }

  // Note, theme properties are set and returned as a group.

  /**
   * Get the theme l10n-id as a string, like "theme-foo-name".
   *     the theme foreground color as CSS style string, like "rgb(1,1,1)",
   *     the theme background color as CSS style string, like "rgb(0,0,0)".
   *
   * @returns {object} an object of the form { themeId, themeFg, themeBg }.
   */
  get theme() {
    return {
      themeId: this.#themeId,
      themeFg: this.#themeFg,
      themeBg: this.#themeBg,
    };
  }

  get iconPaintContext() {
    return {
      fillColor: this.#themeBg,
      strokeColor: this.#themeFg,
      fillOpacity: 1.0,
      strokeOpacity: 1.0,
    };
  }

  /**
   * Update the theme (all three properties are required), then trigger saving
   * the profile, which will notify() other running instances.
   *
   * @param {object} param0 The theme object
   * @param {string} param0.themeId L10n id of the theme
   * @param {string} param0.themeFg Foreground color of theme as CSS style string, like "rgb(1,1,1)",
   * @param {string} param0.themeBg Background color of theme as CSS style string, like "rgb(0,0,0)".
   */
  set theme({ themeId, themeFg, themeBg }) {
    this.#themeId = themeId;
    this.#themeFg = themeFg;
    this.#themeBg = themeBg;

    this.saveUpdatesToDB();
  }

  saveUpdatesToDB() {
    SelectableProfileService.updateProfile(this);
  }

  /**
   * Returns on object with only fields needed for the database.
   *
   * @returns {object} An object with only fields need for the database
   */
  async toDbObject() {
    let profileObj = {
      id: this.id,
      path: this.#path,
      name: this.name,
      avatar: this.avatar,
      ...this.theme,
    };

    return profileObj;
  }

  /**
   * Returns an object representation of the profile.
   * Note: No custom avatar URLs are included because URL.createObjectURL needs
   * to be invoked in the content process for the avatar to be visible.
   *
   * @returns {object} An object representation of the profile
   */
  async toContentSafeObject() {
    let profileObj = {
      id: this.id,
      path: this.#path,
      name: this.name,
      avatar: this.avatar,
      avatarL10nId: this.avatarL10nId,
      hasCustomAvatar: this.hasCustomAvatar,
      ...this.theme,
    };

    if (this.hasCustomAvatar) {
      let path = this.getAvatarPath();
      let file = await this.getAvatarFile();

      profileObj.avatarPaths = Object.fromEntries(
        STANDARD_AVATAR_SIZES.map(s => [`path${s}`, path])
      );
      profileObj.avatarFiles = Object.fromEntries(
        STANDARD_AVATAR_SIZES.map(s => [`file${s}`, file])
      );
      profileObj.avatarURLs = {};
    } else {
      profileObj.avatarPaths = Object.fromEntries(
        STANDARD_AVATAR_SIZES.map(s => [`path${s}`, this.getAvatarPath(s)])
      );
      profileObj.avatarURLs = Object.fromEntries(
        await Promise.all(
          STANDARD_AVATAR_SIZES.map(async s => [
            `url${s}`,
            await this.getAvatarURL(s),
          ])
        )
      );
    }

    return profileObj;
  }
}
