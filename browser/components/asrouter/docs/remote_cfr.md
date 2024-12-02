# Remote CFR Messages
Starting in Firefox 68, CFR messages will be defined using [Remote Settings](https://remote-settings.readthedocs.io/en/latest/index.html).


## Environments
There are three environments available for Remote Settings:
* DEV - `https://remote-settings-dev.allizom.org/v1/`
* STAGING - `https://remote-settings.allizom.org/v1/`
* PROD - `https://remote-settings.mozilla.org/v1/`

DEV is primarily used for testing the Remote Settings API and exploring new use cases. Messaging validation and QA are conducted in the STAGING environment, while finalized and validated messages are delivered in the PROD environment. Both STAGING and PROD follow the [Multi Signoff Workflow](https://remote-settings.readthedocs.io/en/latest/tutorial-multi-signoff.html). During the review process, messages are served in the `main-preview` bucket. Once reviewed, they are moved to the `main` bucket.

Access to STAGING and PROD environments requires VPN (using "Viscosity VPN," which can be requested via the Jira Service Desk). However, the DEV environment is accessible without VPN.

To switch between environments more easily, you can use the [remote-settings-devtools](https://github.com/mozilla-extensions/remote-settings-devtools) extension. After installation, click on the extension in your browser to open its UI. The devtools provide a dropdown menu to switch between environments (`prod`, `prod-preview`, `staging`, `staging-preview`, `dev`, and `dev-preview`) and automatically update the necessary preferences for each environment.

To easily change between these environments, the [remote-settings-devtools](https://github.com/mozilla-extensions/remote-settings-devtools) extension is available. Once installed, you can click on the extensions list in the browser, select `remote-settings-devtools` to interact with the devtools UI. The devtools allows you to switch between environments on your profile through the drop-down menu and will take care of correctly flipping all the required prefs. Specifically, it will allow you to point to `prod`, `prod-preview`, `staging`, `staging-preview`, `dev` and `dev-preview`.

Alternatively, it is possible to switch between environments by updating the following prefs in `about:config`:
* `services.settings.default_bucket`: `main` or `main-preview`
* `services.settings.server`: the applicable environment URL

For release and ESR, for security reasons, you will also need to run the application through the command line with `MOZ_REMOTE_SETTINGS_DEVTOOLS=1` environment variable for the preferences to be taken into account. Note that toggling the preference won’t have any effect until restart.

## Add Message using Remote Settings Admin UI

1. **Log in to the Remote Settings Admin UI**
   - Use your LDAP identity to log in and start testing in [STAGING](https://remote-settings.allizom.org/v1/admin/#/).

2. **Add Messaging System JSON**
   - On the left-hand side, under the **Workspace** bucket, click on the `cfr` collection.
   - Select `Create record` to open a text field.
   - Paste valid [Messaging System JSON](https://firefox-source-docs.mozilla.org/toolkit/components/messaging-system/docs/index.html) into the text field.
   - Ensure the following fields are accurate:
     - `id`: Unique identifier for the message.
     - `last_modified`: Current date and time in milliseconds.
   - These fields will be visible in the **Records** column.

3. **Preview testing**
   - Follow the [Multi Signoff Workflow](https://remote-settings.readthedocs.io/en/latest/tutorial-multi-signoff.html), add the message into preview by putting into review, it will be served in the `main-preview` bucket.
   - Install the [Remote Settings DevTools](https://github.com/mozilla-extensions/remote-settings-devtools).
   - Use the dev tools to point to `staging-preview`.
   - In `about:config`, enable the following preference:
     ```
     browser.newtabpage.activity-stream.asrouter.devtoolsEnabled
     ```
   - Go to `about:asrouter`.
   - Under the **Message** tab:
     - Filter the **Providers** for `cfr` to view the message in ASRouter.

4. **Main testing**
   - If the message looks good, switch to the **Review** tab in Remote Settings.
   - Have a peer review the changes.
   - Once the review has been approved, we can test the message again, pointing to `staging` instead of `staging-preview`.

5. **Deploy to PROD**
   - Follow the same steps for the [PROD Environment](https://remote-settings.mozilla.org/v1/admin/#/).
   - Use the `prod-preview` and `prod` buckets for final testing and deployment.

## Remote l10n
By default, all CFR messages are localized with the remote Fluent files hosted in `ms-language-packs` collection on Remote Settings using the [ms-language-packs script](https://github.com/mozilla-services/ms-language-packs?tab=readme-ov-file#messaging-system-language-packs-for-remote-settings).

We can check which components of the messages are hooked up to RemoteL10n with the following [Searchfox regex query](https://searchfox.org/mozilla-central/search?q=lazy%5C.RemoteL10n%5C.%28formatLocalizableText%7CcreateElement%29&path=&case=false&regexp=true). For example Infobar buttons are currently not Remotel10n configurable, see [Bug 1933819](https://bugzilla.mozilla.org/show_bug.cgi?id=1933819).

For local testing and development, we can force ASRouter to use the local Fluent files by flipping the pref `browser.newtabpage.activity-stream.asrouter.useRemoteL10n` in `about:config`. A note that [RemoteL10n uses the `main` bucket](https://searchfox.org/mozilla-central/source/browser/components/asrouter/modules/ASRouter.sys.mjs#297) so we cannot test using `main-preview`.

## The following are steps on how to authenticate and add messages manually

**1. Obtain your Bearer Token**
This can be done through browser-based authentication or through the Admin portal.

### Browser-based authentication
1. Ensure you have [kinto-http](https://pypi.org/project/kinto-http/) python library installed
2. In your terminal, run `python` with the following:
```python
>>> import kinto_http
>>> c = kinto_http.Client(server_url='https://remote-settings-dev.allizom.org/v1', auth=kinto_http.BrowserOAuth())
>>> c.server_info()["user"]
```
3. This will open the browser with a login successful page with your credentials in the terminal

### Admin portal
1. [Login on the Admin UI](https://remote-settings-dev.allizom.org/v1/admin/) using your LDAP identity
2. Copy the authentication header (📋 icon in the top bar)
3. Test your credentials with ``curl``. When reaching out the server root URL with this bearer token you should see a ``user`` entry whose ``id`` field is ``ldap:<you>@mozilla.com``.

```bash
SERVER=https://settings.dev.mozaws.net/v1
BEARER_TOKEN="Bearer uLdb-Yafefe....2Hyl5_w"

curl -s ${SERVER}/ -H "Authorization:${BEARER_TOKEN}" | jq .user
```

**2. Create/Update/Delete CFR entries**

In following example, we will create a new entry using the REST API (reusing `SERVER` and `BEARER_TOKEN` from previous step).

```bash
CID=cfr

# post a message
curl -X POST ${SERVER}/buckets/main-workspace/collections/${CID}/records \
     -d '{"data":{"id":"PIN_TAB","template":"cfr_doorhanger","content":{"category":"cfrFeatures","bucket_id":"CFR_PIN_TAB","notification_text":{"string_id":"cfr-doorhanger-extension-notification"},"heading_text":{"string_id":"cfr-doorhanger-pintab-heading"},"info_icon":{"label":{"string_id":"cfr-doorhanger-extension-sumo-link"},"sumo_path":"extensionrecommendations"},"text":{"string_id":"cfr-doorhanger-pintab-description"},"descriptionDetails":{"steps":[{"string_id":"cfr-doorhanger-pintab-step1"},{"string_id":"cfr-doorhanger-pintab-step2"},{"string_id":"cfr-doorhanger-pintab-step3"}]},"buttons":{"primary":{"label":{"string_id":"cfr-doorhanger-pintab-ok-button"},"action":{"type":"PIN_CURRENT_TAB"}},"secondary":[{"label":{"string_id":"cfr-doorhanger-extension-cancel-button"},"action":{"type":"CANCEL"}},{"label":{"string_id":"cfr-doorhanger-extension-never-show-recommendation"}},{"label":{"string_id":"cfr-doorhanger-extension-manage-settings-button"},"action":{"type":"OPEN_PREFERENCES_PAGE","data":{"category":"general-cfrfeatures"}}}]}},"targeting":"locale == \"en-US\" && !hasPinnedTabs && recentVisits[.timestamp > (currentDate|date - 3600 * 1000 * 1)]|length >= 3","frequency":{"lifetime":3},"trigger":{"id":"frequentVisits","params":["docs.google.com","www.docs.google.com","calendar.google.com","messenger.com","www.messenger.com","web.whatsapp.com","mail.google.com","outlook.live.com","facebook.com","www.facebook.com","twitter.com","www.twitter.com","reddit.com","www.reddit.com","github.com","www.github.com","youtube.com","www.youtube.com","feedly.com","www.feedly.com","drive.google.com","amazon.com","www.amazon.com","messages.android.com"]}}}' \
     -H 'Content-Type:application/json' \
     -H "Authorization:${BEARER_TOKEN}"
```

The collection was modified and now with pending changes in the workspace. We will now request a review, so that the changes become visible in the **preview** bucket.

```bash
# request review
curl -X PATCH ${SERVER}/buckets/main-workspace/collections/${CID} \
     -H 'Content-Type:application/json' \
     -d '{"data": {"status": "to-review"}}' \
     -H "Authorization:${BEARER_TOKEN}"
```

Now this new record should be listed here: https://settings.dev.mozaws.net/v1/buckets/main-preview/collections/cfr/records

**3. Set Remote Settings prefs to use the dev server.**

Until [support for the DEV environment](https://github.com/mozilla-extensions/remote-settings-devtools/issues/66) is added to the [Remote Settings dev tools](https://github.com/mozilla-extensions/remote-settings-devtools/), we'll change the preferences manually.

> These are critical preferences, you should use a dedicated Firefox profile for development.

```javascript
  Services.prefs.setCharPref("services.settings.loglevel", "debug");
  Services.prefs.setCharPref("services.settings.server", "https://settings.dev.mozaws.net/v1");
  // Pull data from the preview bucket.
  RemoteSettings.enablePreviewMode(true);
```

**3. Set ASRouter CFR pref to use Remote Settings provider and enable asrouter devtools.**

```javascript
Services.prefs.setStringPref("browser.newtabpage.activity-stream.asrouter.providers.cfr", JSON.stringify({"id":"cfr-remote","enabled":true,"type":"remote-settings","collection":"cfr"}));
Services.prefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.devtoolsEnabled", true);
```

**4. Go to `about:asrouter`**
There should be a "cfr-remote" provider listed.
