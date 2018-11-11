/*
 * ====================================================================
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */
package ch.boye.httpclientandroidlib.impl.conn;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.InetAddress;
import java.net.Socket;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocket;

import ch.boye.httpclientandroidlib.HttpConnectionMetrics;
import ch.boye.httpclientandroidlib.HttpEntityEnclosingRequest;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManager;
import ch.boye.httpclientandroidlib.conn.ManagedClientConnection;
import ch.boye.httpclientandroidlib.conn.OperatedClientConnection;
import ch.boye.httpclientandroidlib.protocol.HttpContext;

/**
 * Abstract adapter from {@link OperatedClientConnection operated} to
 * {@link ManagedClientConnection managed} client connections.
 * Read and write methods are delegated to the wrapped connection.
 * Operations affecting the connection state have to be implemented
 * by derived classes. Operations for querying the connection state
 * are delegated to the wrapped connection if there is one, or
 * return a default value if there is none.
 * <p>
 * This adapter tracks the checkpoints for reusable communication states,
 * as indicated by {@link #markReusable markReusable} and queried by
 * {@link #isMarkedReusable isMarkedReusable}.
 * All send and receive operations will automatically clear the mark.
 * <p>
 * Connection release calls are delegated to the connection manager,
 * if there is one. {@link #abortConnection abortConnection} will
 * clear the reusability mark first. The connection manager is
 * expected to tolerate multiple calls to the release method.
 *
 * @since 4.0
 *
 * @deprecated (4.2)  do not use
 */
@Deprecated
@NotThreadSafe
public abstract class AbstractClientConnAdapter implements ManagedClientConnection, HttpContext {

    /**
     * The connection manager.
     */
    private final ClientConnectionManager connManager;

    /** The wrapped connection. */
    private volatile OperatedClientConnection wrappedConnection;

    /** The reusability marker. */
    private volatile boolean markedReusable;

    /** True if the connection has been shut down or released. */
    private volatile boolean released;

    /** The duration this is valid for while idle (in ms). */
    private volatile long duration;

    /**
     * Creates a new connection adapter.
     * The adapter is initially <i>not</i>
     * {@link #isMarkedReusable marked} as reusable.
     *
     * @param mgr       the connection manager, or <code>null</code>
     * @param conn      the connection to wrap, or <code>null</code>
     */
    protected AbstractClientConnAdapter(final ClientConnectionManager mgr,
                                        final OperatedClientConnection conn) {
        super();
        connManager = mgr;
        wrappedConnection = conn;
        markedReusable = false;
        released = false;
        duration = Long.MAX_VALUE;
    }

    /**
     * Detaches this adapter from the wrapped connection.
     * This adapter becomes useless.
     */
    protected synchronized void detach() {
        wrappedConnection = null;
        duration = Long.MAX_VALUE;
    }

    protected OperatedClientConnection getWrappedConnection() {
        return wrappedConnection;
    }

    protected ClientConnectionManager getManager() {
        return connManager;
    }

    /**
     * @deprecated (4.1)  use {@link #assertValid(OperatedClientConnection)}
     */
    @Deprecated
    protected final void assertNotAborted() throws InterruptedIOException {
        if (isReleased()) {
            throw new InterruptedIOException("Connection has been shut down");
        }
    }

    /**
     * @since 4.1
     * @return value of released flag
     */
    protected boolean isReleased() {
        return released;
    }

    /**
     * Asserts that there is a valid wrapped connection to delegate to.
     *
     * @throws ConnectionShutdownException if there is no wrapped connection
     *                                  or connection has been aborted
     */
    protected final void assertValid(
            final OperatedClientConnection wrappedConn) throws ConnectionShutdownException {
        if (isReleased() || wrappedConn == null) {
            throw new ConnectionShutdownException();
        }
    }

    public boolean isOpen() {
        final OperatedClientConnection conn = getWrappedConnection();
        if (conn == null) {
            return false;
        }

        return conn.isOpen();
    }

    public boolean isStale() {
        if (isReleased()) {
            return true;
        }
        final OperatedClientConnection conn = getWrappedConnection();
        if (conn == null) {
            return true;
        }

        return conn.isStale();
    }

    public void setSocketTimeout(final int timeout) {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        conn.setSocketTimeout(timeout);
    }

    public int getSocketTimeout() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        return conn.getSocketTimeout();
    }

    public HttpConnectionMetrics getMetrics() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        return conn.getMetrics();
    }

    public void flush() throws IOException {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        conn.flush();
    }

    public boolean isResponseAvailable(final int timeout) throws IOException {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        return conn.isResponseAvailable(timeout);
    }

    public void receiveResponseEntity(final HttpResponse response)
        throws HttpException, IOException {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        unmarkReusable();
        conn.receiveResponseEntity(response);
    }

    public HttpResponse receiveResponseHeader()
        throws HttpException, IOException {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        unmarkReusable();
        return conn.receiveResponseHeader();
    }

    public void sendRequestEntity(final HttpEntityEnclosingRequest request)
        throws HttpException, IOException {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        unmarkReusable();
        conn.sendRequestEntity(request);
    }

    public void sendRequestHeader(final HttpRequest request)
        throws HttpException, IOException {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        unmarkReusable();
        conn.sendRequestHeader(request);
    }

    public InetAddress getLocalAddress() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        return conn.getLocalAddress();
    }

    public int getLocalPort() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        return conn.getLocalPort();
    }

    public InetAddress getRemoteAddress() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        return conn.getRemoteAddress();
    }

    public int getRemotePort() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        return conn.getRemotePort();
    }

    public boolean isSecure() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        return conn.isSecure();
    }

    public void bind(final Socket socket) throws IOException {
        throw new UnsupportedOperationException();
    }

    public Socket getSocket() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        if (!isOpen()) {
            return null;
        }
        return conn.getSocket();
    }

    public SSLSession getSSLSession() {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        if (!isOpen()) {
            return null;
        }

        SSLSession result = null;
        final Socket    sock    = conn.getSocket();
        if (sock instanceof SSLSocket) {
            result = ((SSLSocket)sock).getSession();
        }
        return result;
    }

    public void markReusable() {
        markedReusable = true;
    }

    public void unmarkReusable() {
        markedReusable = false;
    }

    public boolean isMarkedReusable() {
        return markedReusable;
    }

    public void setIdleDuration(final long duration, final TimeUnit unit) {
        if(duration > 0) {
            this.duration = unit.toMillis(duration);
        } else {
            this.duration = -1;
        }
    }

    public synchronized void releaseConnection() {
        if (released) {
            return;
        }
        released = true;
        connManager.releaseConnection(this, duration, TimeUnit.MILLISECONDS);
    }

    public synchronized void abortConnection() {
        if (released) {
            return;
        }
        released = true;
        unmarkReusable();
        try {
            shutdown();
        } catch (final IOException ignore) {
        }
        connManager.releaseConnection(this, duration, TimeUnit.MILLISECONDS);
    }

    public Object getAttribute(final String id) {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        if (conn instanceof HttpContext) {
            return ((HttpContext) conn).getAttribute(id);
        } else {
            return null;
        }
    }

    public Object removeAttribute(final String id) {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        if (conn instanceof HttpContext) {
            return ((HttpContext) conn).removeAttribute(id);
        } else {
            return null;
        }
    }

    public void setAttribute(final String id, final Object obj) {
        final OperatedClientConnection conn = getWrappedConnection();
        assertValid(conn);
        if (conn instanceof HttpContext) {
            ((HttpContext) conn).setAttribute(id, obj);
        }
    }

}
