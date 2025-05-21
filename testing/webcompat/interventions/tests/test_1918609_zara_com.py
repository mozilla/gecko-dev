import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.zara.com/us/"

MENU_BUTTON_CSS = "[data-qa-id=layout-header-toggle-menu]"
CLOSE_COUNTRY_DIALOG_CSS = ".zds-dialog-close-button"
BACKDROP_CSS = ".zds-backdrop"
MENU_CONTAINER_CSS = ".layout-menu-std__content"


async def does_menu_appear(client):
    await client.navigate(URL)

    try:
        client.await_css(CLOSE_COUNTRY_DIALOG_CSS, is_displayed=True, timeout=4).click()
    except NoSuchElementException:
        pass

    client.await_css(MENU_BUTTON_CSS, is_displayed=True).click()

    try:
        client.await_css(MENU_CONTAINER_CSS, is_displayed=True, timeout=4)
        return True
    except NoSuchElementException:
        return False


@pytest.mark.skip_platforms("android")
@pytest.mark.only_channels("nightly")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await does_menu_appear(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.only_channels("nightly")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    # our version number needs to be high enough for the site to fail, which is not
    # always the case by the site's logic, so we artifically bump it here.
    with client.using_context("chrome"):
        client.execute_script(
            """
          const version = parseInt(navigator.userAgent.match("Firefox/([0-9]*)")[1]);
          const versionBumpedUA = navigator.userAgent.replaceAll(version, version + 2);
          Services.prefs.setStringPref("general.useragent.override", versionBumpedUA);
      """
        )
    assert not await does_menu_appear(client)
