/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
});

const IPV4_PORT_EXPR = /:\d+$/;

const SAMESITE_MAP = new Map([
  ["None", Ci.nsICookie.SAMESITE_NONE],
  ["Lax", Ci.nsICookie.SAMESITE_LAX],
  ["Strict", Ci.nsICookie.SAMESITE_STRICT],
]);

/** @namespace */
export const cookie = {
  manager: Services.cookies,
};

/**
 * @name Cookie
 *
 * @returns {Record<string, (number|boolean|string)>}
 */

/**
 * Unmarshal a JSON Object to a cookie representation.
 *
 * Effectively this will run validation checks on ``json``, which
 * will produce the errors expected by WebDriver if the input is
 * not valid.
 *
 * @param {Record<string, (number | boolean | string)>} json
 *     Cookie to be deserialised. ``name`` and ``value`` are required
 *     fields which must be strings.  The ``path`` and ``domain`` fields
 *     are optional, but must be a string if provided.  The ``secure``,
 *     and ``httpOnly`` are similarly optional, but must be booleans.
 *     Likewise, the ``expiry`` field is optional but must be
 *     unsigned integer.
 *
 * @returns {Cookie}
 *     Valid cookie object.
 *
 * @throws {InvalidArgumentError}
 *     If any of the properties are invalid.
 */
cookie.fromJSON = function (json) {
  let newCookie = {};

  lazy.assert.object(
    json,
    lazy.pprint`Expected "cookie" to be an object, got ${json}`
  );

  newCookie.name = lazy.assert.string(
    json.name,
    lazy.pprint`Expected cookie "name" to be a string, got ${json.name}`
  );
  newCookie.value = lazy.assert.string(
    json.value,
    lazy.pprint`Expected cookie "value" to be a string, got ${json.value}`
  );

  if (typeof json.path != "undefined") {
    newCookie.path = lazy.assert.string(
      json.path,
      lazy.pprint`Expected cookie "path" to be a string, got ${json.path}`
    );
  }
  if (typeof json.domain != "undefined") {
    newCookie.domain = lazy.assert.string(
      json.domain,
      lazy.pprint`Expected cookie "domain" to be a string, got ${json.domain}`
    );
  }
  if (typeof json.secure != "undefined") {
    newCookie.secure = lazy.assert.boolean(
      json.secure,
      lazy.pprint`Expected cookie "secure" to be a boolean, got ${json.secure}`
    );
  }
  if (typeof json.httpOnly != "undefined") {
    newCookie.httpOnly = lazy.assert.boolean(
      json.httpOnly,
      lazy.pprint`Expected cookie "httpOnly" to be a boolean, got ${json.httpOnly}`
    );
  }
  if (typeof json.expiry != "undefined") {
    newCookie.expiry = lazy.assert.positiveInteger(
      json.expiry,
      lazy.pprint`Expected cookie "expiry" to be a positive integer, got ${json.expiry}`
    );
  }
  if (typeof json.sameSite != "undefined") {
    const validOptions = Array.from(SAMESITE_MAP.keys());
    newCookie.sameSite = lazy.assert.in(
      json.sameSite,
      validOptions,
      `Expected cookie "sameSite" to be one of ${validOptions.toString()}, ` +
        lazy.pprint`got ${json.sameSite}`
    );
  }

  return newCookie;
};

/**
 * Insert cookie to the cookie store.
 *
 * @param {Cookie} newCookie
 *     Cookie to add.
 * @param {object} options
 * @param {string=} options.restrictToHost
 *     Perform test that ``newCookie``'s domain matches this.
 * @param {string=} options.protocol
 *     The protocol of the caller. It can be `http:` or `https:`.
 *
 * @throws {TypeError}
 *     If ``name``, ``value``, or ``domain`` are not present and
 *     of the correct type.
 * @throws {InvalidCookieDomainError}
 *     If ``restrictToHost`` is set and ``newCookie``'s domain does
 *     not match.
 * @throws {UnableToSetCookieError}
 *     If an error occurred while trying to save the cookie.
 */
cookie.add = function (
  newCookie,
  { restrictToHost = null, protocol = null } = {}
) {
  lazy.assert.string(
    newCookie.name,
    lazy.pprint`Expected cookie "name" to be a string, got ${newCookie.name}`
  );
  lazy.assert.string(
    newCookie.value,
    lazy.pprint`Expected cookie "value" to be a string, got ${newCookie.value}`
  );

  if (typeof newCookie.path == "undefined") {
    newCookie.path = "/";
  }

  let hostOnly = false;
  if (typeof newCookie.domain == "undefined") {
    hostOnly = true;
    newCookie.domain = restrictToHost;
  }
  lazy.assert.string(
    newCookie.domain,
    lazy.pprint`Expected cookie "domain" to be a string, got ${newCookie.domain}`
  );
  if (newCookie.domain.substring(0, 1) === ".") {
    newCookie.domain = newCookie.domain.substring(1);
  }

  if (typeof newCookie.secure == "undefined") {
    newCookie.secure = false;
  }
  if (typeof newCookie.httpOnly == "undefined") {
    newCookie.httpOnly = false;
  }
  if (typeof newCookie.expiry == "undefined") {
    // The XPCOM interface requires the expiry field even for session cookies.
    newCookie.expiry = Number.MAX_SAFE_INTEGER;
    newCookie.session = true;
  } else {
    newCookie.session = false;
  }
  newCookie.sameSite = SAMESITE_MAP.get(newCookie.sameSite || "None");

  let isIpAddress = false;
  try {
    Services.eTLD.getPublicSuffixFromHost(newCookie.domain);
  } catch (e) {
    switch (e.result) {
      case Cr.NS_ERROR_HOST_IS_IP_ADDRESS:
        isIpAddress = true;
        break;
      default:
        throw new lazy.error.InvalidCookieDomainError(newCookie.domain);
    }
  }

  if (!hostOnly && !isIpAddress) {
    // only store this as a domain cookie if the domain was specified in the
    // request and it wasn't an IP address.
    newCookie.domain = "." + newCookie.domain;
  }

  if (restrictToHost) {
    if (
      !restrictToHost.endsWith(newCookie.domain) &&
      "." + restrictToHost !== newCookie.domain &&
      restrictToHost !== newCookie.domain
    ) {
      throw new lazy.error.InvalidCookieDomainError(
        `Cookies may only be set ` +
          `for the current domain (${restrictToHost})`
      );
    }
  }

  let schemeType = Ci.nsICookie.SCHEME_UNSET;
  switch (protocol) {
    case "http:":
      schemeType = Ci.nsICookie.SCHEME_HTTP;
      break;
    case "https:":
      schemeType = Ci.nsICookie.SCHEME_HTTPS;
      break;
    default:
      // Any other protocol that is supported by the cookie service.
      break;
  }

  // remove port from domain, if present.
  // unfortunately this catches IPv6 addresses by mistake
  // TODO: Bug 814416
  newCookie.domain = newCookie.domain.replace(IPV4_PORT_EXPR, "");

  try {
    cookie.manager.add(
      newCookie.domain,
      newCookie.path,
      newCookie.name,
      newCookie.value,
      newCookie.secure,
      newCookie.httpOnly,
      newCookie.session,
      newCookie.expiry,
      {} /* origin attributes */,
      newCookie.sameSite,
      schemeType
    );
  } catch (e) {
    throw new lazy.error.UnableToSetCookieError(e);
  }
};

/**
 * Remove cookie from the cookie store.
 *
 * @param {Cookie} toDelete
 *     Cookie to remove.
 */
cookie.remove = function (toDelete) {
  cookie.manager.remove(
    toDelete.domain,
    toDelete.name,
    toDelete.path,
    {} /* originAttributes */
  );
};

/**
 * Iterates over the cookies for the current ``host``.  You may
 * optionally filter for specific paths on that ``host`` by specifying
 * a path in ``currentPath``.
 *
 * @param {string} host
 *     Hostname to retrieve cookies for.
 * @param {string=} [currentPath="/"] currentPath
 *     Optionally filter the cookies for ``host`` for the specific path.
 *     Defaults to ``/``, meaning all cookies for ``host`` are included.
 *
 * @returns {Iterable.<Cookie>}
 *     Iterator.
 */
cookie.iter = function* (host, currentPath = "/") {
  lazy.assert.string(
    host,
    lazy.pprint`Expected "host" to be a string, got ${host}`
  );
  lazy.assert.string(
    currentPath,
    lazy.pprint`Expected "currentPath" to be a string, got ${currentPath}`
  );

  const isForCurrentPath = path => currentPath.includes(path);

  let cookies = cookie.manager.getCookiesFromHost(host, {});
  for (let cookie of cookies) {
    // take the hostname and progressively shorten
    let hostname = host;
    do {
      if (
        (cookie.host == "." + hostname || cookie.host == hostname) &&
        isForCurrentPath(cookie.path)
      ) {
        let data = {
          name: cookie.name,
          value: cookie.value,
          path: cookie.path,
          domain: cookie.host,
          secure: cookie.isSecure,
          httpOnly: cookie.isHttpOnly,
        };

        if (!cookie.isSession) {
          data.expiry = cookie.expiry;
        }

        data.sameSite = [...SAMESITE_MAP].find(
          ([, value]) => cookie.sameSite === value
        )[0];

        yield data;
      }
      hostname = hostname.replace(/^.*?\./, "");
    } while (hostname.includes("."));
  }
};
