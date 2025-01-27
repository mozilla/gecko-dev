Permission Manager
==================

The Firefox permission manager offers a simple way to store user preferences
(“permissions” or “exceptions”) on a per-origin basis. On the most basic level,
a permission consists of the following:

1. The **origin** for which the permission applicable. This could for example be
   https://example.com.
2. The permission **type**. This can be an arbitrary string, for example “geo”
   to allow geolocation access on the specified site.
3. The permission **value**. Unless specified otherwise, this value is either
   ``0`` (“Undefined”), ``1`` (“Allow”), ``2`` (“Deny”) or ``3`` (“Prompt the user”).

For storing arbitrary preferences per origin instead of just permission values,
the `content pref service
<https://searchfox.org/mozilla-central/source/dom/interfaces/base/nsIContentPrefService2.idl>`__
offers a good alternative to the permission manager. There also exists the `site
permission manager
<https://searchfox.org/mozilla-central/source/browser/modules/SitePermissions.sys.mjs>`__,
which builds on top of the regular permission manager, and makes temporary
permissions that are not stored to disk, and user interfaces easier.

Interfacing with the Permission Manager
---------------------------------------

The permission manager can be accessed through the ``nsIPermissionManager``
interface. This interface is available through the
``@mozilla.org/permissionmanager;1`` service, or through the quick
``Services.perms`` getter in JavaScript. Below is a list of the most common
methods, and examples on how to use them with JavaScript. For a full list of
signatures, see `nsIPermissionManager.idl
<https://searchfox.org/mozilla-central/source/netwerk/base/nsIPermissionManager.idl>`__.

``testExactPermissionFromPrincipal``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns any possible stored permission value for a given origin and permission
type. To also find permission values if the given origin is a subdomain of the
permission's origin, use the otherwise identical ``testPermissionFromPrincipal``
method.

.. code:: js

  // Only construct the principal yourself if you can't get from somewhere else
  // (e.g. gBrowser.contentPrincipal) directly.
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.org"
  );

  let perm = Services.perms.testExactPermissionFromPrincipal(principal, "geo");
  if (perm == Services.perms.ALLOW_ACTION) {
    // Do things
  }


``addFromPrincipal``
~~~~~~~~~~~~~~~~~~~~

Adds a permission to the permission manager for a given origin, permission type
and value. Optionally, the permission can be configured to expire after the
current browsing session ends, or to expire on a given UNIX timestamp in
milliseconds.

.. code:: js

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.org"
  );

  // Never expires
  Services.perms.addFromPrincipal(principal, "geo", Services.perms.ALLOW_ACTION);

  // Expires after the current session
  Services.perms.addFromPrincipal(
    principal,
    "geo",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_SESSION
  );

  // Expires in 24 hours
  Services.perms.addFromPrincipal(
    principal,
    "geo",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_TIME,
    Date.now() + 1000 * 60 * 60 * 24
  );


``removeFromPrincipal``
~~~~~~~~~~~~~~~~~~~~~~~

Removes a permission from the permission manager for a given origin and
permission type.

.. code:: js

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.org"
  );

  Services.perms.removeFromPrincipal(principal, "geo");
