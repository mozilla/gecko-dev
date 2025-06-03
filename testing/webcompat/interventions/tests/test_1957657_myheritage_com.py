import asyncio

import pytest

URL = "https://www.myheritage.com/complete-genealogy-package?keyword=partners"

COOKIE_BANNER_CSS = "#cookie_preferences_banner_root"
START_FREE_TRIAL_TEXT = "Start free trial"
EMAIL_CSS = "#miniSignupEmail"


async def can_start_free_trial(client):
    await client.navigate(URL)
    client.hide_elements(COOKIE_BANNER_CSS)
    client.await_text(START_FREE_TRIAL_TEXT, is_displayed=True).click()
    email = client.await_css(EMAIL_CSS, is_displayed=True)
    email.click()
    await asyncio.sleep(2)
    return client.execute_script(
        """
      return document.activeElement === arguments[0];
    """,
        email,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_start_free_trial(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_start_free_trial(client)
