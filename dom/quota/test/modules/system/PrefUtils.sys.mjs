/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

export const PrefUtils = {
  /**
   * Retrieves the current values of specified preferences.
   *
   * Each preference is returned as a tuple with its name and value. If a
   * preference does not have a user-defined value, its value in the result is
   * `undefined`.
   *
   * @param {Array.<[string, *]>} prefs - An array of tuples where each tuple
   *   contains the preference name (string) and an example value (boolean,
   *   number, or string) to determine its type.
   * @returns {Array.<[string, *]>} - An array of tuples, each containing the
   *   preference name and its current value or `undefined` if it does not
   *   have a user-defined value.
   * @throws {Error} If a preference type is unsupported (not boolean, number,
   *   or string).
   */
  getPrefs(prefs) {
    return prefs.map(([pref, value]) => {
      if (!Services.prefs.prefHasUserValue(pref)) {
        return [pref, undefined];
      }
      switch (typeof value) {
        case "boolean":
          return [pref, Services.prefs.getBoolPref(pref)];
        case "number":
          return [pref, Services.prefs.getIntPref(pref)];
        case "string":
          return [pref, Services.prefs.getStringPref(pref)];
        default:
          throw new Error("Unsupported pref type!");
      }
    });
  },

  /**
   * Sets the values of specified preferences.
   *
   * Each preference in the input array is updated to the specified value based
   * on its type. If a preference value is `undefined`, the user-defined value
   * for that preference is cleared.
   *
   * @param {Array.<[string, *]>} prefs - An array of tuples, each containing
   *   the preference name (string) and the desired value (boolean, number,
   *   string, or `undefined`).
   *   - If the value is `undefined`, the user-defined preference is cleared.
   * @throws {Error} If a preference type is unsupported (not boolean, number,
   *   or string).
   */
  setPrefs(prefs) {
    for (let [pref, value] of prefs) {
      if (value === undefined) {
        Services.prefs.clearUserPref(pref);
        continue;
      }

      switch (typeof value) {
        case "boolean":
          Services.prefs.setBoolPref(pref, value);
          break;
        case "number":
          Services.prefs.setIntPref(pref, value);
          break;
        case "string":
          Services.prefs.setStringPref(pref, value);
          break;
        default:
          throw new Error("Unsupported pref type!");
      }
    }
  },
};
