/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import java.util.concurrent.BlockingQueue;

import org.mozilla.gecko.fxa.login.State;

import android.content.SyncResult;

public class FxAccountSyncDelegate {
  public enum Result {
    Success,
    Error,
    Postponed,
    Rejected,
  }

  protected final BlockingQueue<Result> latch;
  protected final SyncResult syncResult;

  public FxAccountSyncDelegate(BlockingQueue<Result> latch, SyncResult syncResult) {
    if (latch == null) {
      throw new IllegalArgumentException("latch must not be null");
    }
    if (syncResult == null) {
      throw new IllegalArgumentException("syncResult must not be null");
    }
    this.latch = latch;
    this.syncResult = syncResult;
  }

  /**
   * No error!  Say that we made progress.
   */
  protected void setSyncResultSuccess() {
    syncResult.stats.numUpdates += 1;
  }

  /**
   * Soft error. Say that we made progress, so that Android will sync us again
   * after exponential backoff.
   */
  protected void setSyncResultSoftError() {
    syncResult.stats.numUpdates += 1;
    syncResult.stats.numIoExceptions += 1;
  }

  /**
   * Hard error. We don't want Android to sync us again, even if we make
   * progress, until the user intervenes.
   */
  protected void setSyncResultHardError() {
    syncResult.stats.numAuthExceptions += 1;
  }

  public void handleSuccess() {
    setSyncResultSuccess();
    latch.offer(Result.Success);
  }

  public void handleError(Exception e) {
    setSyncResultSoftError();
    latch.offer(Result.Error);
  }

  /**
   * When the login machine terminates, we might not be in the
   * <code>Married</code> state, and therefore we can't sync. This method
   * messages as much to the user.
   * <p>
   * To avoid stopping us syncing altogether, we set a soft error rather than
   * a hard error. In future, we would like to set a hard error if we are in,
   * for example, the <code>Separated</code> state, and then have some user
   * initiated activity mark the Android account as ready to sync again. This
   * is tricky, though, so we play it safe for now.
   *
   * @param finalState
   *          that login machine ended in.
   */
  public void handleCannotSync(State finalState) {
    setSyncResultSoftError();
    latch.offer(Result.Error);
  }

  public void postponeSync(long millis) {
    if (millis > 0) {
      // delayUntil is broken: https://code.google.com/p/android/issues/detail?id=65669
      // So we don't bother doing this. Instead, we rely on the periodic sync
      // we schedule, and the backoff handler for the rest.
      /*
      Logger.warn(LOG_TAG, "Postponing sync by " + millis + "ms.");
      syncResult.delayUntil = millis / 1000;
       */
    }
    setSyncResultSoftError();
    latch.offer(Result.Postponed);
  }

  /**
   * Simply don't sync, without setting any error flags.
   * This is the appropriate behavior when a routine backoff has not yet
   * been met.
   */
  public void rejectSync() {
    latch.offer(Result.Rejected);
  }
}
