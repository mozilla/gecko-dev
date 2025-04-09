/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @flow

// This file provides simple utils to deal with JWT tokens.
// It was copied from https://github.com/firefox-devtools/profiler/blob/main/src/utils/jwt.js
// and transformed to use typescript jsdocs instead flow types (even though it's
// not TS-checked at the moment).

/**
 * @param {string} jwtToken
 * @returns {unknown}
 */
export function extractAndDecodePayload(jwtToken) {
  if (!isValidJwtToken(jwtToken)) {
    console.error("The token is an invalid JWT token.");
    return null;
  }

  try {
    const payload = jwtToken.split(".")[1];
    const decodedPayload = decodeJwtBase64Url(payload);
    const jsonPayload = JSON.parse(decodedPayload);

    return jsonPayload;
  } catch (e) {
    console.error(
      `We got an unexpected error when trying to decode the JWT token '${jwtToken}':`,
      e
    );
    return null;
  }
}

// A JWT token is composed of 3 parts, separated by a period.
// These parts all use the base64url characters, that is base64 characters where
// "+" is replaced by "-", and "/" is replaced by "_". Moreover the padding
// character "=" isn't used with JWT.
// Here is an example:
// eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJuYW1lIjoiP34_fj9-In0.KIumXQmDxL1bJ0RGNV2-mm-8h0LEQATKbtHUsCHMGcg
//                  ╰ header                        ╰ payload                     ╰ signature
const JWT_TOKEN_RE =
  /^(?:[a-zA-Z0-9_-])+\.(?:[a-zA-Z0-9_-])+\.(?:[a-zA-Z0-9_-])+$/;

/**
 * @param {string} jwtToken
 * @returns {boolean}
 */
export function isValidJwtToken(jwtToken) {
  return JWT_TOKEN_RE.test(jwtToken);
}

/**
 * @param {string} base64UrlEncodedValue
 * @returns {string}
 */
export function decodeJwtBase64Url(base64UrlEncodedValue) {
  // In the base64url variant used in JWT, the padding "=" character is removed.
  // But atob doesn't mind, so we don't need to recover the missing padding like
  // most implementations do.

  // We do need to convert the string to a "normal" base64 encoding though.
  const base64EncodedValue = base64UrlEncodedValue.replace(/[-_]/g, match => {
    // prettier-ignore
    switch (match) {
      case "-": return "+";
      case "_": return "/";
      default: throw new Error(`Unexpected match value ${match}`);
    }
  });

  return atob(base64EncodedValue);
}

/**
 * This function returns a profile token from a JWT token, if the passed string
 * looks like a JWT token. Otherwise it just returns the passed string because
 * this would be the hash directly, as returned by a previous version of the
 * server.
 * In the future when the server will be migrated we'll be able to remove this
 * fallback.
 *
 * @param {string} hashOrToken
 * @returns {string}
 */
export function extractProfileTokenFromJwt(hashOrToken) {
  if (isValidJwtToken(hashOrToken)) {
    // This is a JWT token, let's extract the hash out of it.
    const jwtPayload = extractAndDecodePayload(hashOrToken);
    if (!jwtPayload) {
      throw new Error(
        `The JWT token that's been returned by the server is incorrect.`
      );
    }

    const { profileToken } = jwtPayload;
    if (!profileToken) {
      throw new Error(
        `The JWT token returned by the server doesn't contain a profile token.`
      );
    }
    return profileToken;
  }

  // Then this is a good old hash.
  return hashOrToken;
}
