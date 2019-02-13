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
package ch.boye.httpclientandroidlib.impl.conn.tsccm;

import java.io.IOException;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import ch.boye.httpclientandroidlib.androidextra.HttpClientAndroidLog;
/* LogFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.annotation.GuardedBy;
import ch.boye.httpclientandroidlib.conn.ConnectionPoolTimeoutException;
import ch.boye.httpclientandroidlib.conn.OperatedClientConnection;
import ch.boye.httpclientandroidlib.conn.routing.HttpRoute;
import ch.boye.httpclientandroidlib.impl.conn.IdleConnectionHandler;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * An abstract connection pool.
 * It is used by the {@link ThreadSafeClientConnManager}.
 * The abstract pool includes a {@link #poolLock}, which is used to
 * synchronize access to the internal pool datastructures.
 * Don't use <code>synchronized</code> for that purpose!
 *
 * @since 4.0
 *
 * @deprecated (4.2) use {@link ch.boye.httpclientandroidlib.pool.AbstractConnPool}
 */
@Deprecated
public abstract class AbstractConnPool {

    public HttpClientAndroidLog log;

    /**
     * The global lock for this pool.
     */
    protected final Lock poolLock;

    /** References to issued connections */
    @GuardedBy("poolLock")
    protected Set<BasicPoolEntry> leasedConnections;

    /** The current total number of connections. */
    @GuardedBy("poolLock")
    protected int numConnections;

    /** Indicates whether this pool is shut down. */
    protected volatile boolean isShutDown;

    protected Set<BasicPoolEntryRef> issuedConnections;

    protected ReferenceQueue<Object> refQueue;

    protected IdleConnectionHandler idleConnHandler;

    /**
     * Creates a new connection pool.
     */
    protected AbstractConnPool() {
        super();
        this.log = new HttpClientAndroidLog(getClass());
        this.leasedConnections = new HashSet<BasicPoolEntry>();
        this.idleConnHandler = new IdleConnectionHandler();
        this.poolLock = new ReentrantLock();
    }

    public void enableConnectionGC()
        throws IllegalStateException {
    }

    /**
     * Obtains a pool entry with a connection within the given timeout.
     *
     * @param route     the route for which to get the connection
     * @param timeout   the timeout, 0 or negative for no timeout
     * @param tunit     the unit for the <code>timeout</code>,
     *                  may be <code>null</code> only if there is no timeout
     *
     * @return  pool entry holding a connection for the route
     *
     * @throws ConnectionPoolTimeoutException
     *         if the timeout expired
     * @throws InterruptedException
     *         if the calling thread was interrupted
     */
    public final
        BasicPoolEntry getEntry(
                final HttpRoute route,
                final Object state,
                final long timeout,
                final TimeUnit tunit)
                    throws ConnectionPoolTimeoutException, InterruptedException {
        return requestPoolEntry(route, state).getPoolEntry(timeout, tunit);
    }

    /**
     * Returns a new {@link PoolEntryRequest}, from which a {@link BasicPoolEntry}
     * can be obtained, or the request can be aborted.
     */
    public abstract PoolEntryRequest requestPoolEntry(HttpRoute route, Object state);


    /**
     * Returns an entry into the pool.
     * The connection of the entry is expected to be in a suitable state,
     * either open and re-usable, or closed. The pool will not make any
     * attempt to determine whether it can be re-used or not.
     *
     * @param entry     the entry for the connection to release
     * @param reusable  <code>true</code> if the entry is deemed
     *                  reusable, <code>false</code> otherwise.
     * @param validDuration The duration that the entry should remain free and reusable.
     * @param timeUnit The unit of time the duration is measured in.
     */
    public abstract void freeEntry(BasicPoolEntry entry, boolean reusable, long validDuration, TimeUnit timeUnit)
        ;

    public void handleReference(final Reference<?> ref) {
    }

    protected abstract void handleLostEntry(HttpRoute route);

    /**
     * Closes idle connections.
     *
     * @param idletime  the time the connections should have been idle
     *                  in order to be closed now
     * @param tunit     the unit for the <code>idletime</code>
     */
    public void closeIdleConnections(final long idletime, final TimeUnit tunit) {

        // idletime can be 0 or negative, no problem there
        Args.notNull(tunit, "Time unit");

        poolLock.lock();
        try {
            idleConnHandler.closeIdleConnections(tunit.toMillis(idletime));
        } finally {
            poolLock.unlock();
        }
    }

    public void closeExpiredConnections() {
        poolLock.lock();
        try {
            idleConnHandler.closeExpiredConnections();
        } finally {
            poolLock.unlock();
        }
    }


    /**
     * Deletes all entries for closed connections.
     */
    public abstract void deleteClosedConnections();

    /**
     * Shuts down this pool and all associated resources.
     * Overriding methods MUST call the implementation here!
     */
    public void shutdown() {

        poolLock.lock();
        try {

            if (isShutDown) {
                return;
            }

            // close all connections that are issued to an application
            final Iterator<BasicPoolEntry> iter = leasedConnections.iterator();
            while (iter.hasNext()) {
                final BasicPoolEntry entry = iter.next();
                iter.remove();
                closeConnection(entry.getConnection());
            }
            idleConnHandler.removeAll();

            isShutDown = true;

        } finally {
            poolLock.unlock();
        }
    }


    /**
     * Closes a connection from this pool.
     *
     * @param conn      the connection to close, or <code>null</code>
     */
    protected void closeConnection(final OperatedClientConnection conn) {
        if (conn != null) {
            try {
                conn.close();
            } catch (final IOException ex) {
                log.debug("I/O error closing connection", ex);
            }
        }
    }

} // class AbstractConnPool

