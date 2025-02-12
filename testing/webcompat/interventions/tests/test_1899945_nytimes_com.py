import pytest

# the page is currently down, but we want to monitor it to see if it reverts.
URL = "https://www.nytimes.com/interactive/projects/modern-love/36-questions/?ref=redirector"
BROKEN_SCRIPT_URL = "https://int.nyt.com/applications/modernlove-questions/js/main-bb7fdbbd64befdd6571c1388925b2d06fd527a05.js"
ERROR_MSG = (
    """can't access property "addEventListener", window.applicationCache is undefined"""
)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
    await client.navigate(BROKEN_SCRIPT_URL)
    assert client.await_text("AccessDenied")
