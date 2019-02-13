/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.tasks;

import java.io.UnsupportedEncodingException;
import java.util.Map;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.PasswordStretcher;

import android.content.Context;

public class FxAccountCreateAccountTask extends FxAccountSetupTask<LoginResponse> {
  private static final String LOG_TAG = FxAccountCreateAccountTask.class.getSimpleName();

  protected final byte[] emailUTF8;
  protected final PasswordStretcher passwordStretcher;

  public FxAccountCreateAccountTask(Context context, ProgressDisplay progressDisplay, String email, PasswordStretcher passwordStretcher, FxAccountClient client, Map<String, String> queryParameters, RequestDelegate<LoginResponse> delegate) throws UnsupportedEncodingException {
    super(context, progressDisplay, client, queryParameters, delegate);
    this.emailUTF8 = email.getBytes("UTF-8");
    this.passwordStretcher = passwordStretcher;
  }

  @Override
  protected InnerRequestDelegate<LoginResponse> doInBackground(Void... arg0) {
    try {
      client.createAccountAndGetKeys(emailUTF8, passwordStretcher, queryParameters, innerDelegate);
      latch.await();
      return innerDelegate;
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Got exception logging in.", e);
      delegate.handleError(e);
    }
    return null;
  }
}
