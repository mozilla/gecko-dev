Remote Permissions
==================

The remote permission service offers a simple way to set default permissions
through `remote settings
<https://remote-settings.readthedocs.io/en/latest/introduction.html>`__. For a
general introduction to the permission system, see the :doc:`permission manager
documentation <manager>`.

This mechanism is only meant to be used in combination with permissions that
control exceptions for web compatibility. For example, remote permissions are
used to set permissions of type ``https-only-load-insecure``, allowing
HTTPS-First exceptions to be set through remote settings if a site is known to
be broken with HTTPS-First. A bad example of remote permission would be using
them to set permissions of the type ``uitour``. Permissions of that type grant
sites access to a set of special APIs. These kinds of permissions should be set
directly in source at `browser/app/permissions
<https://searchfox.org/mozilla-central/source/browser/app/permissions>`__.

To limit the types of permissions that are allowed to be set through remote
settings, the permission types that are allowed to be set through remote
permissions are specified `in-source
<https://searchfox.org/mozilla-central/source/extensions/permissions/RemotePermissionService.sys.mjs#:~:text=ALLOWED_PERMISSION_VALUES>`__.
Both updating this allowlist, and adding new remote permissions requires a
review.

Implementing an exception list with remote permissions
----------------------------------------------------------------

If you want to set up a new site exception list for your feature with remote
permissions, you can roughly follow these steps:

1. If it doesn't exist already: Choose a new permission type and set up code
   that checks for that permission type (for example, using the permission
   manager's `testExactPermissionFromPrincipal
   <manager.html#testexactpermissionfromprincipal>`__ method).
2. File bug in `Core :: Permission Manager
   <https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&blocked=remote-permissions&bug_ignored=0&bug_severity=--&bug_status=NEW&bug_type=task&cc=emz%40mozilla.com&cc=maltejur%40mozilla.com&cf_a11y_review_project_flag=---&cf_accessibility_severity=---&cf_fx_iteration=---&cf_fx_points=---&cf_has_str=---&cf_performance_impact=---&cf_status_firefox134=---&cf_status_firefox135=---&cf_status_firefox136=---&cf_status_firefox_esr115=---&cf_status_firefox_esr128=---&cf_status_thunderbird_esr115=---&cf_status_thunderbird_esr128=---&cf_tracking_firefox134=---&cf_tracking_firefox135=---&cf_tracking_firefox136=---&cf_tracking_firefox_esr115=---&cf_tracking_firefox_esr128=---&cf_tracking_firefox_relnote=---&cf_tracking_thunderbird_esr115=---&cf_tracking_thunderbird_esr128=---&cf_webcompat_priority=---&cf_webcompat_score=---&comment=_Remote%20permission%20changes%20for%20this%20permission%20type%20should%20be%20requested%20in%20bugs%20blocking%20this%20bug%20or%20documented%20in%20comments%20on%20this%20bug._%0D%0A%0D%0A_Patches%20updating%20the%20in-source%20allowlist%20should%20be%20attached%20directly%20to%20this%20bug._&component=Permission%20Manager&contenttypemethod=list&contenttypeselection=text%2Fplain&defined_cc=emz%40mozilla.com%2C%20maltejur%40mozilla.com&defined_groups=1&filed_via=standard_form&flag_type-203=X&flag_type-37=X&flag_type-41=X&flag_type-607=X&flag_type-721=X&flag_type-737=X&flag_type-787=X&flag_type-799=X&flag_type-803=X&flag_type-846=X&flag_type-855=X&flag_type-863=X&flag_type-864=X&flag_type-930=X&flag_type-936=X&flag_type-937=X&flag_type-963=X&flag_type-967=X&keywords=leave-open%2Cmeta%2C%20&needinfo_role=other&needinfo_type=needinfo_from&op_sys=Unspecified&priority=--&product=Core&rep_platform=Unspecified&short_desc=%5Bmeta%5D%20Remote%20Permissions%20for%20permission%20type%20%27%3Cpermission%20name%3E%27&target_milestone=---&version=unspecified>`__
   and attach a patch updating ``ALLOWED_PERMISSION_VALUES`` in
   `extensions/permissions/RemotePermissionService.sys.mjs
   <https://searchfox.org/mozilla-central/source/extensions/permissions/RemotePermissionService.sys.mjs#:~:text=ALLOWED_PERMISSION_VALUES>`__
   to include your new permission.
3. For each change to your specific remote permissions, open a bug blocking the
   bug you filed in the step above to request your changes to be added to remote
   settings
4. (Optional) If you expect to regularly make updates to the remote permission
   collection, you can also file a bug in `Infrastructure & Operations ::
   Corporate VPN: ACL requests
   <https://bugzilla.mozilla.org/enter_bug.cgi?product=Infrastructure%20%26%20Operations&component=Corporate%20VPN%3A%20ACL%20requests>`__
   requesting direct access to the `remote settings admin UI
   <https://remote-settings.readthedocs.io/en/latest/getting-started.html>`__
   and the ``remote-permissions`` collection. With that, you can request your
   changes directly in the remote settings admin UI. For transparency reasons,
   we still ask you though to document the changes you make in the bug you filed
   in step 2.
