XPCOMUtils module
=================

XPCOMUtils is a utility class that lives in
:searchfox:`resource://gre/modules/XPCOMUtils.sys.mjs <js/xpconnect/loader/XPCOMUtils.sys.mjs>`.

Its name is a little confusing because it does not live in XPCOM itself,
but it provides utilities that help JS consumers to make consuming
``sys.mjs`` modules and XPCOM services/interfaces more ergonomic.

.. js:autoclass:: XPCOMUtils
  :members:
