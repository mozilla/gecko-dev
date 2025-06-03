/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ProxyTypes: "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "protocolProxyService",
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);

const nsIProtocolProxyChannelFilter = ChromeUtils.generateQI([
  Ci.nsIProtocolProxyChannelFilter,
]);

// The maximum uint32 value.
const PR_UINT32_MAX = 4294967295;

/**
 * A ProxyPerUserContextManager class keeps track of user contexts and their proxy configuration.
 */
export class ProxyPerUserContextManager {
  #channelProxyFilter;
  #proxyFilterRegistered;
  #userContextToProxyConfiguration;

  constructor() {
    this.#proxyFilterRegistered = false;

    // A map between internal user context ids and proxy configurations.
    this.#userContextToProxyConfiguration = new Map();
  }

  destroy() {
    this.#userContextToProxyConfiguration = new Map();

    this.#unregisterProxyFilter();
  }

  /**
   * Add proxy configuration for a provided user context id.
   *
   * @param {string} userContextId
   *     Internal user context id.
   * @param {Proxy} proxy
   *     Proxy configuration.
   */
  addConfiguration(userContextId, proxy) {
    this.#userContextToProxyConfiguration.set(userContextId, proxy);

    this.#registerProxyFilter();
  }

  /**
   * Delete proxy configuration for a provided user context id.
   *
   * @param {string} userContextId
   *     Internal user context id.
   */
  deleteConfiguration(userContextId) {
    this.#userContextToProxyConfiguration.delete(userContextId);

    this.#unregisterProxyFilter();
  }

  #addProxyFilter(proxyFilter, proxySettings) {
    const { host, port, type } = proxySettings;

    proxyFilter.onProxyFilterResult(
      lazy.protocolProxyService.newProxyInfo(
        type,
        host,
        port,
        "" /* aProxyAuthorizationHeader */,
        "" /* aConnectionIsolationKey */,
        Ci.nsIProxyInfo.TRANSPARENT_PROXY_RESOLVES_HOST /* aFlags */,
        PR_UINT32_MAX /* aFailoverTimeout */,
        null /* failover proxy */
      )
    );
  }

  #applyFilter = (channel, defaultProxyInfo, proxyFilter) => {
    const originAttributes =
      channel.loadInfo && channel.loadInfo.originAttributes;

    if (
      this.#userContextToProxyConfiguration.has(originAttributes.userContextId)
    ) {
      const proxyInfo = this.#userContextToProxyConfiguration.get(
        originAttributes.userContextId
      );

      if (proxyInfo.proxyType === lazy.ProxyTypes.Direct) {
        proxyFilter.onProxyFilterResult(null);

        return;
      }

      if (proxyInfo.proxyType === lazy.ProxyTypes.Manual) {
        const channelURI = channel.originalURI;
        if (
          proxyInfo.noProxy?.length &&
          proxyInfo.noProxy.includes(channelURI.host)
        ) {
          proxyFilter.onProxyFilterResult(defaultProxyInfo);

          return;
        }

        if (channelURI.schemeIs("http") && proxyInfo.httpProxy) {
          this.#addProxyFilter(proxyFilter, {
            host: proxyInfo.httpProxy,
            port: proxyInfo.httpProxyPort,
            type: "http",
          });
        }

        if (channelURI.schemeIs("https") && proxyInfo.sslProxy) {
          this.#addProxyFilter(proxyFilter, {
            host: proxyInfo.sslProxy,
            port: proxyInfo.sslProxyPort,
            type: "https",
          });
        }

        if (channelURI.schemeIs(undefined) && proxyInfo.socksProxy) {
          this.#addProxyFilter(proxyFilter, {
            host: proxyInfo.socksProxy,
            port: proxyInfo.socksProxyPort,
            type:
              proxyInfo.socksVersion === 5
                ? "socks"
                : `socks${proxyInfo.socksVersion}`,
          });
        }

        return;
      }
    }

    proxyFilter.onProxyFilterResult(defaultProxyInfo);
  };

  #registerProxyFilter() {
    if (!this.#proxyFilterRegistered) {
      this.#proxyFilterRegistered = true;

      this.#channelProxyFilter = {
        QueryInterface: nsIProtocolProxyChannelFilter,
        applyFilter: this.#applyFilter,
      };

      lazy.protocolProxyService.registerChannelFilter(
        this.#channelProxyFilter,
        0 /* set position `0` to override global filters */
      );
    }
  }

  #unregisterProxyFilter() {
    if (
      this.#proxyFilterRegistered &&
      this.#userContextToProxyConfiguration.size === 0
    ) {
      this.#proxyFilterRegistered = false;
      lazy.protocolProxyService.unregisterChannelFilter(
        this.#channelProxyFilter
      );
      this.#channelProxyFilter = null;
    }
  }
}
