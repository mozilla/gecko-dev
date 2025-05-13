/**
 * Format of a Gecko pref for GeckoView purposes.
 */
export interface GeckoPreference {
  /**
   * The name of the pref (e.g., "some.preference.item").
   */
  pref: string;

  /**
   * The Ci.nsIPrefBranch type of the pref.
   *
   * PREF_INVALID = 0
   * PREF_STRING = 32
   * PREF_INT = 64
   * PREF_BOOL = 128
   */
  type: 0 | 32 | 64 | 128;

  /**
   * The current default value of the pref. Could be a string, boolean, or number. It will depend on the pref type.
   */
  defaultValue: string | boolean | number | null;

  /**
   * The current default value of the pref. Could be a string, boolean, or number. It will depend on the pref type.
   */
  userValue: string | boolean | number | null;
}
