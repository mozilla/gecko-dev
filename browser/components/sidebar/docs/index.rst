.. _components/sidebar:

=========
Sidebar
=========

The new sidebar builds on existing legacy sidebar code treating ``browser-sidebar.js`` as a `SidebarController`. As part of the new sidebar and vertical tabs project, we've added new components including top-level system module `SidebarManager.sys.mjs` and a per window state manager, `SidebarState.sys.ms`. We've added new UI components that use a combination of moz components and custom lit components. The sidebar team maintains the existing synced tabs and history panels.

Introducing a new panel
~~~~~~~~~~~~~~~~~~~~~~~

Every panel that is registered and enabled in ``browser-sidebar.js``` and the ```defaultTools``` map will show as an option in the Customize Sidebar menu (which is a sidebar panel that contains settings).

The launcher is a container for tools (ie, icons that when clicked open or close the associated panel). Registering a panel - which should be behind a pref until it is ready to be introduced - does not automatically add a new icon to the launcher.

A tool can be added once for all users by adding it to the designated pref branch ``sidebar.newTool.migration.`` in ``profile/firefox.js``. So an example would be ``pref("sidebar.newTool.migration.bookmarks", '{}')``.  The pref suffix (``bookmarks`` in this example) is the ``toolID`` that should match what you added as the value portion of the relevant entry in the ``defaultTools`` map in ``browser-sidebar.js``. It's important to note that if you have a pref governing the visibility of your sidebar panel, it will need to be enabled at the same time in order to be shown in a user's launcher - either via a nimbus rollout or in-tree.

If you only want to add this item if the pref governing visibility is true, you can pass the pref you want to observe, e.g. ``pref("sidebar.newTool.migration.reviewchecker", '{ "visibilityPref": "browser.shopping.experience2023.integratedSidebar"}')`` where ``browser.shopping.experience2023.integratedSidebar`` is the pref controlling the visibility of the review checker panel.

In both cases, the tool will be introduced to the launcher one time (appended to a user's customized list of tools) and any customization after that (ie, removing it) takes precedence. If it's not removed, it will persist after that session.
