/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.net.Socket;

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;

import ch.boye.httpclientandroidlib.conn.ssl.SSLSocketFactory;
import ch.boye.httpclientandroidlib.params.HttpParams;

public class TLSSocketFactory extends SSLSocketFactory {
  private static final String LOG_TAG = "TLSSocketFactory";

  // Guarded by `this`.
  private static String[] cipherSuites = GlobalConstants.DEFAULT_CIPHER_SUITES;

  public TLSSocketFactory(SSLContext sslContext) {
    super(sslContext);
  }

  /**
   * Attempt to specify the cipher suites to use for a connection. If
   * setting fails (as it will on Android 2.2, because the wrong names
   * are in use to specify ciphers), attempt to set the defaults.
   *
   * We store the list of cipher suites in `cipherSuites`, which
   * avoids this fallback handling having to be executed more than once.
   *
   * This method is synchronized to ensure correct use of that member.
   *
   * See Bug 717691 for more details.
   *
   * @param socket
   *        The SSLSocket on which to operate.
   */
  public static synchronized void setEnabledCipherSuites(SSLSocket socket) {
    try {
      socket.setEnabledCipherSuites(cipherSuites);
    } catch (IllegalArgumentException e) {
      cipherSuites = socket.getSupportedCipherSuites();
      Logger.warn(LOG_TAG, "Setting enabled cipher suites failed: " + e.getMessage());
      Logger.warn(LOG_TAG, "Using " + cipherSuites.length + " supported suites.");
      socket.setEnabledCipherSuites(cipherSuites);
    }
  }

  @Override
  public Socket createSocket(HttpParams params) throws IOException {
    SSLSocket socket = (SSLSocket) super.createSocket(params);
    socket.setEnabledProtocols(GlobalConstants.DEFAULT_PROTOCOLS);
    setEnabledCipherSuites(socket);
    return socket;
  }
}
