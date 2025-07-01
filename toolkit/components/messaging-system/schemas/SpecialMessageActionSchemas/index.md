# User Actions

A subset of actions are available to messages via fields like `action` on buttons for CFRs.

## Usage

For CFRs, you should add the action `type` in `action` and any additional parameters in `data`. For example:

```json
"action": {
  "type": "OPEN_PREFERENCES_PAGE",
  "data": { "category": "sync" },
}
```

## Available Actions

### `OPEN_APPLICATIONS_MENU`

* args: (none)

Opens the applications menu.

### `OPEN_FIREFOX_VIEW`

* args: (none)

Opens the Firefox View pseudo-tab.

### `OPEN_PRIVATE_BROWSER_WINDOW`

* args: (none)

Opens a new private browsing window.

### `OPEN_SIDEBAR`

* args: `string` id of the pane to open, e.g., viewHistorySidebar

Opens a sidebar pane.

### `OPEN_URL`

* args: `string` (a url)

Opens a given url.

Example:

```json
"action": {
  "type": "OPEN_URL",
  "data": { "args": "https://foo.com" },
}
```

### `OPEN_ABOUT_PAGE`

* args:
```ts
{
  args: string, // (a valid about page without the `about:` prefix)
  entrypoint?: string, // URL search param used for referrals
}
```

Opens a given about page

Example:

```json
"action": {
  "type": "OPEN_ABOUT_PAGE",
  "data": { "args": "privatebrowsing" },
}
```

### `OPEN_PREFERENCES_PAGE`

* args:
```
{
  args?: string, // (a category accessible via a `#`)
  entrypoint?: string // URL search param used to referrals

Opens `about:preferences` with an optional category accessible via a `#` in the URL (e.g. `about:preferences#home`).

Example:

```json
"action": {
  "type": "OPEN_PREFERENCES_PAGE",
  "data": { "category": "general-cfrfeatures" },
}
```

### `SHOW_FIREFOX_ACCOUNTS`

* args: (none)

Opens Firefox accounts sign-up page. Encodes some information that the origin was from snippets by default.

### `FXA_SIGNIN_FLOW`

* args:

```ts
{
  // a valid `where` value for `openUILinkIn`. Only `tab` and `window` have been tested, and `tabshifted`
  // is unlikely to do anything different from `tab`.
  where?: "tab" | "window" = "tab",

  entrypoint?: string // URL search params string to pass along to FxA. Defaults to "activity-stream-firstrun".
  extraParams?: object // Extra parameters to pass along to FxA. See FxAccountsConfig.promiseConnectAccountURI.
}
```

* example:
```json
"action": {
  "type": "FXA_SIGNIN_FLOW",
  "needsAwait": true,
  "navigate": "actionResult",
  "data": {
    "entrypoint": "onboarding",
    "extraParams": {
      "utm_content": "migration-onboarding"
    }
  }
}
```

Opens a Firefox accounts sign-up or sign-in page, and does the work of closing the resulting tab or window once
sign-in completes. Returns a Promise that resolves to `true` if sign-in succeeded, or to `false` if the sign-in
window or tab closed before sign-in could be completed. In messaging surfaces using `aboutwelcome` templates, setting `needsAwait` ensures that the UI will wait for the Promise to resolve. The `navigate` and `dismiss` properties should be assigned the string value "actionResult" for the UI to respect the resolved boolean value before proceeding to the next step.

Encodes some information that the origin was from about:welcome by default.


### `SHOW_MIGRATION_WIZARD`

* args: (none)

Opens import wizard to bring in settings and data from another browser.

### `PIN_CURRENT_TAB`

* args: (none)

Pins the currently focused tab.

### `HIGHLIGHT_FEATURE`

Can be used to highlight (show a light blue overlay) a certain button or part of the browser UI.

* args: `string` a [valid targeting defined in the UITour](https://searchfox.org/mozilla-central/rev/7fd1c1c34923ece7ad8c822bee062dd0491d64dc/browser/components/uitour/UITour.jsm#108)

### `INSTALL_ADDON_FROM_URL`

Can be used to install an addon from addons.mozilla.org.

* args:
```ts
{
  url: string,
  telemetrySource?: string
};
```

### `OPEN_PROTECTION_REPORT`

Opens `about:protections`

### `OPEN_PROTECTION_PANEL`

Opens the protection panel behind on the lock icon of the awesomebar

### `DISABLE_STP_DOORHANGERS`

Disables all Social Tracking Protection messages

* args: (none)

### `OPEN_AWESOME_BAR`

Focuses and expands the awesome bar.

* args: (none)

### `CANCEL`

No-op action used to dismiss CFR notifications (but not remove or block them)

* args: (none)

### `DISABLE_DOH`

User action for turning off the DoH feature

* args: (none)

### `ACCEPT_DOH`

User action for continuing to use the DoH feature

* args: (none)

### `CONFIGURE_HOMEPAGE`

Action for configuring the user homepage and restoring defaults.

* args:
```ts
{
  homePage: "default" | null;
  newtab: "default" | null;
  layout: {
    search: boolean;
    topsites: boolean;
    highlights: boolean;
    topstories: boolean;
  }
}
```

### `PIN_FIREFOX_TO_TASKBAR`

Action for pinning Firefox to the user's taskbar.

* args: (none)

### `PIN_FIREFOX_TO_START_MENU`

Action for pinning Firefox to the user's Windows Start Menu in Windows MSIX builds only.

- args: (none)

### `SET_DEFAULT_BROWSER`

Action for setting the default browser to Firefox on the user's system.

- args: (none)

### `SET_DEFAULT_PDF_HANDLER`

Action for setting the default PDF handler to Firefox on the user's system.

Windows only.

- args:
```ts
{
  // Only set Firefox as the default PDF handler if the current PDF handler is a
  // known browser.
  onlyIfKnownBrowser?: boolean;
}
```

### `DECLINE_DEFAULT_PDF_HANDLER`

Action for declining to set the default PDF handler to Firefox on the user's
system. Prevents the user from being asked again about this.

Windows only.

- args: (none)

### `SHOW_SPOTLIGHT`

Action for opening a spotlight tab or window modal using the content passed to the dialog.

### `BLOCK_MESSAGE`

Disable a message by adding to an indexedDb list of blocked messages

* args: `string` id of the message

### `SET_PREF`

Action for setting various browser prefs

Prefs that can be changed with this action are:

- `browser.dataFeatureRecommendations.enabled`
- `browser.migrate.content-modal.about-welcome-behavior`
- `browser.migrate.content-modal.import-all.enabled`
- `browser.migrate.preferences-entrypoint.enabled`
- `browser.startup.homepage`
- `browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt`
- `browser.privateWindowSeparation.enabled`
- `browser.firefox-view.feature-tour`
- `browser.pdfjs.feature-tour`
- `browser.newtab.feature-tour`
- `cookiebanners.service.mode`
- `cookiebanners.service.mode.privateBrowsing`
- `cookiebanners.service.detectOnly`
- `messaging-system.askForFeedback`

Any pref that begins with `messaging-system-action.` is also allowed.
Alternatively, if the pref is not present in the list above and does not begin
with `messaging-system-action.`, it will be created and prepended with
`messaging-system-action.`. For example, `example.pref` will be created as
`messaging-system-action.example.pref`.

* args:
```ts
{
  pref: {
    name: string;
    value: string | boolean | number;
  }
}
```

### `MULTI_ACTION`

Action for running multiple actions. Actions should be included in an array of actions.

* args:
```ts
interface MultiAction {
  type: "MULTI_ACTION";
  data: {
    actions: Array<UserAction>;
    // Set to true if the actions should be executed in the order they are
    // listed in the `actions` array. If false, the actions will be executed in
    // parallel, with no guarantee of order. Defaults to false. If collectSelect
    // is true and there are multiselect actions, they will be executed in the
    // order they are rendered in the UI.
    orderedExecution?: boolean;
  };
  // Set to true if this action is for the primary button and you're using the
  // "multiselect" tile. This is what allows the primary button to perform the
  // actions specified by the user's checkbox/radio selections. It will combine
  // all the actions for all the selected checkboxes/radios into the above
  // `actions` array before executing them.
  collectSelect?: boolean;
}
```

* example:
```json
"action": {
  "type": "MULTI_ACTION",
  "data": {
    "actions": [
      {
        "type": "OPEN_URL",
        "args": "https://www.example.com"
      },
      {
        "type": "OPEN_AWESOME_BAR"
      }
    ],
    "orderedExecution": true
  }
}
```

### `CLICK_ELEMENT`

* args: `string` A CSS selector for the HTML element to be clicked

Selects an element in the current Window's document and triggers a click action


### `RELOAD_BROWSER`

* args: (none)

Action for reloading the current browser.


### `FOCUS_URLBAR`

Focuses the urlbar in the window the message was displayed in

* args: (none)

### `BOOKMARK_CURRENT_TAB`

Bookmarks the tab that was selected when the message was displayed

- args:
```ts
{
  // Whether the bookmark dialog should be visible or not.
  shouldHideDialog?: boolean;
  // Whether the bookmark confirmation hint should be visible or not.
  shouldHideConfirmationHint?: boolean;
}
```

### `SET_BOOKMARKS_TOOLBAR_VISIBILITY`

Sets the visibility of the bookmarks toolbar.

- args:
```ts
{
  visibility?: string; // "always", "never", or "newtab"
}
```


### `DATAREPORTING_NOTIFY_DATA_POLICY_INTERACTED`

Notify Firefox that the notification policy was interacted with.

- args: (none)

### `CREATE_NEW_SELECTABLE_PROFILE`

Creates a new user profile and launches it in a separate instance.

Any message that uses this action should have `canCreateSelectableProfiles` as part of the targeting, to ensure we don't accidentally show a message where the action will not work.

- args: (none)

### `SUBMIT_ONBOARDING_OPT_OUT_PING`

Submits a Glean `onboarding-opt-out` ping.  Should only be used during preonboarding (but this is not enforced).

- args: (none)

### `SET_SEARCH_MODE`

Sets search mode for a specific browser instance and focuses the urlbar.

- args:

```ts
interface SearchMode {
  // The name of the search engine to restrict to. Can be left empty to use source
  // restriction instead.
  engineName?: string;
  // A result source to restrict to. One of the values in UrlbarUtils.RESULT_SOURCE.
  // Defaults to 3 (SEARCH).
  source?: number;
  // How search mode was entered. This is recorded in event telemetry. One of the
  // values in UrlbarUtils.SEARCH_MODE_ENTRY. Defaults to "other".
  entry?: string;
  // If true, we will preview search mode. Search mode preview does not record
  // telemetry and has slighly different UI behavior. The preview is exited in
  // favor of full search mode when a query is executed. False should be
  // passed if the caller needs to enter search mode but expects it will not
  // be interacted with right away. Defaults to true.
  isPreview?: boolean;
}
```

* example:

```json
"action": {
  "type": "SET_SEARCH_MODE",
  "data": {
    "engineName": "test_engine",
    "source": 3,
    "entry": "other",
    "isPreview": false,
  }
}
```

### `SUMMARIZE_PAGE`

Summarize current page content.

* args: optional `string` entry value to identify initiator default "message"
