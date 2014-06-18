/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef nsProtocolProxyService_h__
#define nsProtocolProxyService_h__

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsIProtocolProxyService2.h"
#include "nsIProtocolProxyFilter.h"
#include "nsIProxyInfo.h"
#include "nsIObserver.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "prio.h"
#include "mozilla/Attributes.h"

typedef nsDataHashtable<nsCStringHashKey, uint32_t> nsFailedProxyTable;

class nsProxyInfo;
struct nsProtocolInfo;
class nsIPrefBranch;
class nsISystemProxySettings;
class nsPACMan;

class nsProtocolProxyService MOZ_FINAL : public nsIProtocolProxyService2
                                       , public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLPROXYSERVICE2
    NS_DECL_NSIPROTOCOLPROXYSERVICE
    NS_DECL_NSIOBSERVER

    nsProtocolProxyService();

    nsresult Init();

protected:
    friend class nsAsyncResolveRequest;

    ~nsProtocolProxyService();

    /**
     * This method is called whenever a preference may have changed or
     * to initialize all preferences.
     *
     * @param prefs
     *        This must be a pointer to the root pref branch.
     * @param name
     *        This can be the name of a fully-qualified preference, or it can
     *        be null, in which case all preferences will be initialized.
     */
    void PrefsChanged(nsIPrefBranch *prefs, const char *name);

    /**
     * This method is called to create a nsProxyInfo instance from the given
     * PAC-style proxy string.  It parses up to the end of the string, or to
     * the next ';' character.
     * 
     * @param proxy
     *        The PAC-style proxy string to parse.  This must not be null.
     * @param aResolveFlags
     *        The flags passed to Resolve or AsyncResolve that are stored in 
     *        proxyInfo.
     * @param result
     *        Upon return this points to a newly allocated nsProxyInfo or null
     *        if the proxy string was invalid.
     *
     * @return A pointer beyond the parsed proxy string (never null).
     */
    const char * ExtractProxyInfo(const char *proxy,
                                              uint32_t aResolveFlags,
                                              nsProxyInfo **result);

    /**
     * Load the specified PAC file.
     * 
     * @param pacURI
     *        The URI spec of the PAC file to load.
     */
    nsresult ConfigureFromPAC(const nsCString &pacURI, bool forceReload);

    /**
     * This method builds a list of nsProxyInfo objects from the given PAC-
     * style string.
     *
     * @param pacString
     *        The PAC-style proxy string to parse.  This may be empty.
     * @param aResolveFlags
     *        The flags passed to Resolve or AsyncResolve that are stored in 
     *        proxyInfo.
     * @param result
     *        The resulting list of proxy info objects.
     */
    void ProcessPACString(const nsCString &pacString,
                                      uint32_t aResolveFlags,
                                      nsIProxyInfo **result);

    /**
     * This method generates a string valued identifier for the given
     * nsProxyInfo object.
     *
     * @param pi
     *        The nsProxyInfo object from which to generate the key.
     * @param result
     *        Upon return, this parameter holds the generated key.
     */
    void GetProxyKey(nsProxyInfo *pi, nsCString &result);

    /**
     * @return Seconds since start of session.
     */
    uint32_t SecondsSinceSessionStart();

    /**
     * This method removes the specified proxy from the disabled list.
     *
     * @param pi
     *        The nsProxyInfo object identifying the proxy to enable.
     */
    void EnableProxy(nsProxyInfo *pi);

    /**
     * This method adds the specified proxy to the disabled list.
     *
     * @param pi
     *        The nsProxyInfo object identifying the proxy to disable.
     */
    void DisableProxy(nsProxyInfo *pi);

    /**
     * This method tests to see if the given proxy is disabled.
     *
     * @param pi
     *        The nsProxyInfo object identifying the proxy to test.
     *
     * @return True if the specified proxy is disabled.
     */
    bool IsProxyDisabled(nsProxyInfo *pi);

    /**
     * This method queries the protocol handler for the given scheme to check
     * for the protocol flags and default port.
     *
     * @param uri
     *        The URI to query.
     * @param info
     *        Holds information about the protocol upon return.  Pass address
     *        of structure when you call this method.  This parameter must not
     *        be null.
     */
    nsresult GetProtocolInfo(nsIURI *uri, nsProtocolInfo *result);

    /**
     * This method is an internal version nsIProtocolProxyService::newProxyInfo
     * that expects a string literal for the type.
     *
     * @param type
     *        The proxy type.
     * @param host
     *        The proxy host name (UTF-8 ok).
     * @param port
     *        The proxy port number.
     * @param flags
     *        The proxy flags (nsIProxyInfo::flags).
     * @param timeout
     *        The failover timeout for this proxy.
     * @param next
     *        The next proxy to try if this one fails.
     * @param aResolveFlags
     *        The flags passed to resolve (from nsIProtocolProxyService).
     * @param result
     *        The resulting nsIProxyInfo object.
     */
    nsresult NewProxyInfo_Internal(const char *type,
                                               const nsACString &host,
                                               int32_t port,
                                               uint32_t flags,
                                               uint32_t timeout,
                                               nsIProxyInfo *next,
                                               uint32_t aResolveFlags,
                                               nsIProxyInfo **result);

    /**
     * This method is an internal version of Resolve that does not query PAC.
     * It performs all of the built-in processing, and reports back to the
     * caller with either the proxy info result or a flag to instruct the
     * caller to use PAC instead.
     *
     * @param uri
     *        The URI to test.
     * @param info
     *        Information about the URI's protocol.
     * @param flags
     *        The flags passed to either the resolve or the asyncResolve method.
     * @param usePAC
     *        If this flag is set upon return, then PAC should be queried to
     *        resolve the proxy info.
     * @param result
     *        The resulting proxy info or null.
     */
    nsresult Resolve_Internal(nsIURI *uri,
                                          const nsProtocolInfo &info,
                                          uint32_t flags,
                                          bool *usePAC, 
                                          nsIProxyInfo **result);

    /**
     * This method applies the registered filters to the given proxy info
     * list, and returns a possibly modified list.
     *
     * @param uri
     *        The URI corresponding to this proxy info list.
     * @param info
     *        Information about the URI's protocol.
     * @param proxyInfo
     *        The proxy info list to be modified.  This is an inout param.
     */
    void ApplyFilters(nsIURI *uri, const nsProtocolInfo &info,
                                  nsIProxyInfo **proxyInfo);

    /**
     * This method is a simple wrapper around ApplyFilters that takes the
     * proxy info list inout param as a nsCOMPtr.
     */
    inline void ApplyFilters(nsIURI *uri, const nsProtocolInfo &info,
                             nsCOMPtr<nsIProxyInfo> &proxyInfo)
    {
      nsIProxyInfo *pi = nullptr;
      proxyInfo.swap(pi);
      ApplyFilters(uri, info, &pi);
      proxyInfo.swap(pi);
    }

    /**
     * This method prunes out disabled and disallowed proxies from a given
     * proxy info list.
     *
     * @param info
     *        Information about the URI's protocol.
     * @param proxyInfo
     *        The proxy info list to be modified.  This is an inout param.
     */
    void PruneProxyInfo(const nsProtocolInfo &info,
                                    nsIProxyInfo **proxyInfo);

    /**
     * This method populates mHostFiltersArray from the given string.
     *
     * @param hostFilters
     *        A "no-proxy-for" exclusion list.
     */
    void LoadHostFilters(const char *hostFilters);

    /**
     * This method checks the given URI against mHostFiltersArray.
     *
     * @param uri
     *        The URI to test.
     * @param defaultPort
     *        The default port for the given URI.
     *
     * @return True if the URI can use the specified proxy.
     */
    bool CanUseProxy(nsIURI *uri, int32_t defaultPort);

    /**
     * Disable Prefetch in the DNS service if a proxy is in use.
     *
     * @param aProxy
     *        The proxy information
     */
    void MaybeDisableDNSPrefetch(nsIProxyInfo *aProxy);

private:
    nsresult SetupPACThread();
    nsresult ResetPACThread();

public:
    // The Sun Forte compiler and others implement older versions of the
    // C++ standard's rules on access and nested classes.  These structs
    // need to be public in order to deal with those compilers.

    struct HostInfoIP {
        uint16_t   family;
        uint16_t   mask_len;
        PRIPv6Addr addr; // possibly IPv4-mapped address
    };

    struct HostInfoName {
        char    *host;
        uint32_t host_len;
    };

protected:

    // simplified array of filters defined by this struct
    struct HostInfo {
        bool    is_ipaddr;
        int32_t port;
        union {
            HostInfoIP   ip;
            HostInfoName name;
        };

        HostInfo()
            : is_ipaddr(false)
            { /* other members intentionally uninitialized */ }
       ~HostInfo() {
            if (!is_ipaddr && name.host)
                nsMemory::Free(name.host);
        }
    };

    // This structure is allocated for each registered nsIProtocolProxyFilter.
    struct FilterLink {
      struct FilterLink                *next;
      uint32_t                          position;
      nsCOMPtr<nsIProtocolProxyFilter>  filter;

      FilterLink(uint32_t p, nsIProtocolProxyFilter *f)
        : next(nullptr), position(p), filter(f) {}

      // Chain deletion to simplify cleaning up the filter links
      ~FilterLink() { if (next) delete next; }
    };

    // Indicates if local hosts (plain hostnames, no dots) should use the proxy
    bool mFilterLocalHosts;

    // Holds an array of HostInfo objects
    nsTArray<nsAutoPtr<HostInfo> > mHostFiltersArray;

    // Points to the start of a sorted by position, singly linked list
    // of FilterLink objects.
    FilterLink                  *mFilters;

    uint32_t                     mProxyConfig;

    nsCString                    mHTTPProxyHost;
    int32_t                      mHTTPProxyPort;

    nsCString                    mFTPProxyHost;
    int32_t                      mFTPProxyPort;

    nsCString                    mHTTPSProxyHost;
    int32_t                      mHTTPSProxyPort;
    
    nsCString                    mSOCKSProxyHost;
    int32_t                      mSOCKSProxyPort;
    int32_t                      mSOCKSProxyVersion;
    bool                         mSOCKSProxyRemoteDNS;
    bool                         mProxyOverTLS;

    nsRefPtr<nsPACMan>           mPACMan;  // non-null if we are using PAC
    nsCOMPtr<nsISystemProxySettings> mSystemProxySettings;

    PRTime                       mSessionStart;
    nsFailedProxyTable           mFailedProxies;
    int32_t                      mFailedProxyTimeout;

private:
    nsresult AsyncResolveInternal(nsIURI *uri, uint32_t flags,
                                  nsIProtocolProxyCallback *callback,
                                  nsICancelable **result,
                                  bool isSyncOK);

};

#endif // !nsProtocolProxyService_h__
