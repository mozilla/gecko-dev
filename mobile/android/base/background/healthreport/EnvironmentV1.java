/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;
import java.util.Iterator;
import java.util.SortedSet;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.nativecode.NativeCrypto;

public abstract class EnvironmentV1 {
  private static final String LOG_TAG = "GeckoEnvironment";
  private static final int VERSION = 1;

  protected final Class<? extends EnvironmentAppender> appenderClass;

  protected volatile String hash = null;
  protected volatile int id = -1;

  public int version = VERSION;

  // org.mozilla.profile.age.
  public int profileCreation;

  // org.mozilla.sysinfo.sysinfo.
  public int cpuCount;
  public int memoryMB;
  public String architecture;
  public String sysName;
  public String sysVersion;       // Kernel.

  // geckoAppInfo.
  public String vendor;
  public String appName;
  public String appID;
  public String appVersion;
  public String appBuildID;
  public String platformVersion;
  public String platformBuildID;
  public String os;
  public String xpcomabi;
  public String updateChannel;

  // appinfo.
  public int isBlocklistEnabled;
  public int isTelemetryEnabled;

  // org.mozilla.addons.active.
  public JSONObject addons = null;

  // org.mozilla.addons.counts.
  public int extensionCount;
  public int pluginCount;
  public int themeCount;

  /**
   * We break out this interface in order to allow for testing -- pass in your
   * own appender that just records strings, for example.
   */
  public static abstract class EnvironmentAppender {
    public abstract void append(String s);
    public abstract void append(int v);
  }

  public static class HashAppender extends EnvironmentAppender {
    private final StringBuilder builder;

    public HashAppender() throws NoSuchAlgorithmException {
      builder = new StringBuilder();
    }

    @Override
    public void append(String s) {
      builder.append((s == null) ? "null" : s);
    }

    @Override
    public void append(int profileCreation) {
      append(Integer.toString(profileCreation, 10));
    }

    @Override
    public String toString() {
      // We *could* use ASCII85… but the savings would be negated by the
      // inclusion of JSON-unsafe characters like double-quote.
      final byte[] inputBytes;
      try {
        inputBytes = builder.toString().getBytes("UTF-8");
      } catch (UnsupportedEncodingException e) {
        Logger.warn(LOG_TAG, "Invalid charset String passed to getBytes", e);
        return null;
      }

      // Note to the security-minded reader: we deliberately use SHA-1 here, not
      // a stronger hash. These identifiers don't strictly need a cryptographic
      // hash function, because there is negligible value in attacking the hash.
      // We use SHA-1 because it's *shorter* -- the exact same reason that Git
      // chose SHA-1.
      final byte[] hash = NativeCrypto.sha1(inputBytes);
      return new Base64(-1, null, false).encodeAsString(hash);
    }
  }

  /**
   * Ensure that the {@link Environment} has been registered with its
   * storage layer, and can be used to annotate events.
   *
   * It's safe to call this method more than once, and each time you'll
   * get the same ID.
   *
   * @return the integer ID to use in subsequent DB insertions.
   */
  public abstract int register();

  protected EnvironmentAppender getAppender() {
    EnvironmentAppender appender = null;
    try {
      appender = appenderClass.newInstance();
    } catch (InstantiationException | IllegalAccessException ex) {
      // Should never happen, but...
      Logger.warn(LOG_TAG,  "Could not compute hash.", ex);
    }

    return appender;
  }

  protected void appendHash(EnvironmentAppender appender) {
    appender.append(profileCreation);
    appender.append(cpuCount);
    appender.append(memoryMB);
    appender.append(architecture);
    appender.append(sysName);
    appender.append(sysVersion);
    appender.append(vendor);
    appender.append(appName);
    appender.append(appID);
    appender.append(appVersion);
    appender.append(appBuildID);
    appender.append(platformVersion);
    appender.append(platformBuildID);
    appender.append(os);
    appender.append(xpcomabi);
    appender.append(updateChannel);
    appender.append(isBlocklistEnabled);
    appender.append(isTelemetryEnabled);
    appender.append(extensionCount);
    appender.append(pluginCount);
    appender.append(themeCount);

    // We need sorted values.
    if (addons != null) {
      appendSortedAddons(getNonIgnoredAddons(), appender);
    }
  }

  /**
   * Compute the stable hash of the configured environment.
   *
   * @return the hash in base34, or null if there was a problem.
   */
  public String getHash() {
    // It's never unset, so we only care about partial reads. volatile is enough.
    if (hash != null) {
      return hash;
    }

    EnvironmentAppender appender = getAppender();
    if (appender == null) {
      return null;
    }

    appendHash(appender);
    return hash = appender.toString();
  }

  public EnvironmentV1(Class<? extends EnvironmentAppender> appenderClass) {
    super();
    this.appenderClass = appenderClass;
  }

  public JSONObject getNonIgnoredAddons() {
    if (addons == null) {
      return null;
    }
    JSONObject out = new JSONObject();
    @SuppressWarnings("unchecked")
    Iterator<String> keys = addons.keys();
    while (keys.hasNext()) {
      try {
        final String key = keys.next();
        final Object obj = addons.get(key);
        if (obj != null &&
            obj instanceof JSONObject &&
            ((JSONObject) obj).optBoolean("ignore", false)) {
          continue;
        }
        out.put(key, obj);
      } catch (JSONException ex) {
        // Do nothing.
      }
    }
    return out;
  }

  /**
   * Take a collection of add-on descriptors, appending a consistent string
   * to the provided builder.
   */
  public static void appendSortedAddons(JSONObject addons, final EnvironmentAppender builder) {
    final SortedSet<String> keys = HealthReportUtils.sortedKeySet(addons);

    // For each add-on, produce a consistent, sorted mapping of its descriptor.
    for (String key : keys) {
      try {
        JSONObject addon = addons.getJSONObject(key);

        // Now produce the output for this add-on.
        builder.append(key);
        builder.append("={");

        for (String addonKey : HealthReportUtils.sortedKeySet(addon)) {
          builder.append(addonKey);
          builder.append("==");
          try {
            builder.append(addon.get(addonKey).toString());
          } catch (JSONException e) {
            builder.append("_e_");
          }
        }

        builder.append("}");
      } catch (Exception e) {
        // Muffle.
        Logger.warn(LOG_TAG, "Invalid add-on for ID " + key);
      }
    }
  }

  public void setJSONForAddons(byte[] json) throws Exception {
    setJSONForAddons(new String(json, "UTF-8"));
  }

  public void setJSONForAddons(String json) throws Exception {
    if (json == null || "null".equals(json)) {
      addons = null;
      return;
    }
    addons = new JSONObject(json);
  }

  public void setJSONForAddons(JSONObject json) {
    addons = json;
  }

  /**
   * Includes ignored add-ons.
   */
  public String getNormalizedAddonsJSON() {
    // We trust that our input will already be normalized. If that assumption
    // is invalidated, then we'll be sorry.
    return (addons == null) ? "null" : addons.toString();
  }
}
