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
import java.util.LinkedList;
import java.util.ListIterator;
import java.util.Queue;

import ch.boye.httpclientandroidlib.androidextra.HttpClientAndroidLog;
/* LogFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.conn.OperatedClientConnection;
import ch.boye.httpclientandroidlib.conn.params.ConnPerRoute;
import ch.boye.httpclientandroidlib.conn.routing.HttpRoute;
import ch.boye.httpclientandroidlib.util.Args;
import ch.boye.httpclientandroidlib.util.Asserts;
import ch.boye.httpclientandroidlib.util.LangUtils;


/**
 * A connection sub-pool for a specific route, used by {@link ConnPoolByRoute}.
 * The methods in this class are unsynchronized. It is expected that the
 * containing pool takes care of synchronization.
 *
 * @since 4.0
 *
 * @deprecated (4.2)  use {@link ch.boye.httpclientandroidlib.pool.AbstractConnPool}
 */
@Deprecated
public class RouteSpecificPool {

    public HttpClientAndroidLog log = new HttpClientAndroidLog(getClass());

    /** The route this pool is for. */
    protected final HttpRoute route; //Immutable

    protected final int maxEntries;

    /** Connections per route */
    protected final ConnPerRoute connPerRoute;

    /**
     * The list of free entries.
     * This list is managed LIFO, to increase idle times and
     * allow for closing connections that are not really needed.
     */
    protected final LinkedList<BasicPoolEntry> freeEntries;

    /** The list of threads waiting for this pool. */
    protected final Queue<WaitingThread> waitingThreads;

    /** The number of created entries. */
    protected int numEntries;

    /**
     * @deprecated (4.1)  use {@link RouteSpecificPool#RouteSpecificPool(HttpRoute, ConnPerRoute)}
     */
    @Deprecated
    public RouteSpecificPool(final HttpRoute route, final int maxEntries) {
        this.route = route;
        this.maxEntries = maxEntries;
        this.connPerRoute = new ConnPerRoute() {
            public int getMaxForRoute(final HttpRoute route) {
                return RouteSpecificPool.this.maxEntries;
            }
        };
        this.freeEntries = new LinkedList<BasicPoolEntry>();
        this.waitingThreads = new LinkedList<WaitingThread>();
        this.numEntries = 0;
    }


    /**
     * Creates a new route-specific pool.
     *
     * @param route the route for which to pool
     * @param connPerRoute the connections per route configuration
     */
    public RouteSpecificPool(final HttpRoute route, final ConnPerRoute connPerRoute) {
        this.route = route;
        this.connPerRoute = connPerRoute;
        this.maxEntries = connPerRoute.getMaxForRoute(route);
        this.freeEntries = new LinkedList<BasicPoolEntry>();
        this.waitingThreads = new LinkedList<WaitingThread>();
        this.numEntries = 0;
    }


    /**
     * Obtains the route for which this pool is specific.
     *
     * @return  the route
     */
    public final HttpRoute getRoute() {
        return route;
    }


    /**
     * Obtains the maximum number of entries allowed for this pool.
     *
     * @return  the max entry number
     */
    public final int getMaxEntries() {
        return maxEntries;
    }


    /**
     * Indicates whether this pool is unused.
     * A pool is unused if there is neither an entry nor a waiting thread.
     * All entries count, not only the free but also the allocated ones.
     *
     * @return  <code>true</code> if this pool is unused,
     *          <code>false</code> otherwise
     */
    public boolean isUnused() {
        return (numEntries < 1) && waitingThreads.isEmpty();
    }


    /**
     * Return remaining capacity of this pool
     *
     * @return capacity
     */
    public int getCapacity() {
        return connPerRoute.getMaxForRoute(route) - numEntries;
    }


    /**
     * Obtains the number of entries.
     * This includes not only the free entries, but also those that
     * have been created and are currently issued to an application.
     *
     * @return  the number of entries for the route of this pool
     */
    public final int getEntryCount() {
        return numEntries;
    }


    /**
     * Obtains a free entry from this pool, if one is available.
     *
     * @return an available pool entry, or <code>null</code> if there is none
     */
    public BasicPoolEntry allocEntry(final Object state) {
        if (!freeEntries.isEmpty()) {
            final ListIterator<BasicPoolEntry> it = freeEntries.listIterator(freeEntries.size());
            while (it.hasPrevious()) {
                final BasicPoolEntry entry = it.previous();
                if (entry.getState() == null || LangUtils.equals(state, entry.getState())) {
                    it.remove();
                    return entry;
                }
            }
        }
        if (getCapacity() == 0 && !freeEntries.isEmpty()) {
            final BasicPoolEntry entry = freeEntries.remove();
            entry.shutdownEntry();
            final OperatedClientConnection conn = entry.getConnection();
            try {
                conn.close();
            } catch (final IOException ex) {
                log.debug("I/O error closing connection", ex);
            }
            return entry;
        }
        return null;
    }


    /**
     * Returns an allocated entry to this pool.
     *
     * @param entry     the entry obtained from {@link #allocEntry allocEntry}
     *                  or presented to {@link #createdEntry createdEntry}
     */
    public void freeEntry(final BasicPoolEntry entry) {
        if (numEntries < 1) {
            throw new IllegalStateException
                ("No entry created for this pool. " + route);
        }
        if (numEntries <= freeEntries.size()) {
            throw new IllegalStateException
                ("No entry allocated from this pool. " + route);
        }
        freeEntries.add(entry);
    }


    /**
     * Indicates creation of an entry for this pool.
     * The entry will <i>not</i> be added to the list of free entries,
     * it is only recognized as belonging to this pool now. It can then
     * be passed to {@link #freeEntry freeEntry}.
     *
     * @param entry     the entry that was created for this pool
     */
    public void createdEntry(final BasicPoolEntry entry) {
        Args.check(route.equals(entry.getPlannedRoute()), "Entry not planned for this pool");
        numEntries++;
    }


    /**
     * Deletes an entry from this pool.
     * Only entries that are currently free in this pool can be deleted.
     * Allocated entries can not be deleted.
     *
     * @param entry     the entry to delete from this pool
     *
     * @return  <code>true</code> if the entry was found and deleted, or
     *          <code>false</code> if the entry was not found
     */
    public boolean deleteEntry(final BasicPoolEntry entry) {

        final boolean found = freeEntries.remove(entry);
        if (found) {
            numEntries--;
        }
        return found;
    }


    /**
     * Forgets about an entry from this pool.
     * This method is used to indicate that an entry
     * {@link #allocEntry allocated}
     * from this pool has been lost and will not be returned.
     */
    public void dropEntry() {
        Asserts.check(numEntries > 0, "There is no entry that could be dropped");
        numEntries--;
    }


    /**
     * Adds a waiting thread.
     * This pool makes no attempt to match waiting threads with pool entries.
     * It is the caller's responsibility to check that there is no entry
     * before adding a waiting thread.
     *
     * @param wt        the waiting thread
     */
    public void queueThread(final WaitingThread wt) {
        Args.notNull(wt, "Waiting thread");
        this.waitingThreads.add(wt);
    }


    /**
     * Checks whether there is a waiting thread in this pool.
     *
     * @return  <code>true</code> if there is a waiting thread,
     *          <code>false</code> otherwise
     */
    public boolean hasThread() {
        return !this.waitingThreads.isEmpty();
    }


    /**
     * Returns the next thread in the queue.
     *
     * @return  a waiting thread, or <code>null</code> if there is none
     */
    public WaitingThread nextThread() {
        return this.waitingThreads.peek();
    }


    /**
     * Removes a waiting thread, if it is queued.
     *
     * @param wt        the waiting thread
     */
    public void removeThread(final WaitingThread wt) {
        if (wt == null) {
            return;
        }

        this.waitingThreads.remove(wt);
    }


} // class RouteSpecificPool
