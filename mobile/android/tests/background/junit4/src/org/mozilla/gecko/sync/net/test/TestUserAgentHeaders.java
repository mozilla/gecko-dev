/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.net.test;

import ch.boye.httpclientandroidlib.protocol.HTTP;
import junit.framework.Assert;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20.RecoveryEmailStatusResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

import java.util.concurrent.Executors;

@RunWith(TestRunner.class)
public class TestUserAgentHeaders {
  private static final int TEST_PORT = HTTPServerTestHelper.getTestPort();
  private static final String TEST_SERVER = "http://localhost:" + TEST_PORT;

  protected final HTTPServerTestHelper data = new HTTPServerTestHelper();

  protected class UserAgentServer extends MockServer {
    public String lastUserAgent = null;

    @Override
    public void handle(Request request, Response response) {
      lastUserAgent = request.getValue(HTTP.USER_AGENT);
      super.handle(request, response);
    }
  }

  protected final UserAgentServer userAgentServer = new UserAgentServer();

  @Before
  public void setUp() {
    BaseResource.rewriteLocalhost = false;
    data.startHTTPServer(userAgentServer);
  }

  @After
  public void tearDown() {
    data.stopHTTPServer();
  }

  @Test
  public void testSyncUserAgent() throws Exception {
    final SyncStorageRecordRequest request = new SyncStorageRecordRequest(TEST_SERVER);
    request.delegate = new SyncStorageRequestDelegate() {
      @Override
      public String ifUnmodifiedSince() {
        return null;
      }

      @Override
      public void handleRequestSuccess(SyncStorageResponse response) {
        WaitHelper.getTestWaiter().performNotify();
      }

      @Override
      public void handleRequestFailure(SyncStorageResponse response) {
        WaitHelper.getTestWaiter().performNotify();
      }

      @Override
      public void handleRequestError(Exception ex) {
        WaitHelper.getTestWaiter().performNotify();
      }

      @Override
      public AuthHeaderProvider getAuthHeaderProvider() {
        return null;
      }
    };

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        request.get();
      }
    });

    // Verify that we're getting the value from the correct place.
    Assert.assertEquals(SyncConstants.USER_AGENT, userAgentServer.lastUserAgent);
  }

  @Test
  public void testFxAccountClientUserAgent() throws Exception {
    final FxAccountClient20 client = new FxAccountClient20(TEST_SERVER, Executors.newSingleThreadExecutor());
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        client.recoveryEmailStatus(new byte[] { 0 }, new RequestDelegate<RecoveryEmailStatusResponse>() {
          @Override
          public void handleSuccess(RecoveryEmailStatusResponse result) {
            WaitHelper.getTestWaiter().performNotify();
          }

          @Override
          public void handleFailure(FxAccountClientRemoteException e) {
            WaitHelper.getTestWaiter().performNotify();
          }

          @Override
          public void handleError(Exception e) {
            WaitHelper.getTestWaiter().performNotify();
          }
        });
      }
    });

    // Verify that we're getting the value from the correct place.
    Assert.assertEquals(FxAccountConstants.USER_AGENT, userAgentServer.lastUserAgent);
  }
}
