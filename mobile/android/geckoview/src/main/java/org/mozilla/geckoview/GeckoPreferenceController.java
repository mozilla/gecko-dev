/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.util.Log;
import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Objects;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;

/** Used to access and manipulate Gecko preferences through GeckoView. */
public class GeckoPreferenceController {
  private static final String LOGTAG = "GeckoPreference";
  private static final boolean DEBUG = false;

  private static final String GET_PREF = "GeckoView:Preferences:GetPref";
  private static final String SET_PREF = "GeckoView:Preferences:SetPref";
  private static final String CLEAR_PREF = "GeckoView:Preferences:ClearPref";

  /**
   * Retrieves the value of a given Gecko preference.
   *
   * @param prefName The preference to find the value of. e.g., some.pref.value.
   * @return The typed Gecko preference that corresponds to this value.
   */
  @AnyThread
  public static @NonNull GeckoResult<GeckoPreference<?>> getGeckoPref(
      @NonNull final String prefName) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("pref", prefName);
    return EventDispatcher.getInstance()
        .queryBundle(GET_PREF, bundle)
        .map(
            GeckoPreference::fromBundle,
            exception -> new Exception("Could not retrieve the preference."));
  }

  /**
   * Sets a String preference with Gecko. Float preferences should use this API.
   *
   * @param prefName The name of the preference to change. e.g., "some.pref.item".
   * @param value The string value the preference should be set to.
   * @param branch The preference branch to operate on. For most usage this will usually be {@link
   *     #PREF_BRANCH_USER} to actively change the value that is active. {@link
   *     #PREF_BRANCH_DEFAULT} will change the current default. If there is ever a user preference
   *     value set, then the user value will be used over the default value. The user value will be
   *     saved as a part of the user's profile. The default value will not be saved on the user's
   *     profile.
   * @return Will return a GeckoResult when the pref is set or else complete exceptionally.
   */
  @AnyThread
  public static @NonNull GeckoResult<Void> setGeckoPref(
      @NonNull final String prefName, @NonNull final String value, @PrefBranch final int branch) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("pref", prefName);
    bundle.putString("value", value);
    bundle.putString("branch", toBranchString(branch));
    bundle.putInt("type", PREF_TYPE_STRING);
    return EventDispatcher.getInstance().queryVoid(SET_PREF, bundle);
  }

  /**
   * Sets an Integer preference with Gecko.
   *
   * @param prefName The name of the preference to change. e.g., "some.pref.item".
   * @param value The integer value the preference should be set to.
   * @param branch The preference branch to operate on. For most usage this will usually be {@link
   *     #PREF_BRANCH_USER} to actively change the value that is active. {@link
   *     #PREF_BRANCH_DEFAULT} will change the current default. If there is ever a user preference
   *     value set, then the user value will be used over the default value. The user value will be
   *     saved as a part of the user's profile. The default value will not be saved on the user's
   *     profile.
   * @return Will return a GeckoResult when the pref is set or else complete exceptionally.
   */
  @AnyThread
  public static @NonNull GeckoResult<Void> setGeckoPref(
      @NonNull final String prefName, @NonNull final Integer value, @PrefBranch final int branch) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("pref", prefName);
    bundle.putInt("value", value);
    bundle.putString("branch", toBranchString(branch));
    bundle.putInt("type", PREF_TYPE_INT);
    return EventDispatcher.getInstance().queryVoid(SET_PREF, bundle);
  }

  /**
   * Sets a boolean preference with Gecko.
   *
   * @param prefName The name of the preference to change. e.g., "some.pref.item".
   * @param value The boolean value the preference should be set to.
   * @param branch The preference branch to operate on. For most usage this will usually be {@link
   *     #PREF_BRANCH_USER} to actively change the value that is active. {@link
   *     #PREF_BRANCH_DEFAULT} will change the current default. If there is ever a user preference
   *     value set, then the user value will be used over the default value. The user value will be
   *     saved as a part of the user's profile. The default value will not be saved on the user's
   *     profile.
   * @return Will return a GeckoResult when the pref is set or else complete exceptionally.
   */
  @AnyThread
  public static @NonNull GeckoResult<Void> setGeckoPref(
      @NonNull final String prefName, @NonNull final Boolean value, @PrefBranch final int branch) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("pref", prefName);
    bundle.putBoolean("value", value);
    bundle.putString("branch", toBranchString(branch));
    bundle.putInt("type", PREF_TYPE_BOOL);
    return EventDispatcher.getInstance().queryVoid(SET_PREF, bundle);
  }

  /***
   * Restated from nsIPrefBranch.idl's clearUserPref:
   * <p>
   * Called to clear a user set value from a specific preference. This will, in
   * effect, reset the value to the default value. If no default value exists
   * the preference will cease to exist.
   *
   * @param prefName The name of the preference to clear. e.g., "some.pref.item".
   * @return Will return a GeckoResult once the pref is cleared.
   */
  @AnyThread
  public static @NonNull GeckoResult<Void> clearGeckoUserPref(@NonNull final String prefName) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("pref", prefName);
    return EventDispatcher.getInstance().queryVoid(CLEAR_PREF, bundle);
  }

  /** The Observer class contains utilities for monitoring preference changes in Gecko. */
  public static final class Observer {
    private static final String REGISTER_PREF = "GeckoView:Preferences:RegisterObserver";
    private static final String UNREGISTER_PREF = "GeckoView:Preferences:UnregisterObserver";

    /**
     * This will register a preference for observation.
     *
     * @param preferenceName The Gecko preference that should be placed under observation. e.g.,
     *     "some.pref.item".
     * @return The GeckoResult will complete with the current preference value when observation is
     *     set.
     */
    @AnyThread
    public static @NonNull GeckoResult<Void> registerPreference(
        @NonNull final String preferenceName) {
      return registerPreferences(List.of(preferenceName));
    }

    /**
     * This will register preferences for observation.
     *
     * @param preferenceNames A list of Gecko preference that should be placed under observation.
     *     e.g., "some.pref.item", "some.pref.item.other".
     * @return The GeckoResult will complete with the current preference value when observation is
     *     set.
     */
    @AnyThread
    public static @NonNull GeckoResult<Void> registerPreferences(
        @NonNull final List<String> preferenceNames) {
      final GeckoBundle bundle = new GeckoBundle();
      bundle.putStringArray("prefs", preferenceNames);
      return EventDispatcher.getInstance().queryVoid(REGISTER_PREF, bundle);
    }

    /**
     * This will deregister a preference for observation.
     *
     * @param preferenceName The Gecko preference that should be removed from observation. e.g.,
     *     "some.pref.item".
     * @return The GeckoResult will complete when the observer is removed. If the item requested is
     *     not under observation, the function will still return.
     */
    @UiThread
    public static @NonNull GeckoResult<Void> unregisterPreference(
        @NonNull final String preferenceName) {
      return unregisterPreferences(List.of(preferenceName));
    }

    /**
     * This will deregister preferences for observation.
     *
     * @param preferenceNames The Gecko preferences that should be removed from observation. e.g.,
     *     "some.pref.item", "some.pref.item.other".
     * @return The GeckoResult will complete when the observer is removed. If the item requested is
     *     not under observation, the function will still return.
     */
    @UiThread
    public static @NonNull GeckoResult<Void> unregisterPreferences(
        @NonNull final List<String> preferenceNames) {
      final GeckoBundle bundle = new GeckoBundle();
      bundle.putStringArray("prefs", preferenceNames);
      return EventDispatcher.getInstance().queryVoid(UNREGISTER_PREF, bundle);
    }

    /** Delegate definition for observing Gecko preferences. */
    public interface Delegate {
      /**
       * When a preference is registered using {@link #registerPreference(String)}, if the
       * preference's value changes, then this callback will occur.
       *
       * @param observedGeckoPreference The new Gecko preference value that was recently observed.
       */
      @AnyThread
      default void onGeckoPreferenceChange(
          @NonNull final GeckoPreference<?> observedGeckoPreference) {}
    }
  }

  /**
   * Pref types as defined by Gecko in nsIPrefBranch.idl and should remain in sync.
   *
   * <p>Note: A Float preference will operate as a PREF_STRING due to Gecko's handling.
   */
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({PREF_TYPE_INVALID, PREF_TYPE_STRING, PREF_TYPE_INT, PREF_TYPE_BOOL})
  public @interface PrefType {}

  /** Used when the preference does not have a type (i.e. is not defined). */
  public static final int PREF_TYPE_INVALID = 0;

  /** Used when the preference conforms to type string. */
  public static final int PREF_TYPE_STRING = 32;

  /** Used when the preference conforms to type integer. */
  public static final int PREF_TYPE_INT = 64;

  /** Used when the preference conforms to type boolean. */
  public static final int PREF_TYPE_BOOL = 128;

  /**
   * Convenience method for converting from {@link PrefType} to string. These values should remain
   * in sync with nsIPrefBranch.idl.
   *
   * @param prefType The defined {@link PrefType}.
   * @return The String representation of the construct.
   */
  @AnyThread
  /* package */ static @NonNull String toTypeString(@PrefType final int prefType) {
    switch (prefType) {
      case PREF_TYPE_INVALID:
        return "PREF_INVALID";
      case PREF_TYPE_STRING:
        return "PREF_STRING";
      case PREF_TYPE_INT:
        return "PREF_INT";
      case PREF_TYPE_BOOL:
        return "PREF_BOOL";
      default:
        return "UNKNOWN";
    }
  }

  /** Pref branch used to distinguish user and default Gecko preferences. */
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({PREF_BRANCH_USER, PREF_BRANCH_DEFAULT})
  public @interface PrefBranch {}

  /**
   * Used when the preference is a "user" defined preference. A "user" preference is specified to be
   * set as the current value of the preference. It will persist through restarts and is a part of
   * the user's profile.
   */
  public static final int PREF_BRANCH_USER = 0;

  /**
   * Used when the preference is a default preference. A "default" preference is what is used when
   * no user preference is set.
   */
  public static final int PREF_BRANCH_DEFAULT = 1;

  /**
   * Convenience method for converting from {@link #@PrefBranch} to string.
   *
   * @param prefBranch The defined {@link #@PrefBranch}.
   * @return The String representation of the construct.
   */
  @AnyThread
  /* package */ static @NonNull String toBranchString(@PrefBranch final int prefBranch) {
    switch (prefBranch) {
      case PREF_BRANCH_USER:
        return "user";
      case PREF_BRANCH_DEFAULT:
        return "default";
      default:
        Log.w(LOGTAG, "Tried to convert an unknown pref branch of " + prefBranch + " !");
        return "default";
    }
  }

  /**
   * This object represents information on a GeckoPreference.
   *
   * @param <T> The type of the preference.
   */
  public static class GeckoPreference<T> {

    /** The Gecko preference name. (e.g., "some.pref.item") */
    public final @NonNull String pref;

    /** The Gecko type of preference. (e.g., "PREF_BOOL" or "PREF_STRING" or "PREF_INT") */
    public final @PrefType int type;

    /** The default value of the preference. Corresponds to the default branch's value. */
    public final @Nullable T defaultValue;

    /** The user value of the preference. Corresponds to the user branch's value. */
    public final @Nullable T userValue;

    /**
     * The current value of the preference that is in operation.
     *
     * @return Will return the user value if set, if not then the default value.
     */
    @AnyThread
    public @Nullable T getValue() {
      if (userValue != null) {
        return userValue;
      }
      return defaultValue;
    }

    /**
     * Checks to see if the user value has changed from the default value.
     *
     * @return Whether the user value has diverged from the default value.
     */
    @AnyThread
    public boolean getHasUserChangedValue() {
      return userValue != null;
    }

    /**
     * Constructor for a GeckoPreference.
     *
     * @param pref Name of preference. (e.g., "some.gecko.pref")
     * @param type The Gecko type for the preference. (e.g., PREF_STRING )
     * @param defaultValue The default value of the pref.
     * @param userValue The user value of the pref. unknown.)
     */
    /* package */ GeckoPreference(
        @NonNull final String pref,
        @PrefType final int type,
        @Nullable final T defaultValue,
        @Nullable final T userValue) {
      this.pref = pref;
      this.type = type;
      this.defaultValue = defaultValue;
      this.userValue = userValue;
    }

    /**
     * Convenience method to format the GeckoPreference object into a string.
     *
     * @return String representing GeckoPreference.
     */
    @NonNull
    @Override
    public String toString() {
      final StringBuilder builder = new StringBuilder("GeckoPreference {");
      builder
          .append("pref=")
          .append(pref)
          .append(", type=")
          .append(toTypeString(type))
          .append(", defaultValue=")
          .append(Objects.toString(defaultValue, "null"))
          .append(", userValue=")
          .append(Objects.toString(userValue, "null"))
          .append("}");
      return builder.toString();
    }

    /**
     * Convenience method to deserialize preference information into a {@link GeckoPreference}.
     *
     * @param bundle The bundle containing the preference information. Should contain pref, type,
     *     branch, status, and value.
     * @return A typed preference object.
     */
    /* package */
    static @Nullable GeckoPreference<?> fromBundle(@Nullable final GeckoBundle bundle) {
      if (bundle == null) {
        Log.w(LOGTAG, "Bundle is null when attempting to deserialize a GeckoPreference.");
        return null;
      }
      try {
        final String pref = bundle.getString("pref", "");
        if (pref.isEmpty()) {
          Log.w(LOGTAG, "Deserialized an empty preference name.");
          return null;
        }
        final int type = bundle.getInt("type", 0);
        switch (type) {
          case PREF_TYPE_INVALID:
            {
              return new GeckoPreference<Object>(pref, type, null, null);
            }
          case PREF_TYPE_STRING:
            {
              final String defaultValue = bundle.getString("defaultValue");
              final String userValue = bundle.getString("userValue");
              return new GeckoPreference<String>(pref, type, defaultValue, userValue);
            }
          case PREF_TYPE_BOOL:
            {
              final Boolean defaultValue = bundle.getBooleanObject("defaultValue");
              final Boolean userValue = bundle.getBooleanObject("userValue");
              return new GeckoPreference<Boolean>(pref, type, defaultValue, userValue);
            }
          case PREF_TYPE_INT:
            {
              final Integer defaultValue = bundle.getInteger("defaultValue");
              final Integer userValue = bundle.getInteger("userValue");
              return new GeckoPreference<Integer>(pref, type, defaultValue, userValue);
            }
          default:
            {
              Log.w(LOGTAG, "Deserialized an unexpected preference type of " + type + ".");
              return null;
            }
        }
      } catch (final Exception e) {
        Log.w(LOGTAG, "Could not deserialize GeckoPreference object: " + e);
        return null;
      }
    }
  }
}
