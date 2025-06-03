import pytest

URL = "https://jobs.jobvite.com/careers/confluent/jobs"
CATEGORY_CSS = "a.jv-job-category-link"
FIRST_JOB_CSS = "a.jv-list-link"
APPLY_CSS = "a.jv-button-apply"
SELECT_CSS = "select#jv-country-select"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.disable_window_alert()  # possible "Cannot contact reCAPTCHA" alert
        await client.navigate(URL)
        client.await_css(CATEGORY_CSS, is_displayed=True).click()
        client.await_css(FIRST_JOB_CSS, is_displayed=True).click()
        client.await_css(APPLY_CSS, is_displayed=True).click()
        return client.test_for_fastclick(client.await_css(SELECT_CSS))


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_fastclick_active(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_fastclick_active(client)
