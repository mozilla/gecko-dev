/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Collections;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.login.StateFactory;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

/**
 * A Firefox Account that stores its details and state as user data attached to
 * an Android Account instance.
 * <p>
 * Account user data is accessible only to the Android App(s) that own the
 * Account type. Account user data is not removed when the App's private data is
 * cleared.
 */
public class AndroidFxAccount {
  protected static final String LOG_TAG = AndroidFxAccount.class.getSimpleName();

  public static final int CURRENT_PREFS_VERSION = 1;

  public static final int CURRENT_ACCOUNT_VERSION = 3;
  public static final String ACCOUNT_KEY_ACCOUNT_VERSION = "version";
  public static final String ACCOUNT_KEY_PROFILE = "profile";
  public static final String ACCOUNT_KEY_IDP_SERVER = "idpServerURI";

  // The audience should always be a prefix of the token server URI.
  public static final String ACCOUNT_KEY_AUDIENCE = "audience";                 // Sync-specific.
  public static final String ACCOUNT_KEY_TOKEN_SERVER = "tokenServerURI";       // Sync-specific.
  public static final String ACCOUNT_KEY_DESCRIPTOR = "descriptor";

  public static final int CURRENT_BUNDLE_VERSION = 2;
  public static final String BUNDLE_KEY_BUNDLE_VERSION = "version";
  public static final String BUNDLE_KEY_STATE_LABEL = "stateLabel";
  public static final String BUNDLE_KEY_STATE = "state";

  protected final Context context;
  protected final AccountManager accountManager;
  protected final Account account;

  /**
   * Create an Android Firefox Account instance backed by an Android Account
   * instance.
   * <p>
   * We expect a long-lived application context to avoid life-cycle issues that
   * might arise if the internally cached AccountManager instance surfaces UI.
   * <p>
   * We take care to not install any listeners or observers that might outlive
   * the AccountManager; and Android ensures the AccountManager doesn't outlive
   * the associated context.
   *
   * @param applicationContext
   *          to use as long-lived ambient Android context.
   * @param account
   *          Android account to use for storage.
   */
  public AndroidFxAccount(Context applicationContext, Account account) {
    this.context = applicationContext;
    this.account = account;
    this.accountManager = AccountManager.get(this.context);
  }

  public Account getAndroidAccount() {
    return this.account;
  }

  protected int getAccountVersion() {
    String v = accountManager.getUserData(account, ACCOUNT_KEY_ACCOUNT_VERSION);
    if (v == null) {
      return 0;         // Implicit.
    }

    try {
      return Integer.parseInt(v, 10);
    } catch (NumberFormatException ex) {
      return 0;
    }
  }

  protected void persistBundle(ExtendedJSONObject bundle) {
    accountManager.setUserData(account, ACCOUNT_KEY_DESCRIPTOR, bundle.toJSONString());
  }

  protected ExtendedJSONObject unbundle() {
    final int version = getAccountVersion();
    if (version < CURRENT_ACCOUNT_VERSION) {
      // Needs upgrade. For now, do nothing. We'd like to just put your account
      // into the Separated state here and have you update your credentials.
      return null;
    }

    if (version > CURRENT_ACCOUNT_VERSION) {
      // Oh dear.
      return null;
    }

    String bundle = accountManager.getUserData(account, ACCOUNT_KEY_DESCRIPTOR);
    if (bundle == null) {
      return null;
    }
    return unbundleAccountV2(bundle);
  }

  protected String getBundleData(String key) {
    ExtendedJSONObject o = unbundle();
    if (o == null) {
      return null;
    }
    return o.getString(key);
  }

  protected boolean getBundleDataBoolean(String key, boolean def) {
    ExtendedJSONObject o = unbundle();
    if (o == null) {
      return def;
    }
    Boolean b = o.getBoolean(key);
    if (b == null) {
      return def;
    }
    return b.booleanValue();
  }

  protected byte[] getBundleDataBytes(String key) {
    ExtendedJSONObject o = unbundle();
    if (o == null) {
      return null;
    }
    return o.getByteArrayHex(key);
  }

  protected void updateBundleDataBytes(String key, byte[] value) {
    updateBundleValue(key, value == null ? null : Utils.byte2Hex(value));
  }

  protected void updateBundleValue(String key, boolean value) {
    ExtendedJSONObject descriptor = unbundle();
    if (descriptor == null) {
      return;
    }
    descriptor.put(key, value);
    persistBundle(descriptor);
  }

  protected void updateBundleValue(String key, String value) {
    ExtendedJSONObject descriptor = unbundle();
    if (descriptor == null) {
      return;
    }
    descriptor.put(key, value);
    persistBundle(descriptor);
  }

  private ExtendedJSONObject unbundleAccountV1(String bundle) {
    ExtendedJSONObject o;
    try {
      o = new ExtendedJSONObject(bundle);
    } catch (Exception e) {
      return null;
    }
    if (CURRENT_BUNDLE_VERSION == o.getIntegerSafely(BUNDLE_KEY_BUNDLE_VERSION)) {
      return o;
    }
    return null;
  }

  private ExtendedJSONObject unbundleAccountV2(String bundle) {
    return unbundleAccountV1(bundle);
  }

  /**
   * Note that if the user clears data, an account will be left pointing to a
   * deleted profile. Such is life.
   */
  public String getProfile() {
    return accountManager.getUserData(account, ACCOUNT_KEY_PROFILE);
  }

  public String getAccountServerURI() {
    return accountManager.getUserData(account, ACCOUNT_KEY_IDP_SERVER);
  }

  public String getAudience() {
    return accountManager.getUserData(account, ACCOUNT_KEY_AUDIENCE);
  }

  public String getTokenServerURI() {
    return accountManager.getUserData(account, ACCOUNT_KEY_TOKEN_SERVER);
  }

  /**
   * This needs to return a string because of the tortured prefs access in GlobalSession.
   */
  public String getSyncPrefsPath() throws GeneralSecurityException, UnsupportedEncodingException {
    String profile = getProfile();
    String username = account.name;

    if (profile == null) {
      throw new IllegalStateException("Missing profile. Cannot fetch prefs.");
    }

    if (username == null) {
      throw new IllegalStateException("Missing username. Cannot fetch prefs.");
    }

    final String tokenServerURI = getTokenServerURI();
    if (tokenServerURI == null) {
      throw new IllegalStateException("No token server URI. Cannot fetch prefs.");
    }

    final String fxaServerURI = getAccountServerURI();
    if (fxaServerURI == null) {
      throw new IllegalStateException("No account server URI. Cannot fetch prefs.");
    }

    final String product = GlobalConstants.BROWSER_INTENT_PACKAGE + ".fxa";
    final long version = CURRENT_PREFS_VERSION;

    // This is unique for each syncing 'view' of the account.
    final String serverURLThing = fxaServerURI + "!" + tokenServerURI;
    return Utils.getPrefsPath(product, username, serverURLThing, profile, version);
  }

  public SharedPreferences getSyncPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    return context.getSharedPreferences(getSyncPrefsPath(), Utils.SHARED_PREFERENCES_MODE);
  }

  /**
   * Extract a JSON dictionary of the string values associated to this account.
   * <p>
   * <b>For debugging use only!</b> The contents of this JSON object completely
   * determine the user's Firefox Account status and yield access to whatever
   * user data the device has access to.
   *
   * @return JSON-object of Strings.
   */
  public ExtendedJSONObject toJSONObject() {
    ExtendedJSONObject o = unbundle();
    o.put("email", account.name);
    try {
      o.put("emailUTF8", Utils.byte2Hex(account.name.getBytes("UTF-8")));
    } catch (UnsupportedEncodingException e) {
      // Ignore.
    }
    return o;
  }

  public static AndroidFxAccount addAndroidAccount(
      Context context,
      String email,
      String profile,
      String idpServerURI,
      String tokenServerURI,
      State state)
          throws UnsupportedEncodingException, GeneralSecurityException, URISyntaxException {
    if (email == null) {
      throw new IllegalArgumentException("email must not be null");
    }
    if (idpServerURI == null) {
      throw new IllegalArgumentException("idpServerURI must not be null");
    }
    if (tokenServerURI == null) {
      throw new IllegalArgumentException("tokenServerURI must not be null");
    }
    if (state == null) {
      throw new IllegalArgumentException("state must not be null");
    }

    // Android has internal restrictions that require all values in this
    // bundle to be strings. *sigh*
    Bundle userdata = new Bundle();
    userdata.putString(ACCOUNT_KEY_ACCOUNT_VERSION, "" + CURRENT_ACCOUNT_VERSION);
    userdata.putString(ACCOUNT_KEY_IDP_SERVER, idpServerURI);
    userdata.putString(ACCOUNT_KEY_TOKEN_SERVER, tokenServerURI);
    userdata.putString(ACCOUNT_KEY_AUDIENCE, FxAccountUtils.getAudienceForURL(tokenServerURI));
    userdata.putString(ACCOUNT_KEY_PROFILE, profile);

    ExtendedJSONObject descriptor = new ExtendedJSONObject();

    descriptor.put(BUNDLE_KEY_STATE_LABEL, state.getStateLabel().name());
    descriptor.put(BUNDLE_KEY_STATE, state.toJSONObject().toJSONString());

    descriptor.put(BUNDLE_KEY_BUNDLE_VERSION, CURRENT_BUNDLE_VERSION);
    userdata.putString(ACCOUNT_KEY_DESCRIPTOR, descriptor.toJSONString());

    Account account = new Account(email, FxAccountConstants.ACCOUNT_TYPE);
    AccountManager accountManager = AccountManager.get(context);
    // We don't set an Android password, because we don't want to persist the
    // password (or anything else as powerful as the password). Instead, we
    // internally manage a sessionToken with a remotely owned lifecycle.
    boolean added = accountManager.addAccountExplicitly(account, null, userdata);
    if (!added) {
      return null;
    }

    AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
    fxAccount.clearSyncPrefs();
    fxAccount.enableSyncing();

    return fxAccount;
  }

  public void clearSyncPrefs() throws UnsupportedEncodingException, GeneralSecurityException {
    getSyncPrefs().edit().clear().commit();
  }

  public void enableSyncing() {
    FxAccountAuthenticator.enableSyncing(context, account);
  }

  public void disableSyncing() {
    FxAccountAuthenticator.disableSyncing(context, account);
  }

  public synchronized void setState(State state) {
    if (state == null) {
      throw new IllegalArgumentException("state must not be null");
    }
    updateBundleValue(BUNDLE_KEY_STATE_LABEL, state.getStateLabel().name());
    updateBundleValue(BUNDLE_KEY_STATE, state.toJSONObject().toJSONString());
  }

  public synchronized State getState() {
    String stateLabelString = getBundleData(BUNDLE_KEY_STATE_LABEL);
    String stateString = getBundleData(BUNDLE_KEY_STATE);
    if (stateLabelString == null) {
      throw new IllegalStateException("stateLabelString must not be null");
    }
    if (stateString == null) {
      throw new IllegalStateException("stateString must not be null");
    }

    try {
      StateLabel stateLabel = StateLabel.valueOf(stateLabelString);
      return StateFactory.fromJSONObject(stateLabel, new ExtendedJSONObject(stateString));
    } catch (Exception e) {
      throw new IllegalStateException("could not get state", e);
    }
  }

  /**
   * <b>For debugging only!</b>
   */
  public void dump() {
    if (!FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      return;
    }
    ExtendedJSONObject o = toJSONObject();
    ArrayList<String> list = new ArrayList<String>(o.keySet());
    Collections.sort(list);
    for (String key : list) {
      FxAccountConstants.pii(LOG_TAG, key + ": " + o.get(key));
    }
  }

  /**
   * Return the Firefox Account's local email address.
   * <p>
   * It is important to note that this is the local email address, and not
   * necessarily the normalized remote email address that the server expects.
   *
   * @return local email address.
   */
  public String getEmail() {
    return account.name;
  }
}
