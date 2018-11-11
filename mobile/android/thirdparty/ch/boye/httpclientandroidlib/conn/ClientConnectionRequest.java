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

package ch.boye.httpclientandroidlib.conn;

import java.util.concurrent.TimeUnit;

/**
 * Encapsulates a request for a {@link ManagedClientConnection}.
 *
 * @since 4.0
 *
 * @deprecated (4.3) replaced by {@link ConnectionRequest}.
 */
@Deprecated
public interface ClientConnectionRequest {

    /**
     * Obtains a connection within a given time.
     * This method will block until a connection becomes available,
     * the timeout expires, or the connection manager is
     * {@link ClientConnectionManager#shutdown() shut down}.
     * Timeouts are handled with millisecond precision.
     *
     * If {@link #abortRequest()} is called while this is blocking or
     * before this began, an {@link InterruptedException} will
     * be thrown.
     *
     * @param timeout   the timeout, 0 or negative for no timeout
     * @param tunit     the unit for the <code>timeout</code>,
     *                  may be <code>null</code> only if there is no timeout
     *
     * @return  a connection that can be used to communicate
     *          along the given route
     *
     * @throws ConnectionPoolTimeoutException
     *         in case of a timeout
     * @throws InterruptedException
     *         if the calling thread is interrupted while waiting
     */
    ManagedClientConnection getConnection(long timeout, TimeUnit tunit)
        throws InterruptedException, ConnectionPoolTimeoutException;

    /**
     * Aborts the call to {@link #getConnection(long, TimeUnit)},
     * causing it to throw an {@link InterruptedException}.
     */
    void abortRequest();

}
