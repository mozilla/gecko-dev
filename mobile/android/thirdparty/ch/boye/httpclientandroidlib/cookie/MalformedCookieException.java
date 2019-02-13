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

package ch.boye.httpclientandroidlib.cookie;

import ch.boye.httpclientandroidlib.ProtocolException;
import ch.boye.httpclientandroidlib.annotation.Immutable;

/**
 * Signals that a cookie is in some way invalid or illegal in a given
 * context
 *
 *
 * @since 4.0
 */
@Immutable
public class MalformedCookieException extends ProtocolException {

    private static final long serialVersionUID = -6695462944287282185L;

    /**
     * Creates a new MalformedCookieException with a <tt>null</tt> detail message.
     */
    public MalformedCookieException() {
        super();
    }

    /**
     * Creates a new MalformedCookieException with a specified message string.
     *
     * @param message The exception detail message
     */
    public MalformedCookieException(final String message) {
        super(message);
    }

    /**
     * Creates a new MalformedCookieException with the specified detail message and cause.
     *
     * @param message the exception detail message
     * @param cause the <tt>Throwable</tt> that caused this exception, or <tt>null</tt>
     * if the cause is unavailable, unknown, or not a <tt>Throwable</tt>
     */
    public MalformedCookieException(final String message, final Throwable cause) {
        super(message, cause);
    }
}
