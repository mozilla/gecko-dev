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

package ch.boye.httpclientandroidlib.protocol;

/**
 * Thread-safe extension of the {@link BasicHttpContext}.
 *
 * @since 4.0
 *
 * @deprecated (4.2) HttpContext instances may not be shared by multiple threads
 */
@Deprecated
public class SyncBasicHttpContext extends BasicHttpContext {

    public SyncBasicHttpContext(final HttpContext parentContext) {
        super(parentContext);
    }

    /**
     * @since 4.2
     */
    public SyncBasicHttpContext() {
        super();
    }

    @Override
    public synchronized Object getAttribute(final String id) {
        return super.getAttribute(id);
    }

    @Override
    public synchronized void setAttribute(final String id, final Object obj) {
        super.setAttribute(id, obj);
    }

    @Override
    public synchronized Object removeAttribute(final String id) {
        return super.removeAttribute(id);
    }

    /**
     * @since 4.2
     */
    @Override
    public synchronized void clear() {
        super.clear();
    }

}
