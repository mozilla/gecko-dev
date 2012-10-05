/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.receivers;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalConstants;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.config.AccountPickler;
import org.mozilla.gecko.sync.setup.Constants;

import android.accounts.AccountManager;
import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

public class SyncAccountDeletedService extends IntentService {
  public static final String LOG_TAG = "SyncAccountDeletedService";

  public SyncAccountDeletedService() {
    super(LOG_TAG);
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    final Context context = this;

    long intentVersion = intent.getLongExtra(Constants.JSON_KEY_VERSION, 0);
    long expectedVersion = GlobalConstants.SYNC_ACCOUNT_DELETED_INTENT_VERSION;
    if (intentVersion != expectedVersion) {
      Logger.warn(LOG_TAG, "Intent malformed: version " + intentVersion + " given but version " + expectedVersion + "expected. " +
          "Not cleaning up after deleted Account.");
      return;
    }

    String accountName = intent.getStringExtra(Constants.JSON_KEY_ACCOUNT); // Android Account name, not Sync encoded account name.
    if (accountName == null) {
      Logger.warn(LOG_TAG, "Intent malformed: no account name given. Not cleaning up after deleted Account.");
      return;
    }

    // Delete the Account pickle.
    Logger.info(LOG_TAG, "Sync account named " + accountName + " being removed; " +
        "deleting saved pickle file '" + Constants.ACCOUNT_PICKLE_FILENAME + "'.");
    deletePickle(context);
  }

  public static void deletePickle(final Context context) {
    try {
      AccountPickler.deletePickle(context, Constants.ACCOUNT_PICKLE_FILENAME);
    } catch (Exception e) {
      // This should never happen, but we really don't want to die in a background thread.
      Logger.warn(LOG_TAG, "Got exception deleting saved pickle file; ignoring.", e);
    }
  }
}
