============================
Web Security Checks in Gecko
============================

Key Concepts and Terminology
=============================

Security Principal (nsIPrincipal)
---------------------------------

A Security Principal represents the security context for a piece of code or data. Firefox uses four types of principals:

- **ContentPrincipal**: Used for typical web pages and can be serialized to an origin URL, e.g., https://example.com/.
- **NullPrincipal**: Used for pages that are never same-origin with anything else, such as iframes with the sandbox attribute or documents loaded with a data: URI. This is also known as an opaque origin.
- **SystemPrincipal**: Used for the browser's user interface, commonly referred to as "browser chrome", and various other background services (OCSP requests, fetching favicons). Pages like about:preferences use the SystemPrincipal.
- **ExpandedPrincipal**: Used by browser extensions that need to assume the security context of a website. An ExpandedPrincipal is best understood as a list of principals.

OriginAttributes
----------------------------

`OriginAttributes` help in managing and enforcing security policies by distinguishing different security contexts that might otherwise be considered the same based on their Principal. They are used to:

- Isolate data and resources in private browsing mode.
- Implement cache isolation.
- Manage user context identifiers for container tabs.
- Enforce first-party isolation.

Attributes
----------

The `OriginAttributes` class extends the functionality of `dom::OriginAttributesDictionary` and includes additional methods for setting and managing various attributes.

Key attributes include:

- **FirstPartyDomain**: Used to isolate data based on the domain.
- **UserContextId**: Identifies different user contexts, such as container tabs.
- **PrivateBrowsingId**: Indicates whether a request is made in private browsing mode.
- **PartitionKey**: Used to implement cache isolation.


Load Info Object (nsILoadInfo)
------------------------------

The `nsILoadInfo` object is crucial for security checks. It holds all security-relevant attributes, including security flags indicating what checks need to be performed and the associated Principal.

Attributes:
-----------

- `loadingPrincipal`: The principal of the document where the result of the load will be used.
- `triggeringPrincipal`: The principal that triggered the URL to load.
- `securityFlags`: Indicate the type of security checks required.
- `contentPolicyType`: Specifies the type of content being loaded, used for security checks like Content Security Policy.

Loading Lifecycle in Firefox
============================

From Request to Response
------------------------

1. **Request Initiation**: A web page initiates a request.
2. **nsIChannel Creation**: Firefox creates an `nsIChannel` object, representing the request.
3. **nsILoadInfo Attachment**: An `nsILoadInfo` object is required for the creation of an `nsIChannel` and holds security-related information.
4. **Security Checks**: Security checks are performed using the `ContentSecurityManager`.
5. **Request Execution**: If all checks pass, the request proceeds.

Role of nsIChannel and nsILoadInfo
----------------------------------

- **nsIChannel**: Manages the transport algorithm (e.g., HTTP, WebSocket).
- **nsILoadInfo**: Holds security relevant meta information of a network load and determines what security checks need to be enforced.


Security Checks During Loading
==============================

Pre-Request Checks
------------------

- **Same-Origin Policy**: Ensures resources are only accessed if they share the same origin.
- **Content Security Policy**: Enforces content restrictions based on policies defined by the site.
- **Mixed Content Blocking**: Implements the Mixed Content standard, including blocking and upgrading of insecure (HTTP) content on secure (HTTPS) pages.

ContentSecurityManager and doContentSecurityCheck()
---------------------------------------------------

- **ContentSecurityManager**: Centralized manager for performing security checks.
- **PerformSecurityCheck()**: Key function that is invoked to perform all relevant security checks before a request is executed.

Subsumes Concept
----------------

- **Definition**: A principal subsumes another if it has access to the same resources.
- **Implementation**: `aPrincipal->Subsumes(aOtherPrincipal)` is used to check access permissions.

Code example::

    bool subsumes = principal1->Subsumes(principal2);

Subsumption is asymmetrical. One principal subsuming the other does not imply the inverse. A typical example is the `SystemPrincipal`, which subsumes all other
principals.

References
----------
The interface definition in source code have a lot of detailed comments:

- The `nsIPrincipal <https://searchfox.org/mozilla-central/source/caps/nsIPrincipal.idl>`_ interface definition.
- The `nsILoadInfo <https://searchfox.org/mozilla-central/source/netwerk/base/nsILoadInfo.idl>`_ interface definition.
- The `nsIContentSecurityManager <https://searchfox.org/mozilla-central/source/dom/interfaces/security/nsIContentSecurityManager.idl>`_ interface definition
