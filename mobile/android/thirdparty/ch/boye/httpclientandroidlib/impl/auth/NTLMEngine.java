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
package ch.boye.httpclientandroidlib.impl.auth;

/**
 * Abstract NTLM authentication engine. The engine can be used to
 * generate Type1 messages and Type3 messages in response to a
 * Type2 challenge.
 *
 * @since 4.0
 */
public interface NTLMEngine {

    /**
     * Generates a Type1 message given the domain and workstation.
     *
     * @param domain Optional Windows domain name. Can be <code>null</code>.
     * @param workstation Optional Windows workstation name. Can be
     *  <code>null</code>.
     * @return Type1 message
     * @throws NTLMEngineException
     */
    String generateType1Msg(
            String domain,
            String workstation) throws NTLMEngineException;

    /**
     * Generates a Type3 message given the user credentials and the
     * authentication challenge.
     *
     * @param username Windows user name
     * @param password Password
     * @param domain Windows domain name
     * @param workstation Windows workstation name
     * @param challenge Type2 challenge.
     * @return Type3 response.
     * @throws NTLMEngineException
     */
    String generateType3Msg(
            String username,
            String password,
            String domain,
            String workstation,
            String challenge) throws NTLMEngineException;

}
