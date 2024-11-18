import base64
import os

from support.context import using_context


def get_base64_for_addon_file(filename):
    with open(
        os.path.join(
            os.path.abspath(os.path.dirname(__file__)), "webextensions", filename
        ),
        "rb",
    ) as file:
        return base64.b64encode(file.read()).decode("utf-8")


def get_ids_for_installed_addons(session):
    with using_context(session, "chrome"):
        return session.execute_async_script(
            """
              const [resolve] = arguments;
              const { AddonManager } = ChromeUtils.importESModule(
                "resource://gre/modules/AddonManager.sys.mjs"
              );

              async function getAllAddons() {
                const addons = await AddonManager.getAllAddons();
                const ids = addons.map(x => x.id);
                resolve(ids);
              }

              getAllAddons();
            """,
        )


def is_addon_temporary_installed(session, addon_id):
    with using_context(session, "chrome"):
        return session.execute_async_script(
            """
              const [addon_id, resolve] = arguments;
              const { AddonManager } = ChromeUtils.importESModule(
                "resource://gre/modules/AddonManager.sys.mjs"
              );

              async function getAddonTemporaryInstalledField() {
                const addons = await AddonManager.getAllAddons();
                const addon = addons.find(x => x.id == addon_id);
                if (addon) {
                    resolve(addon.temporarilyInstalled);
                } else {
                    throw new Error(`Add-on with ID ${addon_id} doesn't exist`)
                }
              }

              return getAddonTemporaryInstalledField();
            """,
            args=[addon_id],
        )


def is_addon_private_browsing_allowed(session, addon_id):
    with using_context(session, "chrome"):
        return session.execute_async_script(
            """
              const [addon_id, resolve] = arguments;

              function getPrivateBrowsingAllowedAddonPolicyField() {
                const policy = WebExtensionPolicy.getByID(addon_id);
                if (policy) {
                  resolve(policy.privateBrowsingAllowed);
                } else {
                  throw new Error(`Policy of add-on with ID ${addon_id} doesn't exist`)
                }
              }

              return getPrivateBrowsingAllowedAddonPolicyField();
            """,
            args=[addon_id],
        )
