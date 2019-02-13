/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.AppConstants.Versions;

/**
 * Constant values common to all Android services.
 */
public class GlobalConstants {
  public static final String BROWSER_INTENT_PACKAGE = AppConstants.ANDROID_PACKAGE_NAME;
  public static final String BROWSER_INTENT_CLASS = AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS;

  /**
   * Bug 800244: this signing-level permission protects broadcast intents that
   * should be received only by the Firefox versions with the given Android
   * package name.
   */
  public static final String PER_ANDROID_PACKAGE_PERMISSION = AppConstants.ANDROID_PACKAGE_NAME + ".permission.PER_ANDROID_PACKAGE";

  public static final int SHARED_PREFERENCES_MODE = 0;

  // These are used to ask Fennec (via reflection) to send
  // us a pref notification. This avoids us having to guess
  // Fennec's prefs branch and pref name.
  // Eventually Fennec might listen to startup notifications and
  // do this automatically, but this will do for now. See Bug 800244.
  public static String GECKO_PREFERENCES_CLASS = "org.mozilla.gecko.preferences.GeckoPreferences";
  public static String GECKO_BROADCAST_HEALTHREPORT_UPLOAD_PREF_METHOD  = "broadcastHealthReportUploadPref";
  public static String GECKO_BROADCAST_HEALTHREPORT_PRUNE_METHOD = "broadcastHealthReportPrune";

  // Common time values.
  public static final long MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;
  public static final long MILLISECONDS_PER_SIX_MONTHS = 180 * MILLISECONDS_PER_DAY;

  // Acceptable cipher suites.
  /**
   * We support only a very limited range of strong cipher suites and protocols:
   * no SSLv3 or TLSv1.0 (if we can), no DHE ciphers that might be vulnerable to Logjam
   * (https://weakdh.org/), no RC4.
   *
   * Backstory: Bug 717691 (we no longer support Android 2.2, so the name
   * workaround is unnecessary), Bug 1081953, Bug 1061273, Bug 1166839.
   *
   * See <http://developer.android.com/reference/javax/net/ssl/SSLSocket.html> for
   * supported Android versions for each set of protocols and cipher suites.
   *
   * Note that currently we need to support connections to Sync 1.1 on Mozilla-hosted infra,
   * as well as connections to FxA and Sync 1.5 on AWS.
   *
   * ELB cipher suites:
   * <http://docs.aws.amazon.com/ElasticLoadBalancing/latest/DeveloperGuide/elb-security-policy-table.html>
   */
  public static final String[] DEFAULT_CIPHER_SUITES;
  public static final String[] DEFAULT_PROTOCOLS;

  static {
    // Prioritize 128 over 256 as a tradeoff between device CPU/battery and the minor
    // increase in strength.
    if (Versions.feature20Plus) {
      DEFAULT_CIPHER_SUITES = new String[]
          {
           "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",   // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",        // 11+
           "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",        // 11+
           
           // For Sync 1.1.
           "TLS_DHE_RSA_WITH_AES_128_CBC_SHA",  // 9+
           "TLS_RSA_WITH_AES_128_CBC_SHA",      // 9+
          };
    } else if (Versions.feature11Plus) {
      DEFAULT_CIPHER_SUITES = new String[]
          {
           "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",        // 11+
           "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",      // 11+
           "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",        // 11+
           
           // For Sync 1.1.
           "TLS_DHE_RSA_WITH_AES_128_CBC_SHA",  // 9+
           "TLS_RSA_WITH_AES_128_CBC_SHA",      // 9+
          };
    } else {       // 9+
      // Fall back to the only half-decent cipher suites supported on Gingerbread.
      // N.B., there appears to be *no overlap* between the ELB 2015-05 default
      // suites and Gingerbread. A custom configuration is needed if moving beyond
      // the 2015-03 defaults.
      DEFAULT_CIPHER_SUITES = new String[]
          {
           // This is for Sync 1.5 on ELB 2015-03.
           "TLS_DHE_RSA_WITH_AES_128_CBC_SHA",
           "TLS_DHE_DSS_WITH_AES_128_CBC_SHA",

           // This is for Sync 1.1.
           "TLS_DHE_RSA_WITH_AES_128_CBC_SHA",  // 9+
           "TLS_RSA_WITH_AES_128_CBC_SHA",      // 9+
          };
    }

    if (Versions.feature16Plus) {
      DEFAULT_PROTOCOLS = new String[]
          {
           "TLSv1.2",
           "TLSv1.1",
           "TLSv1",             // We would like to remove this, and will do so when we can.
          };
    } else {
      // Fall back to TLSv1 if there's nothing better.
      DEFAULT_PROTOCOLS = new String[]
          {
           "TLSv1",
          };
    }
  }
}
